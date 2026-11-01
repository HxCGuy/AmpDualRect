// DefaultCab.h - a small SYNTHETIC guitar-cabinet impulse response, generated from a handful of
// biquads, so the plugin sounds sensible before the user loads a real IR. It is a rough "4x12 with
// 12-inch speakers in a closed back" voicing, not a measurement of any real cabinet.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace defaultcab
{
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double tick (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void set (double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
        static Biquad lowpass (double fs, double f, double q)
        {
            Biquad b; const double w = 2 * juce::MathConstants<double>::pi * f / fs, c = std::cos (w), a = std::sin (w) / (2 * q);
            b.set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a); return b;
        }
        static Biquad highpass (double fs, double f, double q)
        {
            Biquad b; const double w = 2 * juce::MathConstants<double>::pi * f / fs, c = std::cos (w), a = std::sin (w) / (2 * q);
            b.set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a); return b;
        }
        static Biquad peak (double fs, double f, double q, double gainDb)
        {
            Biquad b; const double A = std::pow (10.0, gainDb / 40.0), w = 2 * juce::MathConstants<double>::pi * f / fs, c = std::cos (w), a = std::sin (w) / (2 * q);
            b.set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A); return b;
        }
    };

    inline juce::AudioBuffer<float> make (double fs)
    {
        const int len = (int) (0.040 * fs);
        Biquad f[] = { Biquad::highpass (fs, 80.0, 0.75),  Biquad::peak (fs, 115.0, 1.3, 4.0),  Biquad::peak (fs, 420.0, 1.0, -3.0),
                       Biquad::peak (fs, 2700.0, 1.6, 6.0), Biquad::peak (fs, 4200.0, 2.0, 2.5),
                       Biquad::lowpass (fs, 5600.0, 0.70), Biquad::lowpass (fs, 6200.0, 0.62), Biquad::lowpass (fs, 9000.0, 0.7) };
        juce::AudioBuffer<float> buf (1, len);
        auto* d = buf.getWritePointer (0);
        for (int i = 0; i < len; ++i)
        {
            double x = i == 0 ? 1.0 : 0.0;
            for (auto& s : f) x = s.tick (x);
            const double fade = i > len * 3 / 4 ? 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * (i - len * 3 / 4) / (double) (len - len * 3 / 4))) : 1.0;
            d[i] = (float) (x * fade);
        }
        return buf;
    }
}
