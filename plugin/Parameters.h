// Parameters.h - parameter IDs and layout shared by processor and editor
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
    // input section (before the amp) - same as Amp5150 III
    inline constexpr const char* inGain     = "inGain";      // dB
    inline constexpr const char* gateOn     = "gateOn";
    inline constexpr const char* gateThresh = "gateThresh";  // dB
    inline constexpr const char* gateDecay  = "gateDecay";   // ms
    inline constexpr const char* gateHpf    = "gateHpf";     // Hz (sidechain high-pass)
    // amp switches
    inline constexpr const char* channel    = "channel";     // 0 = ORANGE, 1 = RED
    inline constexpr const char* orMode     = "orMode";      // 0 = CLN, 1 = NORM, 2 = MOD
    inline constexpr const char* rdMode     = "rdMode";      // 0 = NORM, 1 = VINT
    inline constexpr const char* tubes      = "tubes";       // 0 = 6L6, 1 = EL34 (bias select)
    inline constexpr const char* rectifier  = "rectifier";   // 0 = silicon diodes, 1 = tube (2 x 5U4)
    inline constexpr const char* power      = "power";       // 0 = BOLD, 1 = SPONGY
    inline constexpr const char* tight      = "tight";       // mod: tight (V1A cathode + coupling caps)
    inline constexpr const char* defizz     = "defizz";      // mod: de-fizz
    inline constexpr const char* midShift   = "midShift";    // mod: tone-stack mid caps 20n -> 47n
    inline constexpr const char* scaryDeep  = "scaryDeep";   // Mr. Scary DEEP switch
    // amp pots
    inline constexpr const char* orGain     = "orGain";
    inline constexpr const char* orTreble   = "orTreble";
    inline constexpr const char* orMid      = "orMid";
    inline constexpr const char* orBass     = "orBass";
    inline constexpr const char* orPres     = "orPresence";
    inline constexpr const char* orMaster   = "orMaster";
    inline constexpr const char* rdGain     = "rdGain";
    inline constexpr const char* rdTreble   = "rdTreble";
    inline constexpr const char* rdMid      = "rdMid";
    inline constexpr const char* rdBass     = "rdBass";
    inline constexpr const char* rdPres     = "rdPresence";
    inline constexpr const char* rdMaster   = "rdMaster";
    inline constexpr const char* scaryGain  = "scaryGain";   // Mr. Scary GAIN (0 = stage switched out)
    inline constexpr const char* quality    = "quality";     // 0 = Normal (~96 kHz core), 1 = High (~192 kHz core), 2 = Eco (~48 kHz core)
    // cab / output (after the amp) - same as Amp5150 III
    inline constexpr const char* irOn       = "irOn";
    inline constexpr const char* lowCut     = "lowCut";      // Hz
    inline constexpr const char* highCut    = "highCut";     // Hz
    inline constexpr const char* outLevel   = "outLevel";    // dB

    // the 13 amp pots, in the order the processor keeps them
    inline constexpr int numPots = 13;
    inline constexpr const char* pots[numPots] = { orGain, orTreble, orMid, orBass, orPres, orMaster,
                                                   rdGain, rdTreble, rdMid, rdBass, rdPres, rdMaster, scaryGain };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using FP = AudioParameterFloat;
    using BP = AudioParameterBool;
    using CP = AudioParameterChoice;
    using PID = ParameterID;

    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dbText = [] (float v, int) { return String (v, 1) + " dB"; };
    auto hzText = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String ((int) std::round (v)) + " Hz"; };
    auto msText = [] (float v, int) { return String ((int) std::round (v)) + " ms"; };
    auto potText = [] (float v, int) { return String (v, 1); };

    p.push_back (std::make_unique<FP> (PID (ids::inGain, 1), "Input Gain", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<BP> (PID (ids::gateOn, 1), "Gate On", true));
    p.push_back (std::make_unique<FP> (PID (ids::gateThresh, 1), "Gate Threshold", NormalisableRange<float> (-90.0f, -20.0f, 0.1f), -60.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<FP> (PID (ids::gateDecay, 1), "Gate Decay", NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f), 120.0f, AudioParameterFloatAttributes().withStringFromValueFunction (msText)));
    p.push_back (std::make_unique<FP> (PID (ids::gateHpf, 1), "Gate Sidechain HPF", NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.5f), 110.0f, AudioParameterFloatAttributes().withStringFromValueFunction (hzText)));

    p.push_back (std::make_unique<CP> (PID (ids::channel, 1), "Channel", StringArray { "Orange", "Red" }, 1));
    p.push_back (std::make_unique<CP> (PID (ids::orMode, 1), "Orange Mode", StringArray { "Clean", "Normal", "Modern" }, 0));
    p.push_back (std::make_unique<CP> (PID (ids::rdMode, 1), "Red Mode", StringArray { "Normal", "Vintage" }, 0));
    p.push_back (std::make_unique<CP> (PID (ids::tubes, 1), "Power Tubes", StringArray { "6L6", "EL34" }, 0));
    p.push_back (std::make_unique<CP> (PID (ids::rectifier, 1), "Rectifier", StringArray { "Diode", "Tube" }, 0));
    p.push_back (std::make_unique<CP> (PID (ids::power, 1), "Power", StringArray { "Bold", "Spongy" }, 0));
    p.push_back (std::make_unique<BP> (PID (ids::tight, 1), "Tight Mod", false));
    p.push_back (std::make_unique<BP> (PID (ids::defizz, 1), "De-Fizz Mod", false));
    p.push_back (std::make_unique<BP> (PID (ids::midShift, 1), "Mid Shift Mod", false));
    p.push_back (std::make_unique<BP> (PID (ids::scaryDeep, 1), "Mr. Scary Deep", false));
    auto pot = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<FP> (PID (id, 1), name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def, AudioParameterFloatAttributes().withStringFromValueFunction (potText)));
    };
    pot (ids::orGain, "Orange Gain", 2.0f);
    pot (ids::orTreble, "Orange Treble", 6.0f);
    pot (ids::orMid, "Orange Mid", 5.0f);
    pot (ids::orBass, "Orange Bass", 5.0f);
    pot (ids::orPres, "Orange Presence", 5.0f);
    pot (ids::orMaster, "Orange Master", 1.5f);
    pot (ids::rdGain, "Red Gain", 6.5f);
    pot (ids::rdTreble, "Red Treble", 6.0f);
    pot (ids::rdMid, "Red Mid", 4.0f);
    pot (ids::rdBass, "Red Bass", 5.0f);
    pot (ids::rdPres, "Red Presence", 5.0f);
    pot (ids::rdMaster, "Red Master", 3.0f);
    pot (ids::scaryGain, "Mr. Scary Gain", 0.0f);
    p.push_back (std::make_unique<CP> (PID (ids::quality, 1), "Quality", StringArray { "Normal", "High", "Eco" }, 0));

    p.push_back (std::make_unique<BP> (PID (ids::irOn, 1), "IR On", true));
    p.push_back (std::make_unique<FP> (PID (ids::lowCut, 1), "Low Cut", NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.5f), 20.0f, AudioParameterFloatAttributes().withStringFromValueFunction (hzText)));
    p.push_back (std::make_unique<FP> (PID (ids::highCut, 1), "High Cut", NormalisableRange<float> (2000.0f, 20000.0f, 1.0f, 0.5f), 20000.0f, AudioParameterFloatAttributes().withStringFromValueFunction (hzText)));
    p.push_back (std::make_unique<FP> (PID (ids::outLevel, 1), "Output Level", NormalisableRange<float> (-40.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    return { p.begin(), p.end() };
}
