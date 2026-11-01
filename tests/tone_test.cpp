// tone_test.cpp - small-signal frequency responses at the master bus (tone controls, presence, TIGHT),
// Mr. Scary stage gain (its plate / V1A plate) vs knob and DEEP, and the DE-FIZZ effect on a saturated tone.
// build: g++ -O3 -std=c++17 -DDRECT_TAPS -Idsp tests/tone_test.cpp
#include "DualRect.h"
#include <cstdio>
#include <vector>
using namespace drect;

static double gainAt (DualRect& amp, double f, double vin, int which)
{
    const double fs = 96000.0;
    const int settle = (int) (0.15 * fs) + (int) (3.0 * fs / f), meas = std::max ((int) (0.05 * fs), (int) (10 * fs / f));
    double re = 0, im = 0, ri = 0, ii = 0;
    for (int i = 0; i < settle + meas; ++i)
    {
        const double ph = 2 * M_PI * f * i / fs;
        amp.process (vin * std::sin (ph));
        if (i < settle) continue;
        const double y = which == 0 ? amp.lastBus() : amp.tap (0, 2);       // Mr. Scary plate
        const double r = which == 0 ? vin * std::sin (ph) : amp.tap (0, 0);  // V1A plate
        re += y * std::cos (ph); im += y * std::sin (ph);
        ri += r * std::cos (ph); ii += r * std::sin (ph);
    }
    return 20 * std::log10 (std::hypot (re, im) / std::hypot (ri, ii));
}

static void sweep (const char* tag, Params p, double vin)
{
    DualRect amp; amp.setParams (p); amp.prepare (96000.0);
    std::printf ("%-34s", tag);
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 }) std::printf (" %6.1f", gainAt (amp, f, vin, 0));
    std::printf ("\n");
}

int main (int argc, char** argv)
{
    if (argc > 1) DualRect::kDefizzC = atof (argv[1]);
    // usage: tone_test [defizzC] [x = DE-FIZZ only | s = Mr. Scary only] [scaryCf]
    const bool onlyDefizz = argc > 2 && argv[2][0] == 'x', onlyScary = argc > 2 && argv[2][0] == 's';
    if (argc > 3) DualRect::kScaryCf = atof (argv[3]);
    Params p; p.channel = 1; p.rdGain = 0.0; p.rdMaster = 1.0;
    if (! onlyDefizz && ! onlyScary) {
    std::printf ("bus gain (dB re input)              50Hz   100    200    500    1k     2k     4k     8k\n");
    const double v = 2e-4;
    sweep ("RED NORM, all 5", p, v);
    { auto q = p; q.rdBass = 0; sweep ("  bass 0", q, v); q.rdBass = 1; sweep ("  bass 10", q, v); }
    { auto q = p; q.rdMid = 0; sweep ("  mid 0", q, v); q.rdMid = 1; sweep ("  mid 10", q, v); }
    { auto q = p; q.rdTreble = 0; sweep ("  treble 0", q, v); q.rdTreble = 1; sweep ("  treble 10", q, v); }
    { auto q = p; q.rdPres = 0; sweep ("  presence 0", q, v); q.rdPres = 1; sweep ("  presence 10", q, v); }
    { auto q = p; q.tight = true; sweep ("  TIGHT", q, v); }
    { auto q = p; q.defizz = true; sweep ("  DE-FIZZ", q, v); }
    { auto q = p; q.midShift = true; sweep ("  MID SHIFT, mid 5", q, v); q.rdMid = 0; sweep ("  MID SHIFT, mid 0", q, v); q.rdMid = 1; sweep ("  MID SHIFT, mid 10", q, v); }
    { auto q = p; q.rdGain = 0.5; sweep ("  gain 5 (small signal)", q, v * 0.05); }
    Params o; o.channel = 0; o.orMode = 1; o.orGain = 0.0; o.orMaster = 1.0;
    sweep ("ORANGE NORM, all 5", o, v);
    { auto q = o; q.orMode = 2; sweep ("ORANGE MOD", q, v); }
    { auto q = o; q.orMode = 0; sweep ("ORANGE CLN", q, v * 5); }
    { auto q = o; q.orMode = 0; q.orGain = 0.3; sweep ("ORANGE CLN gain 3", q, v * 5); }
    }

    std::printf ("\nMr. Scary stage gain, its plate / V1A plate (dB)   100Hz   1kHz   5kHz\n");
    for (int deep = 0; deep < 2; ++deep)
        for (double s : { 0.01, 0.25, 0.5, 0.75, 1.0 })
        if (! onlyDefizz)
        {
            Params q = p; q.scary = s; q.deep = deep;
            DualRect amp; amp.setParams (q); amp.prepare (96000.0);
            const double g100 = gainAt (amp, 100.0, 2e-5, 1), g1k = gainAt (amp, 1000.0, 2e-5, 1), g5k = gainAt (amp, 5000.0, 2e-5, 1);
            std::printf ("  %s gain %.2f                          %6.1f  %6.1f  %6.1f\n", deep ? "DEEP" : "    ", s, g100, g1k, g5k);
        }
    if (onlyScary) return 0;
    // DE-FIZZ: energy above 5 kHz at the bus with a saturating 220 Hz tone (RED, gain 7)
    std::printf ("\nDE-FIZZ, RED gain 7, 220 Hz 50 mV: energy 5-20 kHz relative to total (V3A plate / bus)\n");
    for (int sc = 0; sc < 2; ++sc)
    for (int df = 0; df < 2; ++df)
    for (int at = 0; at < 2; ++at)
    {
        Params q = p; q.rdGain = 0.7; q.rdMaster = 0.3; q.defizz = df; q.scary = sc ? 0.5 : 0.0;
        DualRect amp; amp.setParams (q); amp.prepare (96000.0);
        const double fs = 96000.0; const int N = 32768, settle = 9600;
        std::vector<double> y (N);
        for (int i = 0; i < settle + N; ++i) { amp.process (0.05 * std::sqrt (2.0) * std::sin (2 * M_PI * 220 * i / fs)); if (i >= settle) y[(size_t) (i - settle)] = at ? amp.lastBus() : amp.tap (3, 0); }
        // DFT bins over the band (Hann window)
        double hi = 0, all = 0;
        for (int k = 1; k < N / 2; k += 3)
        {
            const double f = k * fs / N; if (f > 20000) break;
            double re = 0, im = 0;
            for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / N); re += w * y[(size_t) i] * std::cos (2 * M_PI * k * i / N); im += w * y[(size_t) i] * std::sin (2 * M_PI * k * i / N); }
            const double pw = re * re + im * im; all += pw; if (f > 5000) hi += pw;
        }
        std::printf ("  scary %.1f defizz %s at %s: %.1f dB\n", q.scary, df ? "ON " : "off", at ? "bus      " : "V3A plate", 10 * std::log10 (hi / all));
    }
}
