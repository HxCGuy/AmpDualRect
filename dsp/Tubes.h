// Tubes.h - Koren-style vacuum tube models with analytic derivatives (for Newton solvers).
//   Triode:  12AX7 plate current Ip(Vgk, Vpk) and grid current Ig(Vgk)
//   Pentode: 6L6GC / EL34 plate + screen currents Ip(Vg1k, Vg2k, Vpk), Ig2, and control-grid current Ig1(Vg1k)
// All voltages in volts, all currents in amperes.
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace drect {

// ---- fast transcendental functions (relative error ~1e-9, plenty for audio and Newton) ----
inline double fastExp2 (double x)
{
    x = std::clamp (x, -1020.0, 1020.0);
    const double nr = std::nearbyint (x);
    const int n = (int) nr;
    const double t = (x - nr) * 0.6931471805599453;           // |t| <= ln2/2
    // e^t: Taylor series to t^7 (relative error < 6e-9)
    const double p = 1.0 + t * (1.0 + t * (0.5 + t * (1.0 / 6.0 + t * (1.0 / 24.0 + t * (1.0 / 120.0 + t * (1.0 / 720.0 + t * (1.0 / 5040.0)))))));
    const std::uint64_t bits = (std::uint64_t) (n + 1023) << 52;
    double sc; std::memcpy (&sc, &bits, sizeof (sc));
    return p * sc;
}
inline double fastExp (double x) { return fastExp2 (x * 1.4426950408889634); }
inline double fastLog (double x)       // x > 0, normal
{
    std::uint64_t b; std::memcpy (&b, &x, sizeof (b));
    int e = (int) ((b >> 52) & 0x7ff) - 1023;
    b = (b & 0x000fffffffffffffULL) | 0x3ff0000000000000ULL;
    double m; std::memcpy (&m, &b, sizeof (m));                  // [1, 2)
    if (m > 1.4142135623730951) { m *= 0.5; ++e; }
    const double z = (m - 1.0) / (m + 1.0), z2 = z * z;           // |z| < 0.172
    const double s = z * (2.0 + z2 * (2.0 / 3.0 + z2 * (2.0 / 5.0 + z2 * (2.0 / 7.0 + z2 * (2.0 / 9.0 + z2 * (2.0 / 11.0 + z2 * (2.0 / 13.0)))))));
    return s + (double) e * 0.6931471805599453;
}
inline double fastPow (double a, double b) { return a > 0.0 ? fastExp (b * fastLog (a)) : 0.0; }
inline double softplus (double x)  { return x > 30.0 ? x : (x < -30.0 ? fastExp (x) : fastLog (1.0 + fastExp (x))); }
inline double sigmoid (double x)   { if (x >= 0.0) return 1.0 / (1.0 + fastExp (-x)); const double e = fastExp (x); return e / (1.0 + e); }

// smooth grid-conduction law: I = G * s^1.5, s = vt * softplus(v / vt)
struct GridDiode
{
    double G = 4.0e-4, vt = 0.08;
    inline void eval (double v, double& i, double& di) const
    {
        if (v < -25.0 * vt) { i = 1.0e-12 * v; di = 1.0e-12; return; }     // not conducting
        const double s = vt * softplus (v / vt);
        const double rs = std::sqrt (s);
        i  = G * s * rs + 1.0e-12 * v;
        di = 1.5 * G * rs * sigmoid (v / vt) + 1.0e-12;
    }
};

struct TriodeModel
{
    double mu = 100.0, ex = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0;
    GridDiode grid;

    static TriodeModel ax7() { return {}; }

    // plate current and its partial derivatives
    inline void plate (double vgk, double vpk, double& ip, double& dvg, double& dvp) const
    {
        if (vpk <= 0.0) { ip = 0.0; dvg = 0.0; dvp = 1.0e-9; return; }
        const double r = std::sqrt (kvb + vpk * vpk);
        const double a = kp * (1.0 / mu + vgk / r);
        double sp, sg;
        if (a > 30.0) { sp = a; sg = 1.0; }
        else if (a < -30.0) { sp = sg = fastExp (a); }
        else { const double e = fastExp (a); sp = fastLog (1.0 + e); sg = e / (1.0 + e); }
        const double e1 = vpk / kp * sp;
        // deep cut-off: keep the same tiny leakage as the conducting branch below, otherwise the current
        // steps by 1e-9 * vpk at the switch-over and Newton can cycle between the two branches
        if (e1 <= 1.0e-12) { ip = 1.0e-9 * vpk; dvg = 0.0; dvp = 1.0e-9; return; }
        const double pe = fastPow (e1, ex - 1.0);
        ip = 2.0 * pe * e1 / kg1;
        const double dIdE = 2.0 * ex * pe / kg1;
        const double dEdVg = vpk * sg / r;
        const double dEdVp = sp / kp - vpk * vpk * vgk * sg / (r * r * r);
        dvg = dIdE * dEdVg;
        dvp = dIdE * dEdVp + 1.0e-9;
        ip += 1.0e-9 * vpk;          // tiny leakage keeps the Jacobian well conditioned at cut-off
    }
};

struct PentodeModel
{
    double mu, ex, kg1, kg2, kp, kvb;
    double idleCathodeCurrent;     // per tube, what a technician would bias it to in this amp
    GridDiode grid { 2.0e-3, 0.1 };

    static PentodeModel sixL6() { return { 8.7, 1.35, 1460.0, 4500.0, 48.0, 12.0, 0.030 }; }
    static PentodeModel el34()  { return { 11.0, 1.35, 650.0, 4200.0, 60.0, 24.0, 0.034 }; }

    // returns plate current; dIp/dVpk in dvp; screen current in ig2 (screen dependency on Vpk included:
    // when the plate swings into the knee, cathode current is diverted to the screen)
    inline double eval (double vg1, double vg2, double vpk, double& dvp, double& ig2) const
    {
        const double v2 = std::max (vg2, 1.0);
        const double a = kp * (1.0 / mu + vg1 / v2);
        const double e1 = v2 / kp * softplus (a);
        const double ik = e1 > 1.0e-12 ? 2.0 * fastPow (e1, ex) / kg1 : 0.0;      // "cathode" current scale
        const double vp = std::max (vpk, 0.0);
        const double at = std::atan (vp / kvb);
        const double kn = at * (2.0 / 3.141592653589793);                         // 0..1 knee factor
        const double ip = ik * at;                                                 // Koren: E1^ex/kg1 * (1+sgn) * atan(Vpk/kvb)
        dvp = vpk > 0.0 ? ik * (kvb / (kvb * kvb + vp * vp)) : 0.0;
        dvp += 1.0e-9;
        const double base = v2 / mu + vg1;
        ig2 = (base > 0.0 ? fastPow (base, ex) / kg2 : 0.0) + 0.45 * ik * (1.0 - kn);
        return ip + 1.0e-9 * vpk;
    }
    // split evaluation for the output-stage solver: the part that depends on the grids only...
    inline double cathodeScale (double vg1, double vg2) const
    {
        const double v2 = std::max (vg2, 1.0);
        const double e1 = v2 / kp * softplus (kp * (1.0 / mu + vg1 / v2));
        return e1 > 1.0e-12 ? 2.0 * fastPow (e1, ex) / kg1 : 0.0;
    }
    inline double cathodeScaleD (double vg1, double vg2, double& dIk) const
    {
        const double v2 = std::max (vg2, 1.0);
        const double a = kp * (1.0 / mu + vg1 / v2);
        const double e1 = v2 / kp * softplus (a);
        if (e1 <= 1.0e-12) { dIk = 0.0; return 0.0; }
        const double pe = fastPow (e1, ex - 1.0);
        dIk = 2.0 * ex * pe / kg1 * sigmoid (a);
        return 2.0 * pe * e1 / kg1;
    }
    // ...and the plate-voltage dependent part (cheap, used inside the Newton loop)
    inline double plateFrom (double ik, double vpk, double& dvp) const
    {
        const double vp = std::max (vpk, 0.0);
        dvp = (vpk > 0.0 ? ik * (kvb / (kvb * kvb + vp * vp)) : 0.0) + 1.0e-9;
        return ik * std::atan (vp / kvb) + 1.0e-9 * vpk;
    }
    inline double screenFrom (double ik, double vg1, double vg2, double vpk) const
    {
        const double v2 = std::max (vg2, 1.0);
        const double kn = std::atan (std::max (vpk, 0.0) / kvb) * (2.0 / 3.141592653589793);
        const double base = v2 / mu + vg1;
        return (base > 0.0 ? fastPow (base, ex) / kg2 : 0.0) + 0.45 * ik * (1.0 - kn);
    }
    inline double cathodeCurrent (double vg1, double vg2, double vpk) const
    {
        double d, ig2; const double ip = eval (vg1, vg2, vpk, d, ig2);
        return ip + ig2;
    }
};

} // namespace drect
