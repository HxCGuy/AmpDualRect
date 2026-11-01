// dsp_test.cpp - checks the amp model against the schematic's DC voltages (sheets mbdr1, mbdr2) in
// every channel mode, then prints small-signal gain and output for a 1 kHz tone per mode.
// build: g++ -O2 -std=c++17 -DDRECT_TAPS -Idsp tests/dsp_test.cpp -o dsp_test
#include "DualRect.h"
#include <cstdio>
#include <vector>
#include <chrono>

using namespace drect;

struct Stat { double sum = 0, sum2 = 0; int n = 0; void add (double v) { sum += v; sum2 += v * v; ++n; }
              double mean() const { return sum / n; } double ac() const { const double m = mean(); return std::sqrt (std::max (0.0, sum2 / n - m * m)); } };

int main()
{
    const double fs = 96000.0;
    struct Mode { const char* name; int ch, orMode, rdMode; };
    const Mode modes[] = { { "ORANGE CLN", 0, 0, 0 }, { "ORANGE NORM", 0, 1, 0 }, { "ORANGE MOD", 0, 2, 0 },
                           { "RED NORM", 1, 0, 0 }, { "RED VINT", 1, 0, 1 } };
    std::printf ("schematic DC: V1A 200 V / 1.6 V, V2A 280 / 2, V2B 384 / 5, V3A 213 / 1.6, V3B cathode 216, PI plates 280, tail 48 V\n");
    for (auto& m : modes)
    {
        DualRect amp;
        Params p; p.channel = m.ch; p.orMode = m.orMode; p.rdMode = m.rdMode;
        p.orGain = p.rdGain = 0.5; p.orMaster = p.rdMaster = 0.3;
        amp.setParams (p);
        const auto t0 = std::chrono::steady_clock::now();
        amp.prepare (fs);
        const auto t1 = std::chrono::steady_clock::now();
        Stat pl[4], ck[4], k3b, p3c, piK, piB, piA, vp;
        for (int i = 0; i < 4800; ++i)
        {
            amp.process (0.0);
            for (int s = 0; s < 4; ++s) { pl[s].add (amp.tap (s, 0)); ck[s].add (s < 3 ? amp.segCathode (s) : amp.segCathode (3)); }
            p3c.add (amp.tap (0, 2)); k3b.add (amp.tap (3, 1));
            piK.add (amp.paTap (0)); piB.add (amp.paTap (1)); piA.add (amp.paTap (2)); vp.add (amp.plateSupply());
        }
        std::printf ("\n=== %-12s (prepare %.0f ms)\n", m.name, std::chrono::duration<double, std::milli> (t1 - t0).count());
        const char* nm[4] = { "V1A", "V2A", "V2B", "V3A" };
        for (int s = 0; s < 4; ++s) std::printf ("  %s plate %6.1f V  cathode %5.2f V\n", nm[s], pl[s].mean(), ck[s].mean());
        std::printf ("  Mr.Scary plate %6.1f V   V3B cathode %6.1f V\n", p3c.mean(), k3b.mean());
        std::printf ("  PI cathodes %5.1f V  plates %6.1f / %6.1f V   B+ %.1f V  bias %.1f V\n", piK.mean(), piB.mean(), piA.mean(), vp.mean(), amp.biasVoltage());

        // 1 kHz, 10 mV rms: small-signal gain to V3A plate and bus
        const int settle = (int) (0.3 * fs), meas = (int) (0.2 * fs);
        Stat a3, bus, spk;
        const auto t2 = std::chrono::steady_clock::now();
        for (int i = 0; i < settle + meas; ++i)
        {
            const double x = 0.010 * std::sqrt (2.0) * std::sin (2.0 * M_PI * 1000.0 * i / fs);
            const double y = amp.process (x);
            if (i < settle) continue;
            a3.add (amp.tap (3, 0)); bus.add (amp.lastBus()); spk.add (y);
        }
        const auto t3 = std::chrono::steady_clock::now();
        std::printf ("  10 mV 1 kHz: V3A plate %.2f Vrms  bus %.3f Vrms  speaker %.2f Vrms (%.1f W)   %.2f us/sample\n",
                     a3.ac(), bus.ac(), spk.ac(), spk.ac() * spk.ac() / 16.0,
                     std::chrono::duration<double, std::micro> (t3 - t2).count() / (settle + meas));
    }
    return 0;
}
