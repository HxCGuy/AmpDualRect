// DKNet.h - a small nodal-analysis circuit engine (Discrete K-method). (Shared with Amp5150 III.)
//
// A netlist of resistors, capacitors, ideal voltage sources (to ground), triodes and grid diodes is
// compiled into a discrete-time state-space system (trapezoidal rule). Capacitors become states,
// tubes become nonlinear "ports" that are solved per sample with Newton's method:
//
//      vp = p0 - K i(vp)                 (port voltages, p0 from inputs + states)
//      vc = Cu u + Cs s - Ci i           (capacitor voltages)
//      y  = Yu u + Ys s - Yi i           (outputs)
//      s' = 2 g vc - s                   (trapezoidal companion state update)
//
// Resistances can be changed at run time (pots, relays); update() then recomputes the matrices
// without allocating, so it is safe on the audio thread. All allocation happens in prepare().
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "Tubes.h"

namespace drect {

class DKNet
{
public:
    static constexpr int gnd = 0;
    // fixed upper bounds for the per-sample matrices (keeps the inner loops cache friendly)
    // (MM = 24: the last preamp net holds V3A, the Mr. Scary stage, the cathode follower and both
    // tone stacks with their master / presence networks - 19 capacitors)
    static constexpr int MP = 8, MS = 6, MM = 24, MY = 8;

    // ------------------------------------------------------------------ netlist (setup time)
    int node()                          { isSrc.push_back (-1); return (int) isSrc.size() - 1; }
    int source()                        { isSrc.push_back (numSrc++); return (int) isSrc.size() - 1; }
    int resistor (int a, int b, double r) { res.push_back ({ a, b, r }); return (int) res.size() - 1; }
    void capacitor (int a, int b, double c) { caps.push_back ({ a, b, c }); }
    int triode (int g, int p, int k, const TriodeModel* m)
    {
        devs.push_back ({ 0, (int) ports.size(), m, nullptr });
        ports.push_back ({ g, k }); ports.push_back ({ p, k });
        return (int) devs.size() - 1;
    }
    int diode (int a, int b, const GridDiode* m)
    {
        devs.push_back ({ 1, (int) ports.size(), nullptr, m });
        ports.push_back ({ a, b });
        return (int) devs.size() - 1;
    }
    int output (int a, int b = gnd)     { outs.push_back ({ a, b }); return (int) outs.size() - 1; }
    // a potentiometer: top - wiper - bottom. Returns the id of the top half; bottom half is id + 1.
    int pot (int top, int wiper, int bottom, double r)
    {
        const int id = resistor (top, wiper, r * 0.5);
        resistor (wiper, bottom, r * 0.5);
        return id;
    }

    // ------------------------------------------------------------------ run time
    void setResistance (int id, double r)
    {
        r = std::max (r, 1.0);
        if (res[(size_t) id].r != r) { res[(size_t) id].r = r; dirty = true; }
    }
    // f = fraction of the track between wiper and bottom (0 = wiper at bottom)
    void setPot (int id, double total, double f)
    {
        f = std::clamp (f, 0.0, 1.0);
        setResistance (id, total * (1.0 - f));
        setResistance (id + 1, total * f);
    }

    bool isValid() const   { return valid; }
    int numInputs() const  { return numSrc; }
    int numOutputs() const { return (int) outs.size(); }

    void prepare (double sampleRate)
    {
        T = 1.0 / sampleRate;
        nn = (int) isSrc.size();
        uidx.assign ((size_t) nn, -1);
        nu = 0;
        for (int n = 1; n < nn; ++n) if (isSrc[(size_t) n] < 0) uidx[(size_t) n] = nu++;
        ns = numSrc; nm = (int) caps.size(); np = (int) ports.size(); ny = (int) outs.size();
        if (ns > MS || nm > MM || np > MP || ny > MY) { valid = false; return; }
        valid = true;

        auto z = [] (std::vector<double>& v, int n) { v.assign ((size_t) std::max (n, 1), 0.0); };
        z (G, nu * nu); z (S, nu * nu); z (Bs, nu * ns);
        z (SBs, nu * ns); z (SBc, nu * nm); z (SNt, nu * np);
        z (work, nu * 2 * nu);
        for (int k = 0; k < nm; ++k) { g[k] = 2.0 * caps[(size_t) k].c / T; s[k] = 0.0; }
        for (auto& d : devs)
        {
            if (d.type == 0) { vp[d.port] = -1.5; vp[d.port + 1] = 150.0; }
            else vp[d.port] = -2.0;
        }
        for (int i = 0; i < np; ++i) vpPrev[i] = vp[i];
        dirty = true;
        compile (false);
    }

    void update() { if (dirty) compile (false); }

    // Puts the circuit at its DC operating point for constant inputs u (caps open, then states set).
    void solveDC (const double* u)
    {
        compile (true);
        for (int i = 0; i < np; ++i)
        {
            double a = 0; for (int j = 0; j < ns; ++j) a += Pu[i][j] * u[j];
            p0[i] = a;
        }
        newton (300, 5.0);
        double vcDC[MM];
        for (int k = 0; k < nm; ++k)
        {
            double a = 0;
            for (int j = 0; j < ns; ++j) a += Cu[k][j] * u[j];
            for (int q = 0; q < np; ++q) a -= Ci[k][q] * cur[q];
            vcDC[k] = a;
        }
        compile (false);
        for (int k = 0; k < nm; ++k) s[k] = g[k] * vcDC[k];
        for (int i = 0; i < np; ++i) vpPrev[i] = vp[i];
    }

    // one sample: u = inputs (numInputs), y = outputs (numOutputs)
    inline void process (const double* u, double* y) { solve (u, y); commit (u); }

    // solves the nonlinear ports and outputs for inputs u, without advancing the state
    inline void solve (const double* u, double* y, bool extrapolate = true)
    {
        for (int i = 0; i < np; ++i)
        {
            double a = 0;
            for (int j = 0; j < ns; ++j) a += Pu[i][j] * u[j];
            for (int k = 0; k < nm; ++k) a += Ps[i][k] * s[k];
            p0[i] = a;
            // start Newton from a linear extrapolation of the last two solutions
            if (extrapolate) { const double v = vp[i]; vp[i] = 2.0 * v - vpPrev[i]; vpPrev[i] = v; }
        }
        if (np > 0)
        {
            newton (16, 200.0);
            if (! lastConverged)
            {
                // fallback (rare): restart from the last solution with limited steps. Needed where two
                // nonlinear knees sit close together (e.g. a cathode-follower grid behind a grid
                // stopper, driven by a stage inside a feedback loop): full Newton steps can cycle there.
                ++fallbacks;
                if (extrapolate) for (int i = 0; i < np; ++i) vp[i] = vpPrev[i];
                newton (120, 4.0);
            }
        }
        for (int r = 0; r < ny; ++r)
        {
            double a = 0;
            for (int j = 0; j < ns; ++j) a += Yu[r][j] * u[j];
            for (int k = 0; k < nm; ++k) a += Ys[r][k] * s[k];
            for (int q = 0; q < np; ++q) a -= Yi[r][q] * cur[q];
            y[r] = a;
        }
    }

    // advances the state using the last solve() (u must be the same inputs)
    inline void commit (const double* u)
    {
        double vc[MM];
        for (int k = 0; k < nm; ++k)
        {
            double a = 0;
            for (int j = 0; j < ns; ++j) a += Cu[k][j] * u[j];
            for (int c = 0; c < nm; ++c) a += Cs[k][c] * s[c];
            for (int q = 0; q < np; ++q) a -= Ci[k][q] * cur[q];
            vc[k] = a;
        }
        for (int k = 0; k < nm; ++k) s[k] = 2.0 * g[k] * vc[k] - s[k];
    }

    // small-signal response to input "in" at the last solution: fills dy[out] = d y / d u[in]
    void sensitivity (int in, double* dy)
    {
        double J[MP][MP];
        for (int r = 0; r < np; ++r) { for (int c = 0; c < np; ++c) J[r][c] = r == c ? 1.0 : 0.0; sx[r] = Pu[r][in]; }
        for (const auto& d : devs)
        {
            const int p = d.port;
            if (d.type == 0)
                for (int r = 0; r < np; ++r) { J[r][p] += K[r][p] * jA[p] + K[r][p + 1] * jB[p]; J[r][p + 1] += K[r][p + 1] * jC[p]; }
            else
                for (int r = 0; r < np; ++r) J[r][p] += K[r][p] * jA[p];
        }
        gaussSolve (J, sx);
        for (const auto& d : devs)
        {
            const int p = d.port;
            if (d.type == 0) { sdi[p] = jA[p] * sx[p]; sdi[p + 1] = jB[p] * sx[p] + jC[p] * sx[p + 1]; }
            else sdi[p] = jA[p] * sx[p];
        }
        sIn = in;
        for (int r = 0; r < ny; ++r)
        {
            double a = Yu[r][in];
            for (int q = 0; q < np; ++q) a -= Yi[r][q] * sdi[q];
            sdy[r] = dy[r] = a;
        }
    }
    // first-order update of the last solution after input "in" (the one passed to sensitivity) changed by delta
    void applyInputStep (double delta, double* y)
    {
        for (int q = 0; q < np; ++q) { vp[q] += sx[q] * delta; cur[q] += sdi[q] * delta; }
        for (int r = 0; r < ny; ++r) y[r] += sdy[r] * delta;
    }

    double portVoltage (int p) const { return vp[p]; }
    int lastIterations = 0;
    bool lastConverged = true;
    long fallbacks = 0;          // how often the damped fallback solve was needed (diagnostics)
#ifdef DKNET_DEBUG
    double dbgErr[160] {}; double dbgVp[160][8] {};
#endif
    double tol = 1e-3;           // Newton step (volts) at which the solution is accepted; with the
                                 // first-order current update the remaining error is ~tol^2 (checked:
                                 // output identical to tol 1e-9 within -100 dB)

private:
    struct R { int a, b; double r; };
    struct C { int a, b; double c; };
    struct P { int a, b; };
    struct D { int type, port; const TriodeModel* tri; const GridDiode* dio; };

    std::vector<int> isSrc { -1 };      // node 0 = ground
    int numSrc = 0;
    std::vector<R> res; std::vector<C> caps; std::vector<P> ports, outs; std::vector<D> devs;

    double T = 1.0 / 96000.0;
    int nn = 0, nu = 0, ns = 0, nm = 0, np = 0, ny = 0;
    bool dirty = true, valid = false;
    std::vector<int> uidx;
    std::vector<double> G, S, Bs, SBs, SBc, SNt, work;          // setup / update only

    // per-sample matrices and state
    double Pu[MP][MS] {}, Ps[MP][MM] {}, K[MP][MP] {};
    double Cu[MM][MS] {}, Cs[MM][MM] {}, Ci[MM][MP] {};
    double Yu[MY][MS] {}, Ys[MY][MM] {}, Yi[MY][MP] {};
    double g[MM] {}, s[MM] {};
    double vp[MP] {}, vpPrev[MP] {}, cur[MP] {}, p0[MP] {};
    double jA[MP] {}, jB[MP] {}, jC[MP] {};
    double sx[MP] {}, sdi[MP] {}, sdy[MY] {}; int sIn = 0;   // device derivatives (triode: dIg/dVg, dIp/dVg, dIp/dVp; diode: dI/dV in jA)

    static inline size_t idx (int r, int c, int cols) { return (size_t) r * (size_t) cols + (size_t) c; }

    void stamp (int a, int b, double cond)
    {
        const int ua = a > 0 ? uidx[(size_t) a] : -1, ub = b > 0 ? uidx[(size_t) b] : -1;
        const int sa = a > 0 ? isSrc[(size_t) a] : -1, sb = b > 0 ? isSrc[(size_t) b] : -1;
        if (ua >= 0) G[idx (ua, ua, nu)] += cond;
        if (ub >= 0) G[idx (ub, ub, nu)] += cond;
        if (ua >= 0 && ub >= 0) { G[idx (ua, ub, nu)] -= cond; G[idx (ub, ua, nu)] -= cond; }
        if (ua >= 0 && sb >= 0) Bs[idx (ua, sb, ns)] += cond;
        if (ub >= 0 && sa >= 0) Bs[idx (ub, sa, ns)] += cond;
    }

    bool invert()
    {
        const int w = 2 * nu;
        for (int r = 0; r < nu; ++r)
            for (int c = 0; c < w; ++c)
                work[idx (r, c, w)] = c < nu ? G[idx (r, c, nu)] : (c - nu == r ? 1.0 : 0.0);
        for (int c = 0; c < nu; ++c)
        {
            int piv = c; double best = std::fabs (work[idx (c, c, w)]);
            for (int r = c + 1; r < nu; ++r) { const double v = std::fabs (work[idx (r, c, w)]); if (v > best) { best = v; piv = r; } }
            if (best < 1e-300) return false;
            if (piv != c) for (int k = 0; k < w; ++k) std::swap (work[idx (c, k, w)], work[idx (piv, k, w)]);
            const double inv = 1.0 / work[idx (c, c, w)];
            for (int k = 0; k < w; ++k) work[idx (c, k, w)] *= inv;
            for (int r = 0; r < nu; ++r)
            {
                if (r == c) continue;
                const double f = work[idx (r, c, w)];
                if (f == 0.0) continue;
                for (int k = 0; k < w; ++k) work[idx (r, k, w)] -= f * work[idx (c, k, w)];
            }
        }
        for (int r = 0; r < nu; ++r)
            for (int c = 0; c < nu; ++c) S[idx (r, c, nu)] = work[idx (r, nu + c, w)];
        return true;
    }

    inline double rowVal (const std::vector<double>& X, int node, int c, int cols) const
    {
        if (node <= 0) return 0.0;
        const int u = uidx[(size_t) node];
        return u >= 0 ? X[idx (u, c, cols)] : 0.0;
    }
    inline double srcIs (int node, int j) const { return (node > 0 && isSrc[(size_t) node] == j) ? 1.0 : 0.0; }

    void compile (bool dc)
    {
        if (! valid) return;
        std::fill (G.begin(), G.end(), 0.0);
        std::fill (Bs.begin(), Bs.end(), 0.0);
        for (auto& r : res) stamp (r.a, r.b, 1.0 / r.r);
        if (! dc) for (int k = 0; k < nm; ++k) stamp (caps[(size_t) k].a, caps[(size_t) k].b, g[k]);
        for (int i = 0; i < nu; ++i) G[idx (i, i, nu)] += 1e-12;      // gmin
        invert();

        for (int r = 0; r < nu; ++r)
        {
            for (int j = 0; j < ns; ++j)
            {
                double a = 0; for (int k = 0; k < nu; ++k) a += S[idx (r, k, nu)] * Bs[idx (k, j, ns)];
                SBs[idx (r, j, ns)] = a;
            }
            for (int k = 0; k < nm; ++k)
            {
                const auto& c = caps[(size_t) k];
                double a = 0;
                if (c.a > 0 && uidx[(size_t) c.a] >= 0) a += S[idx (r, uidx[(size_t) c.a], nu)];
                if (c.b > 0 && uidx[(size_t) c.b] >= 0) a -= S[idx (r, uidx[(size_t) c.b], nu)];
                SBc[idx (r, k, nm)] = dc ? 0.0 : a;
            }
            for (int q = 0; q < np; ++q)
            {
                const auto& p = ports[(size_t) q];
                double a = 0;
                if (p.a > 0 && uidx[(size_t) p.a] >= 0) a += S[idx (r, uidx[(size_t) p.a], nu)];
                if (p.b > 0 && uidx[(size_t) p.b] >= 0) a -= S[idx (r, uidx[(size_t) p.b], nu)];
                SNt[idx (r, q, np)] = a;
            }
        }
        auto fill = [&] (int a, int b, double* xu, double* xs, double* xi)
        {
            for (int j = 0; j < ns; ++j) xu[j] = rowVal (SBs, a, j, ns) - rowVal (SBs, b, j, ns) + srcIs (a, j) - srcIs (b, j);
            for (int k = 0; k < nm; ++k) xs[k] = rowVal (SBc, a, k, nm) - rowVal (SBc, b, k, nm);
            for (int q = 0; q < np; ++q) xi[q] = rowVal (SNt, a, q, np) - rowVal (SNt, b, q, np);
        };
        for (int q = 0; q < np; ++q) fill (ports[(size_t) q].a, ports[(size_t) q].b, Pu[q], Ps[q], K[q]);
        for (int k = 0; k < nm; ++k) fill (caps[(size_t) k].a, caps[(size_t) k].b, Cu[k], Cs[k], Ci[k]);
        for (int r = 0; r < ny; ++r) fill (outs[(size_t) r].a, outs[(size_t) r].b, Yu[r], Ys[r], Yi[r]);
        dirty = false;
    }

    inline void evalDevices()
    {
        for (const auto& d : devs)
        {
            const int p = d.port;
            if (d.type == 0)
            {
                d.tri->grid.eval (vp[p], cur[p], jA[p]);
                d.tri->plate (vp[p], vp[p + 1], cur[p + 1], jB[p], jC[p]);
            }
            else d.dio->eval (vp[p], cur[p], jA[p]);
        }
    }

    void gaussSolve (double (&J)[MP][MP], double (&F)[MP]) const
    {
        for (int c = 0; c < np; ++c)
        {
            int piv = c; double best = std::fabs (J[c][c]);
            for (int r = c + 1; r < np; ++r) { const double v = std::fabs (J[r][c]); if (v > best) { best = v; piv = r; } }
            if (piv != c) { for (int k = 0; k < np; ++k) std::swap (J[c][k], J[piv][k]); std::swap (F[c], F[piv]); }
            const double inv = 1.0 / J[c][c];
            for (int r = c + 1; r < np; ++r)
            {
                const double f = J[r][c] * inv;
                if (f == 0.0) continue;
                for (int k = c + 1; k < np; ++k) J[r][k] -= f * J[c][k];
                F[r] -= f * F[c];
            }
        }
        for (int r = np - 1; r >= 0; --r)
        {
            double a = F[r];
            for (int k = r + 1; k < np; ++k) a -= J[r][k] * F[k];
            F[r] = a / J[r][r];
        }
    }

    // solves vp = p0 - K i(vp). Dispatches to a version compiled for the net's port count, so the
    // small dense loops are fully unrolled (the nets in the amps have 3 to 6 ports).
    void newton (int maxIt, double maxStep)
    {
        switch (np)
        {
            case 3:  newtonN<3> (maxIt, maxStep); break;
            case 4:  newtonN<4> (maxIt, maxStep); break;
            case 5:  newtonN<5> (maxIt, maxStep); break;
            case 6:  newtonN<6> (maxIt, maxStep); break;
            default: newtonN<0> (maxIt, maxStep); break;
        }
    }

    template <int N>
    void newtonN (int maxIt, double maxStep)
    {
        const int n = N > 0 ? N : np;
        lastConverged = false;
        int it = 0;
        double J[MP][MP], F[MP];
        for (; it < maxIt; ++it)
        {
            evalDevices();
            for (int r = 0; r < n; ++r)
            {
                double a = vp[r] - p0[r];
                for (int q = 0; q < n; ++q) a += K[r][q] * cur[q];
                F[r] = -a;
                for (int c = 0; c < n; ++c) J[r][c] = 0.0;
                J[r][r] = 1.0;
            }
            // J = I + K Jf, Jf block diagonal per device
            for (const auto& d : devs)
            {
                const int p = d.port;
                if (d.type == 0)
                    for (int r = 0; r < n; ++r)
                    {
                        J[r][p]     += K[r][p] * jA[p] + K[r][p + 1] * jB[p];
                        J[r][p + 1] += K[r][p + 1] * jC[p];
                    }
                else
                    for (int r = 0; r < n; ++r) J[r][p] += K[r][p] * jA[p];
            }
            // Gaussian elimination with partial pivoting
            for (int c = 0; c < n; ++c)
            {
                int pr = c; double best = std::fabs (J[c][c]);
                for (int r = c + 1; r < n; ++r) { const double v = std::fabs (J[r][c]); if (v > best) { best = v; pr = r; } }
                if (pr != c) { for (int k = 0; k < n; ++k) std::swap (J[c][k], J[pr][k]); std::swap (F[c], F[pr]); }
                const double inv = 1.0 / J[c][c];
                for (int r = c + 1; r < n; ++r)
                {
                    const double f = J[r][c] * inv;
                    for (int k = c + 1; k < n; ++k) J[r][k] -= f * J[c][k];
                    F[r] -= f * F[c];
                }
            }
            for (int r = n - 1; r >= 0; --r)
            {
                double a = F[r];
                for (int k = r + 1; k < n; ++k) a -= J[r][k] * F[k];
                F[r] = a / J[r][r];
            }
            double err = 0;
            for (int r = 0; r < n; ++r)
            {
                double dx = F[r];
                dx = std::clamp (dx, -maxStep, maxStep);
                if (! (dx == dx)) dx = 0.0;
                F[r] = dx;
                vp[r] += dx;
                err = std::max (err, std::fabs (dx));
            }
#ifdef DKNET_DEBUG
            if (it < 160) { dbgErr[it] = err; for (int r = 0; r < n; ++r) dbgVp[it][r] = vp[r]; }
#endif
            if (err < tol)
            {
                // converged: bring the currents to the final voltages to first order (no re-evaluation)
                for (const auto& d : devs)
                {
                    const int p = d.port;
                    if (d.type == 0) { cur[p] += jA[p] * F[p]; cur[p + 1] += jB[p] * F[p] + jC[p] * F[p + 1]; }
                    else cur[p] += jA[p] * F[p];
                }
                lastConverged = true; ++it; break;
            }
        }
        lastIterations = it;
        if (! lastConverged) evalDevices();
    }
};

} // namespace drect
