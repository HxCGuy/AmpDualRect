// NoiseGate.h - ISP Decimator-style noise gate: single threshold, very fast attack,
// adjustable decay, adjustable depth, hysteresis + hold so it never chatters on note tails,
// and a detector high-pass so low-frequency rumble does not hold the gate open.
#pragma once
#include <cmath>
#include <algorithm>

namespace p5150 {

class NoiseGate
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        setDetectorHz (detHz);
        envAtk = coeff (0.15); envRel = coeff (35.0);
        gAtk = coeff (0.25);
        holdSamples = (int) (0.006 * fs);
        setRelease (120.0); reset();
    }
    void reset() { env = 0; gain = 1; hpX = hpY = 0; open = true; hold = 0; }
    void setThresholdDb (double db) { thrOpen = std::pow (10.0, db / 20.0); thrClose = thrOpen * 0.55; }
    void setReleaseMs (double ms) { setRelease (ms); }
    void setDepthDb (double db) { floorGain = std::pow (10.0, -std::fabs (db) / 20.0); }
    void setEnabled (bool e) { enabled = e; }
    // sidechain (detector-only) high-pass: keeps low-frequency rumble / hum from holding the gate open
    void setDetectorHz (double hz) { detHz = hz; hpA = std::exp (-2.0 * 3.141592653589793 * std::clamp (hz, 10.0, 0.45 * fs) / fs); }

    inline float process (float x)
    {
        if (! enabled) { gain = 1; return x; }
        // detector: high-passed, rectified, fast-attack / slower-release peak follower
        const double hp = hpA * (hpY + (double) x - hpX); hpX = (double) x; hpY = hp;
        const double a = std::fabs (hp);
        env += (a > env ? (1.0 - envAtk) : (1.0 - envRel)) * (a - env);
        if (open) { if (env < thrClose) { if (--hold <= 0) open = false; } else hold = holdSamples; }
        else if (env > thrOpen) { open = true; hold = holdSamples; }
        const double target = open ? 1.0 : floorGain;
        const double c = target > gain ? gAtk : gRel;
        gain = target + (gain - target) * c;
        return (float) (x * gain);
    }
    double currentGain() const { return gain; }

private:
    double fs = 48000, detHz = 110.0, hpA = 0.99, envAtk = 0, envRel = 0, gAtk = 0, gRel = 0, hpX = 0, hpY = 0;
    double env = 0, gain = 1, thrOpen = 0.003, thrClose = 0.0017, floorGain = 0.01;
    int holdSamples = 288, hold = 0; bool open = true, enabled = true;
    double coeff (double ms) const { return std::exp (-1.0 / (0.001 * ms * fs)); }
    void setRelease (double ms) { gRel = coeff (std::max (ms, 1.0)); }
};

} // namespace p5150
