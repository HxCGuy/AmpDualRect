// pa_test.cpp - power amp: output power / THD / B+ sag for a 1 kHz tone at the master bus, per
// rectifier, BOLD/SPONGY, tube type and NFB setting; plus presence response with NFB on.
#include "DualRect.h"
#include <cstdio>
using namespace drect;

static void run (const char* tag, Params p, double vbus, double f = 1000.0, bool quiet = false)
{
    const double fs = 96000.0;
    DualRect amp; amp.setParams (p); amp.prepare (fs);
    const int settle = (int) (0.5 * fs), meas = (int) (0.3 * fs);
    double s2 = 0, re = 0, im = 0, bmin = 1e9, bsum = 0;
    for (int i = 0; i < settle + meas; ++i)
    {
        const double x = vbus * std::sqrt (2.0) * std::sin (2 * M_PI * f * i / fs);
        const double y = amp.processPA (x);
        if (i < settle) continue;
        s2 += y * y; re += y * std::cos (2 * M_PI * f * i / fs); im += y * std::sin (2 * M_PI * f * i / fs);
        bmin = std::min (bmin, amp.plateSupply()); bsum += amp.plateSupply();
    }
    const double rms = std::sqrt (s2 / meas), fund = std::sqrt (2.0) * std::hypot (re, im) / meas;
    const double thd = std::sqrt (std::max (0.0, rms * rms - fund * fund)) / fund;
    if (! quiet)
        std::printf ("%-28s in %5.0f mV %5.0f Hz -> %6.2f Vrms %6.1f W  THD %5.1f%%  B+ avg %5.1f min %5.1f  bias %.1f  unsolved %ld\n",
                     tag, vbus * 1000, f, rms, rms * rms / 16.0, thd * 100.0, bsum / meas, bmin, amp.biasVoltage(), amp.fbUnsolved);
    else
        std::printf ("    %5.0f Hz  %6.2f Vrms (%+.1f dB)\n", f, rms, 20 * std::log10 (rms));
}

int main()
{
    Params p; p.channel = 1; p.rdMode = 0;                     // RED NORM: no NFB
    std::printf ("--- RED NORM (no NFB), silicon, BOLD, 6L6\n");
    for (double v : { 0.2, 0.5, 1.0, 2.0, 4.0 }) run ("6L6 silicon bold", p, v);
    p.tubeRect = true;  for (double v : { 1.0, 4.0 }) run ("6L6 5U4 bold", p, v);
    p.spongy = true;    for (double v : { 1.0, 4.0 }) run ("6L6 5U4 spongy", p, v);
    p.tubeRect = false; for (double v : { 1.0, 4.0 }) run ("6L6 silicon spongy", p, v);
    p.spongy = false; p.el34 = true; for (double v : { 0.5, 1.0, 4.0 }) run ("EL34 silicon bold", p, v);
    p.el34 = false;
    std::printf ("--- NFB\n");
    p.rdMode = 1; for (double v : { 0.5, 1.0, 4.0 }) run ("RED VINT (NFB)", p, v);
    p.channel = 0; p.orMode = 0; for (double v : { 0.5, 1.0, 4.0 }) run ("ORANGE CLN (more NFB)", p, v);
    std::printf ("--- ORANGE NORM presence 0 / 0.5 / 1 (small signal)\n");
    p.orMode = 1;
    for (double pr : { 0.0, 0.5, 1.0 })
    {
        p.orPres = pr; std::printf ("  presence %.1f\n", pr);
        for (double f : { 100.0, 1000.0, 3000.0, 6000.0 }) run ("", p, 0.2, f, true);
    }
}
