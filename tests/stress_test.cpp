// stress_test.cpp - extreme inputs, knob sweeps and every switch combination: output must stay finite
// and bounded, and the circuit solver must converge.
#define private public
#include "DualRect.h"
#include <cstdio>
#include <random>
using namespace drect;
int main()
{
    const double fs = 96000.0;
    std::mt19937 rng (2); std::uniform_real_distribution<double> U (-1.0, 1.0);
    int bad = 0; double worst = 0; long fails = 0, fallbacks = 0, fbUnsolved = 0, samples = 0;
    for (int mode = 0; mode < 5; ++mode)
    for (int mods = 0; mods < 8; ++mods)
    {
        Params p; p.channel = mode < 3 ? 0 : 1; p.orMode = mode < 3 ? mode : 1; p.rdMode = mode == 4;
        p.tight = mods & 1; p.defizz = mods & 2; p.deep = mods & 4;
        p.el34 = (mode + mods) % 2; p.tubeRect = (mods / 2) % 2; p.spongy = mode % 2;
        DualRect amp; amp.setParams (p); amp.prepare (fs);
        double peak = 0;
        for (int i = 0; i < (int) (1.0 * fs); ++i)
        {
            const double t = i / fs;
            auto sw = [&] (double r) { return 0.5 + 0.5 * std::sin (2 * M_PI * r * t + mods); };
            if (i % 64 == 0)
            {
                Params q = p;
                q.orGain = sw (0.7); q.orTreble = sw (1.1); q.orMid = sw (1.3); q.orBass = sw (0.9); q.orPres = sw (0.5); q.orMaster = sw (0.45);
                q.rdGain = sw (0.6); q.rdTreble = sw (1.2); q.rdMid = sw (0.8); q.rdBass = sw (1.4); q.rdPres = sw (0.33); q.rdMaster = sw (0.27);
                q.scary = std::max (0.0, sw (0.8) * 1.2 - 0.2);          // crosses the click-out at 0
                amp.setParams (q);
            }
            double x;
            if (t < 0.25) x = 6.0 * U (rng);
            else if (t < 0.5) x = (std::sin (2 * M_PI * 82.0 * t) > 0 ? 3.0 : -3.0);
            else if (t < 0.75) x = (t < 0.6 ? 2.0 : -2.0);
            else x = 1.0 * std::sin (2 * M_PI * 110 * t) + 0.3 * U (rng);
            const double y = amp.process (x);
            ++samples;
            for (auto& s : amp.pre.segs) if (! s.net.lastConverged) ++fails;
            if (! std::isfinite (y)) { ++bad; break; }
            peak = std::max (peak, std::fabs (y));
        }
        // switch everything mid-stream
        p.channel ^= 1; p.tight = ! p.tight; p.defizz = ! p.defizz; p.el34 = ! p.el34; p.tubeRect = ! p.tubeRect; p.scary = 0.7;
        amp.setParams (p);
        for (int i = 0; i < (int) (0.2 * fs); ++i) { const double y = amp.process (0.5 * std::sin (2 * M_PI * 220 * i / fs)); if (! std::isfinite (y)) { ++bad; break; } peak = std::max (peak, std::fabs (y)); }
        for (auto& s : amp.pre.segs) fallbacks += s.net.fallbacks;
        fbUnsolved += amp.fbUnsolved;
        worst = std::max (worst, peak);
        if (mods == 0 || peak > 80.0 || bad) std::printf ("mode %d mods %d: peak %.1f V %s\n", mode, mods, peak, bad ? "NON-FINITE!" : "");
    }
    std::printf ("worst peak %.1f V (16 ohm), non-finite %d, unconverged preamp solves %ld of %ld, fallbacks %ld, PA loop unsolved %ld\n",
                 worst, bad, fails, samples * 4, fallbacks, fbUnsolved);
    return bad;
}
