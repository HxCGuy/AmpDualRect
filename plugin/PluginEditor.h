#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------------------------
// General look (dark UI panels) - same as Amp5150 III
class AmpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AmpLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// Faceplate look: silver brushed-aluminium plate with dark print, black ribbed knobs with a white
// pointer and a printed 0-10 scale, push-buttons with indicator LEDs.
class PanelLookAndFeel : public AmpLookAndFeel
{
public:
    static juce::Colour ink() { return juce::Colour (0xff141414); }
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
};

// a knob with a caption above it and a value readout underneath
class Knob : public juce::Component
{
public:
    enum class Style { Standard, Big, Panel };
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption,
          Style style = Style::Standard, juce::Colour accent = juce::Colour (0xffd9a441));
    void resized() override;
    void setDimmed (bool d) { slider.setAlpha (d ? 0.45f : 1.0f); caption.setAlpha (d ? 0.45f : 1.0f); }
    juce::Slider& getSlider() { return slider; }
    void useLookAndFeel (juce::LookAndFeel* l) { slider.setLookAndFeel (l); }
    void setInk (juce::Colour captionInk, juce::Colour valueInk)
    {
        caption.setColour (juce::Label::textColourId, captionInk);
        slider.setColour (juce::Slider::textBoxTextColourId, valueInk);
        repaint();
    }
private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label caption;
    juce::AudioProcessorValueTreeState::SliderAttachment attach;
};

// a chrome toggle switch with 2 or 3 positions (top = first choice), attached to a choice or bool
// parameter. Position labels are printed beside the lever; a click above / below the nut moves it.
class Lever : public juce::Component,
              public juce::SettableTooltipClient
{
public:
    Lever (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption,
           juce::StringArray positionLabels);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setDimmed (bool d) { dimmed = d; repaint(); }
private:
    juce::String caption;
    juce::StringArray labels;
    int position = 0;
    bool dimmed = false;
    std::unique_ptr<juce::ParameterAttachment> attach;
    juce::Rectangle<float> nutArea() const;
};

class LedMeter : public juce::Component
{
public:
    std::function<float()> source;
    float level = 0.0f;
    bool dbScale = false;
    void paint (juce::Graphics&) override;
};

// the front panel of the amp (silver plate, bezel, screws and printed group labels)
class FacePlate : public juce::Component
{
public:
    struct Group { juce::String title; juce::Rectangle<int> span; };
    std::vector<Group> groups;
    void paint (juce::Graphics&) override;
};

// ---------------------------------------------------------------------------------------------
// The complete interface at a fixed design size. The editor scales it to the window.
class AmpUi : public juce::Component,
              public juce::FileDragAndDropTarget,
              private juce::Timer
{
public:
    static constexpr int kW = 1520, kH = 640;
    explicit AmpUi (AmpDualRectProcessor&);
    ~AmpUi() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;
    void updateChannelDimming();
    void chooseIR();

    AmpDualRectProcessor& proc;
    AmpLookAndFeel laf;
    PanelLookAndFeel panelLaf;
    juce::Rectangle<int> inputArea, cabArea, plateArea, outArea;
    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CmbAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    FacePlate facePlate;
    juce::TooltipWindow tooltips { this, 700 };
    // input
    Knob kInGain, kGateThr, kGateDec, kGateHpf;
    juce::ToggleButton bGate { "GATE" };
    // amp faceplate
    juce::TextButton bOrange { "ORANGE" }, bRed { "RED" };
    juce::TextButton bTight { "TIGHT" }, bDefizz { "DE-FIZZ" }, bMidShift { "MID SHIFT" };
    Knob kOrGain, kOrTreble, kOrMid, kOrBass, kOrPres, kOrMaster;
    Knob kRdGain, kRdTreble, kRdMid, kRdBass, kRdPres, kRdMaster;
    Knob kScary;
    Lever lOrMode, lRdMode, lDeep, lRect, lPower, lTubes;
    // cab + output
    juce::ToggleButton bIr { "IR ON" };
    juce::TextButton bLoad { "LOAD IR..." }, bDefault { "DEFAULT" };
    juce::Label irName;
    Knob kLowCut, kHighCut, kOut;
    juce::ComboBox cbQuality;
    juce::Label qualityLabel, coreLabel, peakLabel, meterCaption;
    LedMeter gateLed, outMeter;

    std::unique_ptr<BtnAtt> aGate, aIr;
    std::unique_ptr<CmbAtt> aQuality;
    std::unique_ptr<juce::ParameterAttachment> aChannel, aTight, aDefizz, aMidShift;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<Knob*> orangeKnobs() { return { &kOrGain, &kOrTreble, &kOrMid, &kOrBass, &kOrPres, &kOrMaster }; }
    std::vector<Knob*> redKnobs()    { return { &kRdGain, &kRdTreble, &kRdMid, &kRdBass, &kRdPres, &kRdMaster }; }
    std::vector<Knob*> allPlateKnobs()
    {
        auto v = orangeKnobs(); for (auto* k : redKnobs()) v.push_back (k);
        v.push_back (&kScary); return v;
    }
    std::vector<juce::TextButton*> plateButtons() { return { &bOrange, &bRed, &bTight, &bDefizz, &bMidShift }; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpUi)
};

// ---------------------------------------------------------------------------------------------
// Resizable plugin window: hosts AmpUi and scales it, keeping the aspect ratio.
class AmpDualRectEditor : public juce::AudioProcessorEditor
{
public:
    explicit AmpDualRectEditor (AmpDualRectProcessor&);
    void resized() override;
private:
    AmpDualRectProcessor& proc;
    AmpUi ui;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpDualRectEditor)
};
