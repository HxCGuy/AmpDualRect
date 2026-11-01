#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "DualRect.h"
#include "NoiseGate.h"

class AmpDualRectProcessor : public juce::AudioProcessor,
                             private juce::AsyncUpdater
{
public:
    AmpDualRectProcessor();
    ~AmpDualRectProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "AmpDualRect"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- cabinet IR (message thread) ----
    bool loadIR (const juce::File& file);
    void useDefaultIR();
    juce::String getIRName() const;

    juce::AudioProcessorValueTreeState apvts;

    // meters for the editor
    std::atomic<float> gateGainMeter { 1.0f }, outPeakMeter { 0.0f };
    std::atomic<bool> engineBusy { false };
    std::atomic<int> editorWidth { 1440 };       // remembered window width
    std::atomic<double> coreRateMeter { 0.0 };   // sample rate the amp model is currently running at
    std::atomic<bool> sleepingMeter { false };

    // level calibration: 1.0 FS at the plugin input = kInVoltsPerFS at the amp input jack,
    // speaker-terminal volts * kOutScale = FS at the plugin output
    static constexpr double kInVoltsPerFS = 1.0;
    static constexpr double kOutScale = 1.0 / 130.0;

private:
    struct Engine
    {
        drect::DualRect amp;
        int stages = 1;
        double coreRate = 96000.0;
    };
    class Builder;

    // the switch positions (everything that is changed under a short output duck)
    struct Switches
    {
        int channel = 1, orMode = 0, rdMode = 0;
        bool el34 = false, tubeRect = false, spongy = false, tight = false, defizz = false, deep = false, scaryOn = false, midShift = false;
        bool operator== (const Switches& o) const
        {
            return channel == o.channel && orMode == o.orMode && rdMode == o.rdMode && el34 == o.el34 && tubeRect == o.tubeRect
                && spongy == o.spongy && tight == o.tight && defizz == o.defizz && deep == o.deep && scaryOn == o.scaryOn
                && midShift == o.midShift;
        }
        bool operator!= (const Switches& o) const { return ! (*this == o); }
    };

    static int stagesFor (double hostRate, int quality);
    std::unique_ptr<Engine> buildEngine (double hostRate, int stages, const drect::Params& p) const;
    void requestEngine (int stages);
    void handleAsyncUpdate() override;
    void processSub (juce::AudioBuffer<float>&);
    void processAmpChunk (const float* in, float* out, int m);
    void applyAmpParams (bool force);
    Switches wantedSwitches() const;
    drect::Params makeParams (const Switches& s, const double* pots) const;
    drect::Params currentParams() const;         // parameter values as they are now (for building engines)

    // state --------------------------------------------------------------
    double hostRate = 48000.0;
    int maxBlock = 512;
    static constexpr int kChunk = 64;
    static constexpr int kNumPots = ids::numPots;

    std::unique_ptr<Engine> active, pending;
    bool pendingFresh = false;
    juce::SpinLock engineLock;
    std::unique_ptr<Builder> builder;
    std::atomic<int> requestedStages { -1 };
    std::atomic<double> requestedRate { 48000.0 };
    std::atomic<int> generation { 0 };
    std::atomic<int> latencyToReport { 0 };

    std::vector<std::unique_ptr<juce::dsp::Oversampling<float>>> oversamplers;   // index = stages (0 unused)
    std::atomic<float>* pInGain {}; std::atomic<float>* pGateOn {}; std::atomic<float>* pGateThr {}; std::atomic<float>* pGateDec {}; std::atomic<float>* pGateHpf {};
    std::atomic<float>* pChannel {}; std::atomic<float>* pOrMode {}; std::atomic<float>* pRdMode {};
    std::atomic<float>* pTubes {}; std::atomic<float>* pRect {}; std::atomic<float>* pPower {};
    std::atomic<float>* pTight {}; std::atomic<float>* pDefizz {}; std::atomic<float>* pDeep {}; std::atomic<float>* pMidShift {};
    std::atomic<float>* pPot[kNumPots] {};
    std::atomic<float>* pQuality {}; std::atomic<float>* pIrOn {}; std::atomic<float>* pLowCut {}; std::atomic<float>* pHighCut {}; std::atomic<float>* pOut {};

    p5150::NoiseGate gate;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGainSm, outGainSm;
    double potCur[kNumPots] {};
    drect::Params applied;
    Switches committed;                          // switch positions the amp currently runs with
    int duckState = 0; int holdLeft = 0;
    // silence sleep: the amp model is skipped while its input is (near) digital silence and its
    // output has decayed below -100 dBFS; it resumes on the first sample above -120 dBFS
    bool ampAsleep = false; int quietSamples = 0;

    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 256 } };
    juce::AudioBuffer<float> work, ampOut;
    std::vector<float> dryL;
    juce::SmoothedValue<float> irMix;
    bool irWasOn = true;
    juce::dsp::IIR::Filter<float> hp[2], lp[2];
    float lastLowCut = -1.0f, lastHighCut = -1.0f;

    juce::String irPath;      // empty = built-in synthetic cab
    juce::String irDisplay { "Built-in cab (synthetic)" };
    mutable juce::CriticalSection irLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpDualRectProcessor)
};
