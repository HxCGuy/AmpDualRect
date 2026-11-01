// level_match.cpp - loudness (through a cab-like filter) of EL34 vs 6L6 per mode and setting, and of this
// amp against the Amp5150 III model (optional: compile with -DWITH_5150 and its dsp folder on the path).
#include "DualRect.h"
#ifdef WITH_5150
#include "Amp5150III.h"
#endif
#include <cstdio>
#include <vector>
#include <random>
struct BQ { double b0,b1,b2,a1,a2,z1=0,z2=0; double tick (double x) { double y = b0*x + z1; z1 = b1*x - a1*y + z2; z2 = b2*x - a2*y; return y; } };
static BQ mk (double fs, int type, double fc, double q, double g)
{
    BQ b; const double w = 2*M_PI*fc/fs, c = std::cos (w), al = std::sin (w)/(2*q); double B0,B1,B2,A0,A1,A2;
    if (type==0) { B0=(1-c)/2; B1=1-c; B2=(1-c)/2; A0=1+al; A1=-2*c; A2=1-al; }
    else if (type==1) { B0=(1+c)/2; B1=-(1+c); B2=(1+c)/2; A0=1+al; A1=-2*c; A2=1-al; }
    else { const double A=std::pow(10.0,g/40); B0=1+al*A; B1=-2*c; B2=1-al*A; A0=1+al/A; A1=-2*c; A2=1-al/A; }
    b.b0=B0/A0; b.b1=B1/A0; b.b2=B2/A0; b.a1=A1/A0; b.a2=A2/A0; return b;
}
static std::vector<double> makeDI (double fs, double secs)
{
    const int n = (int) (secs * fs); std::vector<double> x ((size_t) n, 0.0);
    std::mt19937 rng (7); std::uniform_real_distribution<double> U (-1, 1);
    auto pluck = [&] (double t0, double f, double a, double dec)
    {
        int st = (int) (t0 * fs), P = std::max (2, (int) std::lround (fs / f)); std::vector<double> l ((size_t) P); for (auto& v : l) v = U (rng);
        double prev = 0; for (int i = st, k = 0; i < n && k < (int) (1.5 * fs); ++i, ++k) { size_t j = (size_t) (k % P); double c = l[j]; double o = dec * 0.5 * (c + prev); prev = c; l[j] = o; x[(size_t) i] += a * c; }
    };
    for (int i = 0; i < 6; ++i) { pluck (0.25 * i, 82.41, 0.25, 0.996); pluck (0.25 * i, 123.47, 0.18, 0.996); }
    for (double f : { 82.41, 123.47, 164.81, 207.65, 246.94, 329.63 }) pluck (1.6, f, 0.12, 0.99925);
    return x;
}
template <class Amp>
static double loud (Amp& amp, const std::vector<double>& di, double fs, double* peak = nullptr)
{
    BQ f[] = { mk(fs,1,80,0.75,0), mk(fs,2,115,1.3,4), mk(fs,2,420,1.0,-3), mk(fs,2,2700,1.6,6), mk(fs,2,4200,2.0,2.5), mk(fs,0,5600,0.7,0), mk(fs,0,6200,0.62,0), mk(fs,0,9000,0.7,0) };
    double s2 = 0, pk = 0;
    for (double x : di) { double y = amp.process (x); pk = std::max (pk, std::fabs (y)); for (auto& b : f) y = b.tick (y); s2 += y * y; }
    if (peak) *peak = pk;
    return 10 * std::log10 (s2 / di.size());
}
int main (int argc, char** argv)
{
    using namespace drect;
    if (argc > 1) DualRect::kEl34Drive = std::pow (10.0, atof (argv[1]) / 20.0);
    const double fs = 96000; auto di = makeDI (fs, 3.0);
    struct M { const char* n; int ch, om, rm; };
    const M modes[] = { { "OR CLN ", 0, 0, 0 }, { "OR NORM", 0, 1, 0 }, { "RD NORM", 1, 0, 0 }, { "RD VINT", 1, 0, 1 } };
    std::printf ("mode     gain master |  6L6 dB   EL34 dB   EL34-6L6   (speaker peak 6L6)\n");
    for (auto& m : modes)
        for (double g : { 0.3, 0.7 })
            for (double v : { 0.15, 0.3, 0.6 })
            {
                Params p; p.channel = m.ch; p.orMode = m.om; p.rdMode = m.rm; p.orGain = p.rdGain = g; p.orMaster = p.rdMaster = v;
                DualRect a; a.setParams (p); a.prepare (fs);
                double pk; const double l6 = loud (a, di, fs, &pk);
                p.el34 = true; DualRect b; b.setParams (p); b.prepare (fs);
                const double le = loud (b, di, fs);
                std::printf ("%s  %.1f  %.2f   | %7.1f  %7.1f   %+5.1f      (%.0f V)\n", m.n, g, v, l6, le, le - l6, pk);
            }
#ifdef WITH_5150
    std::printf ("\nAmp5150 III reference (6L6):\n");
    for (int ch : { 0, 2 })
        for (double v : { 0.15, 0.3, 0.6 })
        {
            p5150iii::Amp5150III a; p5150iii::Params p; p.channel = ch; p.gain12 = p.gain3 = 0.6; p.vol12 = p.vol3 = v;
            a.setParams (p); a.prepare (fs);
            double pk; const double l = loud (a, di, fs, &pk);
            std::printf ("  ch%d gain 0.6 vol %.2f  %7.1f dB  (speaker peak %.0f V)\n", ch + 1, v, l, pk);
        }
#endif
}
