// DualRect.h - circuit-level model of a 100 W, two-channel (Orange / Red) "dual rectifier" tube head.
//
// Built from the hand-traced 2-channel "Dual Rectifier Solo Head" schematic (sheets mbdr1..mbdr7).
// Both channels share one tube path; the channel and mode switches only change LDRs (light-dependent
// resistors) that select the input pad, the gain pot, the cathode bypass caps, the tone stack, the
// master and the global feedback. The model does the same: one preamp chain whose LDRs are run-time
// resistors, switched from the schematic's truth table (sheet mbdr7).
//
//   V1A -> [Mr. Scary stage] -> pad network (680k||2n, 82p, 2M2; LDR4) -> ORANGE / RED gain pot
//       (1M, 1n bright; LDR1/2/5/6) -> 470k/20p -> V2A -> 20n -> 470k/1M -> V2B (39k "cold" clipper,
//       1n across its load) -> 20n -> 330k / 220k -> V3A -> V3B cathode follower
//       -> RED / ORANGE tone stacks (LDR8/9) -> RED presence | ORANGE roll-off + master bypass
//       -> RED / ORANGE master (LDR14/15/16) -> (no FX loop) 20n -> long-tail-pair PI (V5)
//       with global NFB (LDR19/20, ORANGE presence) -> 4 x 6L6GC or EL34 -> output transformer
//       -> reactive speaker load; silicon-diode or 5U4 tube rectifier, BOLD / SPONGY supply.
//       (Added, not on the trace: plate flyback diodes and the transformer's core loss.)
//
// Mods (not on the stock schematic):
//   TIGHT    V1A cathode bypass 1u -> 470n and V1A coupling cap 20n -> 2n2 (values from sheet mbdr1mod)
//   DE-FIZZ  a cap across V3A's 220k plate load and a 22k grid stopper on the V3B cathode follower
//            (the mbdr1mod sheet's change). The sheet's 220p took only ~1 dB off the 5-20 kHz band in
//            the model, so 470p is used.
//   MID SHIFT  both tone stacks' mid caps 20n -> 47n: the MID knob then reaches down into the
//            150-500 Hz region (stock it works mainly at 400 Hz - 1 kHz), so it can cut the low-mid build-up.
//   MR. SCARY  an extra 12AX7 gain stage right after V1A, in the spirit of the plug-in "Mr. Scary"
//            module (whose circuit is not published - this one is our own design): an inverting
//            stage with plate-to-grid feedback. GAIN sets the feedback resistor (470k + 10M audio
//            rheostat against a 470k input resistor, 33p across it so the extra gain stays in the
//            lows and mids): about unity just above 0, about +22 dB (lows) / +18 dB (1 kHz) / +5 dB
//            (5 kHz) fully CW, where it clips like an extra cascaded stage early in the chain. At 0 a
//            switch on the pot takes the stage out (stock amp). DEEP changes the input cap 4.7n -> 47n
//            (low cut ~72 Hz -> ~7 Hz in front of the extra gain: a modest bass boost).
//            (It sits after V1A rather than after V3A: there the signal is already clipped to a
//            square wave and an extra stage changed the sound by 1 dB at most.)
//
// Units: process() takes volts at the input jack and returns volts at the speaker (16 ohm load).
#pragma once
#include "DKNet.h"
#include <array>

namespace drect {

struct Params
{
    int channel = 1;                 // 0 = ORANGE, 1 = RED
    int orMode = 1;                  // ORANGE: 0 = CLN, 1 = NORM, 2 = MOD
    int rdMode = 0;                  // RED:    0 = NORM, 1 = VINT
    bool el34 = false;               // bias select: false = 6L6GC, true = EL34
    bool tubeRect = false;           // rectifier: false = silicon diodes, true = 2 x 5U4
    bool spongy = false;             // power: false = BOLD, true = SPONGY
    bool tight = false, defizz = false, deep = false;
    bool midShift = false;           // MID SHIFT mod: tone-stack mid caps 20n -> 47n (both channels)
    // knob positions 0..1 (0 = fully counter-clockwise)
    double orGain = 0.5, orTreble = 0.5, orMid = 0.5, orBass = 0.5, orPres = 0.5, orMaster = 0.3;
    double rdGain = 0.6, rdTreble = 0.5, rdMid = 0.5, rdBass = 0.5, rdPres = 0.5, rdMaster = 0.3;
    double scary = 0.0;              // Mr. Scary gain
};

// Mr. Scary GAIN fully counter-clockwise switches the stage out of the circuit
inline bool scaryEngaged (double knob) { return knob > 0.005; }

// log ("A") pot law: p = fraction of the track at mid rotation (e.g. 0.15 for a "15A" pot)
inline double taperA (double x, double p)
{
    x = std::clamp (x, 0.0, 1.0);
    if (p >= 0.499) return x;
    const double b = (1.0 / p - 1.0) * (1.0 / p - 1.0);
    return (std::pow (b, x) - 1.0) / (b - 1.0);
}

// The LDR truth table (schematic sheet mbdr7). true = LDR lit (low resistance).
struct LdrState
{
    bool l1, l2, l3, l4, l5, l6, l7, l8, l9, l10, l14, l15, l16, l19, l20;
    static LdrState from (int channel, int orMode, int rdMode)
    {
        //                 1      2      3(a,b) 4      5      6      7      8      9      10     14     15     16     19     20
        static constexpr LdrState orNorm  { false, true,  true,  true,  false, true,  true,  false, true,  true,  false, true,  false, true,  false };
        static constexpr LdrState orCln   { false, true,  false, false, false, true,  true,  false, true,  false, false, true,  true,  true,  true  };
        static constexpr LdrState orMod   { false, true,  true,  true,  false, true,  false, false, true,  true,  false, true,  false, false, false };
        static constexpr LdrState rdNorm  { true,  false, true,  true,  true,  false, false, true,  false, true,  true,  false, false, false, false };
        static constexpr LdrState rdVint  { true,  false, true,  true,  true,  false, true,  true,  false, true,  true,  false, false, true,  false };
        if (channel == 0) return orMode == 0 ? orCln : (orMode == 2 ? orMod : orNorm);
        return rdMode == 1 ? rdVint : rdNorm;
    }
};

class DualRect
{
public:
    // supply rails at idle (schematic sheet mbdr4: A 460, B 454, C 422, D 406, E 402)
    static constexpr double kA = 460.0, kB = 454.0, kC = 422.0, kD = 406.0, kE = 402.0;
    static constexpr double kRaa = 1900.0;           // plate-to-plate load, 4 x 6L6 100 W transformer
    static constexpr double kLdrOn = 150.0, kLdrOff = 25.0e6;   // LDR lit / dark
    static constexpr double kSwOn = 1.0, kSwOff = 1.0e9;         // mechanical switch closed / open
    static inline double kFbSign = -1.0;             // polarity of the speaker tap feeding the NFB network
    static inline double kEl34Drive = 0.8318;        // -1.6 dB drive trim with EL34s (level match, tests/level_match)
    static inline int kFbMaxIter = 8;
    static inline double kFbTol = 5.0e-2;
    // idle current per tube the bias supply is set for (the schematic's -51 V / -39 V fixed bias)
    static inline double kIdle6L6 = 0.034, kIdleEl34 = 0.032;
    static inline double kDefizzC = 470e-12;           // DE-FIZZ cap across V3A's plate load
    static inline double kScaryCf = 33e-12;            // across the Mr. Scary feedback resistor

    void prepare (double sampleRate)
    {
        fs = sampleRate; T = 1.0 / fs;
        tri = TriodeModel::ax7();
        buildPreamp();
        buildPA();
        setParams (params, true);
        solveAllDC();
        for (int i = 0, n = (int) (0.25 * fs); i < n; ++i) process (0.0);     // settle states and supply
    }

    void setParams (const Params& p, bool force = false)
    {
        if (pre.segs.empty()) { params = p; return; }        // not built yet: prepare() applies them
        const bool tubeChange = p.el34 != params.el34;
        params = p;
        const auto L = LdrState::from (p.channel, p.orMode, p.rdMode);
        auto ldr = [] (DKNet& n, int id, bool on) { n.setResistance (id, on ? kLdrOn : kLdrOff); };
        auto sw  = [] (DKNet& n, int id, bool on) { n.setResistance (id, on ? kSwOn : kSwOff); };

        // ---- A: V1A, pad, gain pots
        {
            auto& n = pre.segs[0].net;
            ldr (n, idL3b, L.l3); ldr (n, idL4, L.l4); ldr (n, idL1, L.l1); ldr (n, idL2, L.l2);
            ldr (n, idL5, L.l5); ldr (n, idL6, L.l6);
            sw (n, idTightK, ! p.tight);                         // the extra 530n (1u total) unless TIGHT
            sw (n, idTightC, ! p.tight);                         // the extra 17.8n (20n total) unless TIGHT
            sw (n, idMillerV2A, L.l3);                           // V2A gain (Miller) with its cathode bypassed
            // Mr. Scary
            const bool scaryOn = scaryEngaged (p.scary);
            sw (n, idDeep, p.deep);
            n.setResistance (idScaryRf, 470e3 + 10e6 * taperA (p.scary, 0.10));
            n.setResistance (idScaryRin, scaryOn ? 470e3 : kSwOff);
            sw (n, idScaryOut, scaryOn);
            sw (n, idScaryByp, ! scaryOn);
            n.setPot (idOrGain, 1e6, taperA (p.orGain, 0.12));
            n.setPot (idRdGain, 1e6, taperA (p.rdGain, 0.12));
            n.update();
        }
        // ---- B: V2A
        { auto& n = pre.segs[1].net; ldr (n, idL3a, L.l3); n.update(); }
        // ---- C: V2B
        { auto& n = pre.segs[2].net; sw (n, idMillerV3A, L.l10); n.update(); }
        // ---- D: V3A, Mr. Scary, CF, tone stacks, masters
        {
            auto& n = pre.segs[3].net;
            ldr (n, idL10, L.l10); ldr (n, idL7, L.l7); ldr (n, idL8, L.l8); ldr (n, idL9, L.l9);
            ldr (n, idL14, L.l14); ldr (n, idL15, L.l15); ldr (n, idL16, L.l16);
            sw (n, idDefizzCap, p.defizz);
            n.setResistance (idDefizzStop, p.defizz ? 22e3 : 1.0);
            sw (n, idRdMidShift, p.midShift);
            sw (n, idOrMidShift, p.midShift);
            // RED stack
            n.setPot (idRdTreble, 220e3, p.rdTreble);
            n.setResistance (idRdBass, 1e6 * taperA (p.rdBass, 0.15));
            n.setPot (idRdMid, 25e3, p.rdMid);
            n.setResistance (idRdPres, 25e3 * p.rdPres);
            n.setPot (idRdMaster, 1e6, taperA (p.rdMaster, 0.15));
            // ORANGE stack
            n.setPot (idOrTreble, 220e3, p.orTreble);
            n.setResistance (idOrBass, 1e6 * taperA (p.orBass, 0.15));
            n.setPot (idOrMid, 25e3, p.orMid);
            n.setPot (idOrMaster, 1e6, taperA (p.orMaster, 0.15));
            n.update();
        }
        // ---- power amp: NFB LDRs and the ORANGE presence (25k rheostat; CW = less resistance = more presence)
        ldr (pa, idL19, L.l19); ldr (pa, idL20, L.l20);
        pa.setResistance (idOrPres, 25e3 * taperA (1.0 - p.orPres, 0.10));
        pa.update();
        if (tubeChange && prepared) rebias();
        (void) force;
    }

    // one sample at the core rate: volts at the input jack -> volts at the speaker
    inline double process (double vin)
    {
        const double bus = runChain (pre, vin);
        return processPA (bus * (params.el34 ? kEl34Drive : 1.0));
    }

    // master bus (PI input) -> power amp -> speaker volts
    inline double processPA (double a)
    {
        // The global feedback loop is solved implicitly within the sample (as in Amp5150 III):
        // Newton iteration on the feedback voltage using the PI's and the output stage's small-signal
        // sensitivities, until the feedback the PI sees agrees with the output it produces.
        const double vy = kC * (vScreen / kScreenIdle);
        const double fbGain = kFbSign * 0.70710678;          // 8-ohm tap of the 16-ohm winding
        double u[5] = { a, 2.0 * uFb1 - uFb2, vy, vBias, 0.0 };
        double y[DKNet::MY] {};
        double sens[DKNet::MY];
        double lastAbsRes = 1e300, step = 0.0;
        bool solved = false;
        for (int it = 0; it < kFbMaxIter; ++it)
        {
            pa.solve (u, y, it == 0);
            double dA, dB;
            const double vs0 = powerStage (y[0], y[1], false, &dA, &dB, it > 0);
            const double res = fbGain * vs0 - u[1];
            const double absRes = std::fabs (res);
            if (absRes < kFbTol)
            {
                pa.sensitivity (1, sens);
                const double loop = fbGain * (dA * sens[0] + dB * sens[1]);
                const double du = res / std::max (1.0 - loop, 1.0e-3);
                u[1] += du;
                pa.applyInputStep (du, y);
                solved = true; break;
            }
            if (it > 0 && absRes > lastAbsRes) { step *= -0.5; u[1] += step; continue; }
            lastAbsRes = absRes;
            pa.sensitivity (1, sens);
            const double loop = fbGain * (dA * sens[0] + dB * sens[1]);
            step = std::clamp (res / std::max (1.0 - loop, 1.0e-3), -60.0, 60.0);
            u[1] += step;
        }
        if (! solved) { pa.solve (u, y, false); ++fbUnsolved; }
        const double vs = powerStage (y[0], y[1], true, nullptr, nullptr);
        pa.commit (u);
        uFb2 = uFb1; uFb1 = u[1];
        for (int j = 0; j < 3; ++j) paTaps[j] = 2 + j < pa.numOutputs() ? y[2 + j] : 0.0;
        return vs;
    }

    const Params& getParams() const { return params; }

    // ---- for testing / metering
    double lastBus() const { return lastPre; }
    double plateSupply() const { return vPlate; }
    double screenSupply() const { return vScreen; }
    double biasVoltage() const { return vBias; }
    double tap (int seg, int j) const { return pre.taps[(size_t) seg][(size_t) j]; }
    double segOut (int seg) const { return pre.out[(size_t) seg]; }
    double segCathode (int seg) const { return pre.cath[(size_t) seg]; }
    double lastPlateA() const { return lastVa; }
    double lastPiGrid (int side) const { return side == 0 ? lastVgA : lastVgB; }
    double lastFb() const { return uFb1; }
    double idleCurrentPerTube() const { return idlePerTube; }
    double paTap (int j) const { return paTaps[j]; }          // test builds: PI cathode, V5B plate, V5A plate
    long fbUnsolved = 0;

private:
    struct Segment { DKNet net; double supply = kD; };
    struct Chain
    {
        std::vector<Segment> segs;
        std::vector<double> out, cath, ref;
        std::vector<std::array<double, 4>> taps;
    };

    double fs = 96000.0, T = 1.0 / 96000.0;
    bool prepared = false;
    Params params;
    TriodeModel tri;
    Chain pre;
    double lastPre = 0.0;

    // run-time resistor ids (LDRs, switches, pots)
    int idL1 = -1, idL2 = -1, idL3a = -1, idL3b = -1, idL4 = -1, idL5 = -1, idL6 = -1, idL7 = -1, idL8 = -1, idL9 = -1,
        idL10 = -1, idL14 = -1, idL15 = -1, idL16 = -1, idL19 = -1, idL20 = -1;
    int idTightK = -1, idTightC = -1, idMillerV2A = -1, idMillerV3A = -1;
    int idRdMidShift = -1, idOrMidShift = -1;
    int idDefizzCap = -1, idDefizzStop = -1, idDeep = -1, idScaryRf = -1, idScaryRin = -1, idScaryOut = -1, idScaryByp = -1;
    int idOrGain = -1, idRdGain = -1;
    int idRdTreble = -1, idRdBass = -1, idRdMid = -1, idRdPres = -1, idRdMaster = -1;
    int idOrTreble = -1, idOrBass = -1, idOrMid = -1, idOrMaster = -1, idOrPres = -1;

    // ---------------------------------------------------------------- preamp chain
    inline double runChain (Chain& c, double vin)
    {
        double x = vin;
        const int n = (int) c.segs.size();
        double u[3], y[DKNet::MY];
        for (int k = 0; k < n; ++k)
        {
            auto& s = c.segs[(size_t) k];
            u[0] = x; u[1] = s.supply; u[2] = c.ref[(size_t) k];
            s.net.process (u, y);
            x = y[0];
            c.out[(size_t) k] = y[0];
            c.cath[(size_t) k] = y[1];
            auto& t = c.taps[(size_t) k];
            for (int j = 0; j < 4; ++j) t[(size_t) j] = j + 2 < s.net.numOutputs() ? y[j + 2] : 0.0;
        }
        for (int k = 0; k + 1 < n; ++k) c.ref[(size_t) k] = c.cath[(size_t) k + 1];
        lastPre = x;
        return x;
    }

    void initChainVectors (Chain& c)
    {
        const size_t n = c.segs.size();
        c.out.assign (n, 0.0); c.cath.assign (n, 1.5); c.ref.assign (n, 1.5); c.taps.assign (n, { 0, 0, 0, 0 });
        for (auto& s : c.segs) s.net.prepare (fs);
    }

    void solveChainDC (Chain& c)
    {
        const int n = (int) c.segs.size();
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int k = 0; k < n; ++k)
            {
                auto& s = c.segs[(size_t) k];
                double u[3] = { 0.0, s.supply, c.ref[(size_t) k] };
                s.net.solveDC (u);
                double y[DKNet::MY];
                s.net.process (u, y);
                c.out[(size_t) k] = y[0]; c.cath[(size_t) k] = y[1];
            }
            for (int k = 0; k + 1 < n; ++k) c.ref[(size_t) k] = c.cath[(size_t) k + 1];
        }
    }

    // 12AX7 inter-electrode capacitances, for triodes whose grid is a node inside the same net
    static void triodeCaps (DKNet& n, int g, int p, int k)
    {
        n.capacitor (g, p, 1.7e-12);          // Cgp (Miller)
        n.capacitor (g, k, 1.6e-12);          // Cgk
    }
    // a grid that is the last node of one net: the next stage's input capacitance, Miller effect
    // included (Cgk + Cgp * (1 + |Av|), Av = that stage's small-signal gain in circuit)
    static void millerLoad (DKNet& n, int g, double gain)
    {
        n.capacitor (g, DKNet::gnd, 1.6e-12 + 1.7e-12 * (1.0 + gain));
    }
    static void tapOut (DKNet& n, int node)
    {
#ifdef DRECT_TAPS
        n.output (node);
#else
        (void) n; (void) node;
#endif
    }

    void buildPreamp()
    {
        pre.segs.clear(); pre.segs.resize (4);
        // ---- A: V1A -> pad network -> ORANGE / RED gain pots -> 470k -> V2A grid
        {
            auto& n = pre.segs[0].net; pre.segs[0].supply = kE;
            const int in = n.source(), w = n.source(), ref = n.source();
            const int p = n.node(), k = n.node();
            n.triode (in, p, k, &tri);                       // V1A (grid straight from the input jack, 1M to ground)
            triodeCaps (n, in, p, k);
            n.resistor (k, DKNet::gnd, 1.8e3);               // 1k8
            const int ny = n.node(), nt = n.node();
            n.capacitor (k, ny, 470e-9);                     // 1u cathode cap = 470n ...
            n.capacitor (k, nt, 530e-9);                     // ... + 530n, switched out by TIGHT
            idTightK = n.resistor (nt, ny, kSwOn);
            n.resistor (ny, DKNet::gnd, 47e3);               // 47k
            idL3b = n.resistor (ny, DKNet::gnd, kLdrOn);     // LDR3b (V1A cathode cap)
            n.resistor (p, w, 220e3);                        // 220k plate load (E)
            // Mr. Scary stage (V1C, our own design): input cap 4.7n (+42.3n with DEEP) -> 470k -> grid
            // (1M leak); plate -> 47n -> Rf || 33p -> grid; 220k / 1k8 || 22u from E (same DC point as V1A)
            const int n1 = n.node(), nd = n.node(), g1c = n.node(), nf = n.node(), p1c = n.node(), k1c = n.node();
            n.capacitor (p, n1, 4.7e-9);
            n.capacitor (p, nd, 42.3e-9);
            idDeep = n.resistor (nd, n1, kSwOff);
            idScaryRin = n.resistor (n1, g1c, 470e3);
            n.resistor (g1c, DKNet::gnd, 1e6);
            idScaryRf = n.resistor (g1c, nf, 470e3);
            n.capacitor (g1c, nf, kScaryCf);                 // keeps the extra gain in the lows and mids
            n.capacitor (nf, p1c, 47e-9);
            n.triode (g1c, p1c, k1c, &tri);
            triodeCaps (n, g1c, p1c, k1c);
            n.resistor (k1c, DKNet::gnd, 1.8e3); n.capacitor (k1c, DKNet::gnd, 22e-6);
            n.resistor (p1c, w, 220e3);
            // GAIN fully CCW switches the stage out (a switch on the pot): V1A drives the coupling cap, as stock
            const int cIn = n.node();
            idScaryOut = n.resistor (p1c, cIn, kSwOff);
            idScaryByp = n.resistor (p, cIn, kSwOn);
            // V1A coupling: 20n = 2n2 + 17.8n (the 17.8n is switched out by TIGHT)
            const int P1 = n.node(), nc = n.node();
            n.capacitor (cIn, P1, 2.2e-9);
            n.capacitor (cIn, nc, 17.8e-9);
            idTightC = n.resistor (nc, P1, kSwOn);
            n.resistor (P1, DKNet::gnd, 2.2e6);              // 2M2
            // pad: P1 -> (680k || 2n via LDR4) || 82p || 2M2 -> Q
            const int A = n.node(), Q = n.node();
            n.resistor (P1, A, 680e3); n.capacitor (P1, A, 2e-9);
            idL4 = n.resistor (A, Q, kLdrOn);                // LDR4 (V1A output pad)
            n.capacitor (P1, Q, 82e-12);
            n.resistor (P1, Q, 2.2e6);
            // input selects and gain pots (1M, 1n bright cap top -> wiper)
            const int rgt = n.node(), rgw = n.node(), ogt = n.node(), ogw = n.node(), W = n.node(), g = n.node();
            idL1 = n.resistor (Q, rgt, kLdrOn);              // LDR1 (RED input select)
            idL2 = n.resistor (Q, ogt, kLdrOn);              // LDR2 (ORANGE input select)
            idRdGain = n.pot (rgt, rgw, DKNet::gnd, 1e6);    // RED GAIN 1M
            n.capacitor (rgt, rgw, 1e-9);
            idOrGain = n.pot (ogt, ogw, DKNet::gnd, 1e6);    // ORANGE GAIN 1M
            n.capacitor (ogt, ogw, 1e-9);
            idL5 = n.resistor (rgw, W, kLdrOn);              // LDR5 (RED gain pot)
            idL6 = n.resistor (ogw, W, kLdrOn);              // LDR6 (ORANGE gain pot)
            n.resistor (W, g, 470e3);                        // 470k grid stopper
            n.capacitor (g, DKNet::gnd, 20e-12);             // 20p
            millerLoad (n, g, 28.0);                         // V2A, cathode unbypassed ...
            const int ms = n.node();
            n.capacitor (g, ms, 1.7e-12 * 32.0);             // ... + the extra Miller capacitance when bypassed
            idMillerV2A = n.resistor (ms, DKNet::gnd, kSwOn);
            n.diode (g, ref, &tri.grid);                     // V2A grid conduction
            n.output (g); n.output (k); tapOut (n, p); tapOut (n, Q); tapOut (n, p1c);
        }
        // ---- B: V2A -> 20n -> 470k / 1M -> V2B grid
        {
            auto& n = pre.segs[1].net; pre.segs[1].supply = kD;
            const int in = n.source(), w = n.source(), ref = n.source();
            const int p = n.node(), k = n.node();
            n.triode (in, p, k, &tri);                       // V2A
            n.resistor (k, DKNet::gnd, 1.8e3);               // 1k8
            const int ny = n.node();
            n.capacitor (k, ny, 1e-6);                       // 1u
            n.resistor (ny, DKNet::gnd, 47e3);               // 47k
            idL3a = n.resistor (ny, DKNet::gnd, kLdrOn);     // LDR3a (V2A cathode cap)
            n.resistor (p, w, 100e3);                        // 100k (D)
            const int a = n.node(), g = n.node();
            n.capacitor (p, a, 20e-9);                       // 20n
            n.resistor (a, g, 470e3);                        // 470k
            n.resistor (g, DKNet::gnd, 1e6);                 // 1M
            millerLoad (n, g, 2.5);                          // V2B (39k unbypassed)
            n.diode (g, ref, &tri.grid);
            n.output (g); n.output (k); tapOut (n, p);
        }
        // ---- C: V2B (39k cold clipper, 1n across its plate load) -> 20n -> 330k / 220k -> V3A grid
        {
            auto& n = pre.segs[2].net; pre.segs[2].supply = kD;
            const int in = n.source(), w = n.source(), ref = n.source();
            const int p = n.node(), k = n.node();
            n.triode (in, p, k, &tri);                       // V2B
            n.resistor (k, DKNet::gnd, 39e3);                // 39k
            n.resistor (p, w, 100e3); n.capacitor (p, w, 1e-9);    // 100k || 1n
            const int a = n.node(), g = n.node();
            n.capacitor (p, a, 20e-9);                       // 20n
            n.resistor (a, DKNet::gnd, 330e3);               // 330k
            n.resistor (a, g, 220e3);                        // 220k
            millerLoad (n, g, 38.0);                         // V3A unbypassed (ORANGE CLN) ...
            const int ms = n.node();
            n.capacitor (g, ms, 1.7e-12 * 32.0);             // ... + extra Miller capacitance when bypassed
            idMillerV3A = n.resistor (ms, DKNet::gnd, kSwOn);
            n.diode (g, ref, &tri.grid);
            n.output (g); n.output (k); tapOut (n, p);
        }
        // ---- D: V3A -> V3B cathode follower -> tone stacks -> masters -> bus
        {
            auto& n = pre.segs[3].net; pre.segs[3].supply = kC;
            const int in = n.source(), x = n.source(); n.source();   // third input unused
            // V3A
            const int p3a = n.node(), k3a = n.node();
            n.triode (in, p3a, k3a, &tri);
            n.resistor (k3a, DKNet::gnd, 1.8e3);             // 1k8
            const int ny = n.node();
            n.capacitor (k3a, ny, 1e-6);                     // 1u
            n.resistor (ny, DKNet::gnd, 47e3);               // 47k
            idL10 = n.resistor (ny, DKNet::gnd, kLdrOn);     // LDR10 (V3 cathode cap)
            n.resistor (p3a, x, 220e3);                      // 220k (C)
            const int ndf = n.node();
            n.capacitor (p3a, ndf, kDefizzC);                // DE-FIZZ: cap across the plate load
            idDefizzCap = n.resistor (ndf, x, kSwOff);
            // V3B cathode follower (DC coupled), grid stopper 22k with DE-FIZZ
            const int g3b = n.node(), k3b = n.node();
            idDefizzStop = n.resistor (p3a, g3b, 1.0);
            n.triode (g3b, x, k3b, &tri);
            triodeCaps (n, g3b, x, k3b);
            n.resistor (k3b, DKNet::gnd, 100e3);             // 100k
            // tone-cap LDRs: the selected stack is driven by the CF, the other is fed through 1M
            const int R0 = n.node(), O0 = n.node();
            idL8 = n.resistor (k3b, R0, kLdrOn);             // LDR8 (RED tone caps)
            idL9 = n.resistor (k3b, O0, kLdrOn);             // LDR9 (ORANGE tone caps)
            n.resistor (R0, O0, 1e6);
            // RED stack: 680p treble, 47k slope, 20n bass, 20n mid (into the mid-pot wiper)
            const int rt = n.node(), rw = n.node(), rb = n.node(), rm = n.node(), rmw = n.node(), R1 = n.node();
            n.capacitor (R0, rt, 680e-12);
            n.resistor (R0, R1, 47e3);
            n.capacitor (R1, rb, 20e-9);
            n.capacitor (R1, rmw, 20e-9);
            const int rms = n.node();
            n.capacitor (R1, rms, 27e-9);                    // MID SHIFT: + 27n = 47n mid cap
            idRdMidShift = n.resistor (rms, rmw, kSwOff);
            idRdTreble = n.pot (rt, rw, rb, 220e3);          // TREBLE 220k
            idRdBass = n.resistor (rb, rm, 500e3);           // BASS 1M rheostat
            idRdMid = n.pot (rm, rmw, DKNet::gnd, 25e3);     // MID 25k
            // RED presence: 22k + 3n + 25k rheostat to ground from the treble wiper
            const int rp1 = n.node(), rp2 = n.node();
            n.resistor (rw, rp1, 22e3); n.capacitor (rp1, rp2, 3e-9);
            idRdPres = n.resistor (rp2, DKNet::gnd, 12.5e3);
            const int rmv = n.node(), bus = n.node();
            idRdMaster = n.pot (rw, rmv, DKNet::gnd, 1e6);   // RED MASTER 1M
            idL14 = n.resistor (rmv, bus, kLdrOn);           // LDR14
            // ORANGE stack: 500p treble, 47k slope, 20n bass, 20n mid
            const int ot = n.node(), ow = n.node(), ob = n.node(), om = n.node(), omw = n.node(), O1 = n.node();
            n.capacitor (O0, ot, 500e-12);
            n.resistor (O0, O1, 47e3);
            n.capacitor (O1, ob, 20e-9);
            n.capacitor (O1, omw, 20e-9);
            const int oms = n.node();
            n.capacitor (O1, oms, 27e-9);                    // MID SHIFT: + 27n = 47n mid cap
            idOrMidShift = n.resistor (oms, omw, kSwOff);
            idOrTreble = n.pot (ot, ow, ob, 220e3);
            idOrBass = n.resistor (ob, om, 500e3);
            idOrMid = n.pot (om, omw, DKNet::gnd, 25e3);
            // treble roll-off: 22k + 3n + (100k || LDR7) to ground
            const int q1 = n.node(), q2 = n.node();
            n.resistor (ow, q1, 22e3); n.capacitor (q1, q2, 3e-9);
            n.resistor (q2, DKNet::gnd, 100e3);
            idL7 = n.resistor (q2, DKNet::gnd, kLdrOff);     // LDR7
            // master bypass (CLN): 82k || 82p to the bus through LDR16
            const int bb = n.node();
            n.resistor (ow, bb, 82e3); n.capacitor (ow, bb, 82e-12);
            idL16 = n.resistor (bb, bus, kLdrOff);           // LDR16
            const int omv = n.node();
            idOrMaster = n.pot (ow, omv, DKNet::gnd, 1e6);   // ORANGE MASTER 1M
            idL15 = n.resistor (omv, bus, kLdrOff);          // LDR15
            // bus: no FX loop (RY2 in bypass) -> 20n into the PI grid (1M, bootstrapped): ~1M load
            n.resistor (bus, DKNet::gnd, 1.0e6);
            n.output (bus); n.output (k3a); tapOut (n, p3a); tapOut (n, k3b); tapOut (n, rw);
        }
        initChainVectors (pre);
    }

    // ---------------------------------------------------------------- power amp
    DKNet pa;
    PentodeModel tube = PentodeModel::sixL6();
    GridDiode pairGrid { 4.0e-3, 0.1 };                      // two power tubes in parallel per side
    static constexpr double kScreenIdle = kB;
    static constexpr double kScreenR = 1.0e3;                // 1k / 2W per tube
    static constexpr double kChokeR = 280.0;                 // choke DCR (A 460 -> B 454 at idle)
    double vBias = -51.0, idlePerTube = 0.034;
    double vPlate = kA, vScreen = kB, vnl = 480.0, iPre = 0.012, iRect = 0.0;
    static constexpr double rSup = 75.0, cRes = 110e-6, cScr = 30e-6;
    static constexpr double kRect5U4 = 103.0;                // 2 x 5U4: drop = k * I^(2/3) (~30 V at idle)
    double ig2a = 0.0, ig2b = 0.0, dPrev = 0.0, dPrev2 = 0.0, dTrial = 0.0, uFb1 = 0.0, uFb2 = 0.0;
    double lastVa = 0.0, lastVgA = 0.0, lastVgB = 0.0, paTaps[3] {};

    // output transformer + reflected speaker load (same network as Amp5150 III)
    struct Load
    {
        static constexpr int N = 4;
        double M[N][N] {};
        double gLh = 0, gLk = 0, gLe = 0, gLr = 0, gCw = 0, gCr = 0;
        double jLh = 0, jLk = 0, jLe = 0, jLr = 0, sCw = 0, sCr = 0;
        double v[N] {};
        double kRef = 1.0;
        static inline double kCoreLoss = 30e3;             // plate-to-plate equivalent core-loss resistance
        void prepare (double T, double raa)
        {
            kRef = (raa / 4.0) / 16.0;
            const double Re = 13.0 * kRef, Rle = 45.0 * kRef, Rres = 55.0 * kRef;
            const double Lh = 30.0 / 4.0, Lk = 8e-3 / 4.0, Cw = 1.0e-9 * 4.0;
            const double Le = 0.9e-3 * kRef, Lres = 25e-3 * kRef, Cres = 125e-6 / kRef;
            gLh = T / (2.0 * Lh); gLk = T / (2.0 * Lk); gLe = T / (2.0 * Le); gLr = T / (2.0 * Lres);
            gCw = 2.0 * Cw / T; gCr = 2.0 * Cres / T;
            double G[N][N] {};
            auto st = [&] (int a, int b, double g)
            {
                if (a >= 0) G[a][a] += g;
                if (b >= 0) G[b][b] += g;
                if (a >= 0 && b >= 0) { G[a][b] -= g; G[b][a] -= g; }
            };
            st (0, -1, gLh); st (0, -1, gCw);
            st (0, -1, 1.0 / (kCoreLoss / 4.0));            // core loss (plate-to-plate value reflected to a half)
            st (0, 1, gLk);
            st (1, 2, 1.0 / Re);
            st (2, 3, 1.0 / Rle); st (2, 3, gLe);
            st (3, -1, 1.0 / Rres); st (3, -1, gLr); st (3, -1, gCr);
            invert (G, M);
            reset();
        }
        void reset() { jLh = jLk = jLe = jLr = sCw = sCr = 0; for (auto& x : v) x = 0; }
        static void invert (const double a[N][N], double m[N][N])
        {
            double w[N][2 * N];
            for (int r = 0; r < N; ++r) for (int c = 0; c < 2 * N; ++c) w[r][c] = c < N ? a[r][c] : (c - N == r ? 1.0 : 0.0);
            for (int c = 0; c < N; ++c)
            {
                int piv = c; for (int r = c + 1; r < N; ++r) if (std::fabs (w[r][c]) > std::fabs (w[piv][c])) piv = r;
                for (int k = 0; k < 2 * N; ++k) std::swap (w[c][k], w[piv][k]);
                const double inv = 1.0 / w[c][c];
                for (int k = 0; k < 2 * N; ++k) w[c][k] *= inv;
                for (int r = 0; r < N; ++r) if (r != c) { const double f = w[r][c]; for (int k = 0; k < 2 * N; ++k) w[r][k] -= f * w[c][k]; }
            }
            for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) m[r][c] = w[r][N + c];
        }
        inline void rhs (double r[N]) const
        {
            r[0] = -jLh + sCw - jLk;
            r[1] = jLk;
            r[2] = -jLe;
            r[3] = jLe - jLr + sCr;
        }
        inline void thevenin (double& z0, double& h, double& zs, double& hs) const
        {
            double r[N]; rhs (r);
            z0 = M[0][0]; zs = M[1][0];
            h = 0; hs = 0;
            for (int k = 0; k < N; ++k) { h += M[0][k] * r[k]; hs += M[1][k] * r[k]; }
        }
        inline void commit (double d)
        {
            double r[N]; rhs (r); r[0] += d;
            for (int i = 0; i < N; ++i) { double a = 0; for (int k = 0; k < N; ++k) a += M[i][k] * r[k]; v[i] = a; }
            auto ind = [] (double g, double& j, double vab) { const double i = g * vab + j; j = i + g * vab; };
            ind (gLh, jLh, v[0]);
            ind (gLk, jLk, v[0] - v[1]);
            ind (gLe, jLe, v[2] - v[3]);
            ind (gLr, jLr, v[3]);
            sCw = 2.0 * gCw * v[0] - sCw;
            sCr = 2.0 * gCr * v[3] - sCr;
        }
        inline double speakerVolts() const { return v[1] / std::sqrt (kRef); }
    } load;

    void buildPA()
    {
        auto& n = pa;
        const int in = n.source(), fb = n.source(), y = n.source(), bias = n.source(), vk = n.source();
        // long-tail-pair phase inverter (V5): V5B takes the signal, V5A's grid is AC-grounded to the
        // feedback node F; tail 470R + 10k + 4k7
        const int gB = n.node(), gA = n.node(), M = n.node(), K = n.node(), F = n.node();
        n.capacitor (in, gB, 20e-9);                         // 20n (RY2 bypass position)
        n.resistor (gB, M, 1e6);                             // 1M
        n.resistor (gA, M, 1e6);                             // 1M
        n.capacitor (gA, F, 100e-9);                         // 100n
        n.resistor (K, M, 470.0);                            // 470R
        n.resistor (M, F, 10e3);                             // 10k
        n.resistor (F, DKNet::gnd, 4.7e3);                   // 4k7
        const int Pp = n.node();
        n.capacitor (F, Pp, 100e-9);                         // 100n
        idOrPres = n.resistor (Pp, DKNet::gnd, 12.5e3);      // ORANGE PRESENCE 25k rheostat
        // NFB: FB -> 100n -> 47k (|| 47k via LDR20) -> LDR19 -> F
        const int J = n.node(), Jx = n.node(), K19 = n.node();
        n.capacitor (fb, J, 100e-9);
        n.resistor (J, K19, 47e3);
        n.resistor (J, Jx, 47e3);
        idL20 = n.resistor (Jx, K19, kLdrOff);
        idL19 = n.resistor (K19, F, kLdrOn);
        const int pB = n.node(), pA = n.node();
        n.triode (gB, pB, K, &tri);                          // V5B
        n.triode (gA, pA, K, &tri);                          // V5A
        triodeCaps (n, gB, pB, K);
        triodeCaps (n, gA, pA, K);
        n.resistor (pB, y, 82e3);  n.capacitor (pB, y, 120e-12);    // 82k || 120p
        n.resistor (pA, y, 90e3);  n.capacitor (pA, y, 120e-12);    // 90k || 120p
        n.capacitor (pB, pA, 75e-12);                        // 75p
        // coupling caps, 220k bias feeds, 1k5 grid stoppers (two tubes per side: 750R)
        const int XA = n.node(), tA = n.node(), XB = n.node(), tB = n.node();
        n.capacitor (pB, XA, 47e-9);
        n.resistor (XA, bias, 220e3);
        n.resistor (XA, tA, 750.0);
        n.capacitor (pA, XB, 47e-9);
        n.resistor (XB, bias, 220e3);
        n.resistor (XB, tB, 750.0);
        n.diode (tA, vk, &pairGrid);
        n.diode (tB, vk, &pairGrid);
        n.output (tA); n.output (tB); tapOut (n, K); tapOut (n, pB); tapOut (n, pA);
        n.prepare (fs);
        load.prepare (T, kRaa);
    }

    // rectifier current for a voltage difference dv between the no-load and the reservoir voltage
    inline double rectCurrent (double dv)
    {
        if (dv <= 0.0) { iRect = 0.0; return 0.0; }
        const double rs = rSup * (params.spongy ? 1.4 : 1.0);
        if (! params.tubeRect) { iRect = dv / rs; return iRect; }
        // 5U4 (Child's law plate drop) in series with the winding resistance: rs*I + k*I^(2/3) = dv
        double i = std::max (iRect, 1.0e-4);
        for (int it = 0; it < 4; ++it)
        {
            const double c = std::cbrt (i);
            const double f = rs * i + kRect5U4 * c * c - dv;
            const double df = rs + kRect5U4 * (2.0 / 3.0) / c;
            i = std::max (1.0e-7, i - f / df);
        }
        iRect = i;
        return i;
    }

    // sets the bias supply so each tube idles at its target current (at the nominal 460 V / 454 V),
    // then re-solves the PA DC point
    void rebias()
    {
        tube = params.el34 ? PentodeModel::el34() : PentodeModel::sixL6();
        idlePerTube = params.el34 ? kIdleEl34 : kIdle6L6;
        double vg2 = kB, ig2 = 0.0;
        for (int it = 0; it < 6; ++it)
        {
            double lo = -150.0, hi = 0.0;
            for (int k = 0; k < 60; ++k)
            {
                const double mid = 0.5 * (lo + hi);
                if (tube.cathodeCurrent (mid, vg2, kA) > idlePerTube) hi = mid; else lo = mid;
            }
            vBias = 0.5 * (lo + hi);
            double d; tube.eval (vBias, vg2, kA, d, ig2);
            vg2 = kB - kScreenR * ig2;
        }
        double d, g2;
        const double ipIdle = tube.eval (vBias, vg2, kA, d, g2);
        const double iChoke = (kA - kB) / kChokeR;
        iPre = std::max (0.0, iChoke - 4.0 * g2);
        vnl = kA + rSup * (4.0 * ipIdle + iChoke);           // silicon, BOLD: 460 V at idle
        vPlate = kA; vScreen = kB; ig2a = ig2b = g2; iRect = 0.0;
        dPrev = dPrev2 = dTrial = 0.0; uFb1 = uFb2 = 0.0;
        load.reset();
        double u[5] = { 0.0, 0.0, kC, vBias, 0.0 };
        pa.solveDC (u);
    }

    void solveAllDC()
    {
        solveChainDC (pre);
        prepared = true;
        rebias();
    }

    // winding current of one side of the output stage (two tubes in parallel), minus a plate clamp
    // (flyback) diode to ground. The traced schematic does not show these diodes; they are added
    // because without them the model's transformer flyback drives the cut-off plates to ~1 kV when
    // the amp is cranked (they are commonly fitted to high-power push-pull amps for that reason).
    // They only conduct when a plate is driven below ground.
    inline double plateSide (double ik, double va, double& dva) const
    {
        double dp; const double ip = tube.plateFrom (ik, va, dp);
        const double vt = 0.05, G = 2.0;
        const double x = -va / vt;
        dva = dp + G * sigmoid (x);
        return ip - G * vt * softplus (x);
    }

    inline double powerStage (double vgA, double vgB, bool commit, double* dA, double* dB, bool warmTrial = false)
    {
        const double vg2a = vScreen - kScreenR * ig2a, vg2b = vScreen - kScreenR * ig2b;
        double z0, h, zs, hs; load.thevenin (z0, h, zs, hs);
        double dkA, dkB;
        double ikA = tube.cathodeScaleD (vgA, vg2a, dkA), ikB = tube.cathodeScaleD (vgB, vg2b, dkB);
        ikA *= 2.0; ikB *= 2.0; dkA *= 2.0; dkB *= 2.0;              // two tubes per side
        double d = (commit || warmTrial) ? dTrial : 2.0 * dPrev - dPrev2, ia = 0, ib = 0, dpa = 0, dpb = 0;
        for (int it = 0; it < 30; ++it)
        {
            const double vh = z0 * d + h;
            ia = plateSide (ikA, vPlate - vh, dpa);
            ib = plateSide (ikB, vPlate + vh, dpb);
            const double f = ia - ib - d;
            const double df = -z0 * dpa - z0 * dpb - 1.0;
            const double stp = std::clamp (-f / df, -0.4, 0.4);
            d += stp;
            if (std::fabs (stp) < 1e-8) break;
        }
        const double vh = z0 * d + h;
        const double sk = 1.0 / std::sqrt (load.kRef);
        if (! commit)
        {
            dTrial = d;
            ia = plateSide (ikA, vPlate - vh, dpa);
            ib = plateSide (ikB, vPlate + vh, dpb);
            const double df = -z0 * dpa - z0 * dpb - 1.0;
            const double atA = std::atan (std::max (vPlate - vh, 0.0) / tube.kvb), atB = std::atan (std::max (vPlate + vh, 0.0) / tube.kvb);
            if (dA) *dA = zs * sk * (-(atA * dkA) / df);
            if (dB) *dB = zs * sk * ((atB * dkB) / df);
            return (zs * d + hs) * sk;
        }
        ia = plateSide (ikA, vPlate - vh, dpa);
        ib = plateSide (ikB, vPlate + vh, dpb);
        // screen current per tube
        const double ga = tube.screenFrom (0.5 * ikA, vgA, vg2a, vPlate - vh);
        const double gb = tube.screenFrom (0.5 * ikB, vgB, vg2b, vPlate + vh);
        load.commit (d);
        dPrev2 = dPrev; dPrev = d;
        ig2a = ga; ig2b = gb;
        lastVa = vPlate - vh; lastVgA = vgA; lastVgB = vgB;
        // supply: rectifier -> reservoir (A, plates) -> choke -> B (screens, preamp)
        const double vnlEff = vnl * (params.spongy ? 0.90 : 1.0);
        const double iR = rectCurrent (vnlEff - vPlate);
        const double iS = (vPlate - vScreen) / kChokeR;
        vPlate += T / cRes * (iR - (ia + ib) - iS);
        vScreen += T / cScr * (iS - 2.0 * (ga + gb) - iPre);
        return load.speakerVolts();
    }
};

} // namespace drect
