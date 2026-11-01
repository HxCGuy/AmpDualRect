#include "PluginEditor.h"

namespace
{
    const juce::Colour bgTop    (0xff1d1b1a);
    const juce::Colour bgBottom (0xff0f0e0e);
    const juce::Colour panel    (0xff2a2725);
    const juce::Colour edge     (0xff433e3a);
    const juce::Colour textCol  (0xffe9e1d3);
    const juce::Colour dimText  (0xff9a9186);
    const juce::Colour gold     (0xffd9a441);

    // indicator LEDs
    constexpr juce::int64 ledOrange = 0xffff8a1c;
    constexpr juce::int64 ledRed    = 0xffe0281e;
    constexpr juce::int64 ledTight  = 0xff35c84a;   // green
    constexpr juce::int64 ledDefizz = 0xff2f7cf6;   // blue
    constexpr juce::int64 ledMidShift = 0xffb04dff; // violet

    juce::Font bold (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
    juce::Font plain (float size) { return juce::Font (juce::FontOptions (size)); }
}

//==============================================================================
AmpLookAndFeel::AmpLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, textCol);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, textCol);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, textCol);
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::ComboBox::arrowColourId, gold);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, textCol);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, edge);
    setColour (juce::TextButton::textColourOffId, textCol);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

juce::Font AmpLookAndFeel::getLabelFont (juce::Label& l)
{
    // value read-outs under the knobs: size is set per slider through the "valueFont" property
    if (auto* s = dynamic_cast<juce::Slider*> (l.getParentComponent()))
        return bold ((float) (double) s->getProperties().getWithDefault ("valueFont", 13.0));
    return l.getFont();
}

void AmpLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f - 5.0f;
    const auto c = bounds.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (float) i / 10.0f * (a1 - a0);
        const float r0 = r + 1.0f, r1 = r + (i % 5 == 0 ? 6.0f : 3.5f);
        g.setColour (dimText.withAlpha (0.8f));
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, 1.0f);
    }
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r - 1.0f, r - 1.0f, 0.0f, a0, ang, true);
    g.setColour (accent.withAlpha (0.9f));
    g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float rb = r - 5.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4541), c.x - rb * 0.4f, c.y - rb * 0.6f, juce::Colour (0xff161413), c.x + rb * 0.6f, c.y + rb, true));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
    g.setColour (edge.brighter (0.2f));
    g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);
    juce::Path p;
    p.addRoundedRectangle (-1.6f, -rb + 2.0f, 3.2f, rb * 0.5f, 1.2f);
    g.setColour (accent);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void AmpLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? gold.withAlpha (0.95f) : panel);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (hl ? gold : edge);
    g.drawRoundedRectangle (r, 5.0f, 1.2f);
    g.setColour (on ? juce::Colours::black : textCol);
    g.setFont (bold (12.5f));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void AmpLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? gold.withAlpha (0.95f) : (down ? edge : panel));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (hl ? gold : edge);
    g.drawRoundedRectangle (r, 5.0f, 1.2f);
}

//==============================================================================
// Faceplate look: black knobs on the silver plate
void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R  = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const float rk = R * 0.68f;                       // knob (skirt) radius
    const float rn = R * 0.90f;                       // radius of the printed numbers
    const float ang = a0 + pos * (a1 - a0);
    const auto print = ink();

    // printed scale: numbers 0..10 with small dots in between
    const float fs = juce::jlimit (8.0f, 14.0f, R * 0.18f);
    g.setFont (bold (fs));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (float) i / 10.0f * (a1 - a0);
        const float px = c.x + std::sin (a) * rn, py = c.y - std::cos (a) * rn;
        g.setColour (print.withAlpha (0.9f));
        g.drawText (juce::String (i), juce::Rectangle<float> (px - 12.0f, py - fs * 0.62f, 24.0f, fs * 1.24f), juce::Justification::centred);
        if (i < 10)
        {
            const float am = a + 0.5f / 10.0f * (a1 - a0);
            const float dx = c.x + std::sin (am) * rn, dy = c.y - std::cos (am) * rn;
            g.setColour (print.withAlpha (0.5f));
            g.fillEllipse (dx - 1.1f, dy - 1.1f, 2.2f, 2.2f);
        }
    }

    // shadow on the plate
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - rk + 1.5f, c.y - rk + 3.0f, rk * 2.0f, rk * 2.0f);

    // black skirt
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3d3d), c.x - rk * 0.55f, c.y - rk * 0.75f,
                                             juce::Colour (0xff050505), c.x + rk * 0.65f, c.y + rk, true));
    g.fillEllipse (c.x - rk, c.y - rk, rk * 2.0f, rk * 2.0f);
    const int ribs = 40;
    for (int k = 0; k < ribs; ++k)
    {
        const float a = (float) k / (float) ribs * juce::MathConstants<float>::twoPi;
        const float facing = std::max (0.0f, std::cos (a - juce::MathConstants<float>::pi * 1.25f));   // light from the upper left
        g.setColour (juce::Colours::white.withAlpha (0.08f + 0.22f * facing));
        g.drawLine (c.x + std::sin (a) * rk * 0.86f, c.y - std::cos (a) * rk * 0.86f, c.x + std::sin (a) * rk * 0.985f, c.y - std::cos (a) * rk * 0.985f, 1.3f);
    }
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - rk, c.y - rk, rk * 2.0f, rk * 2.0f, 1.2f);

    // cap with a small silver insert
    const float rc = rk * 0.76f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff595959), c.x - rc * 0.4f, c.y - rc * 0.55f,
                                             juce::Colour (0xff0b0b0b), c.x + rc * 0.35f, c.y + rc * 0.6f, true));
    g.fillEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f, 1.0f);
    const float ri = rc * 0.42f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f0), c.x - ri * 0.5f, c.y - ri * 0.6f,
                                             juce::Colour (0xff8a8a8a), c.x + ri * 0.5f, c.y + ri * 0.7f, true));
    g.fillEllipse (c.x - ri, c.y - ri, ri * 2.0f, ri * 2.0f);

    // white pointer line running from the cap out to the skirt edge
    juce::Path p;
    p.addRoundedRectangle (-rk * 0.055f, -rk * 0.97f, rk * 0.11f, rk * 0.5f, rk * 0.05f);
    g.setColour (juce::Colours::white);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xff101010) : juce::Colour (0xff2e2e2e), 0.0f, r.getY(),
                                             juce::Colour (0xff0a0a0a), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (hl ? juce::Colour (0xffffffff) : juce::Colour (0xff6a6a6a));
    g.drawRoundedRectangle (r, 4.0f, 1.5f);
    // indicator LED
    const auto ledCol = juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("led", (juce::int64) 0xffd02820));
    const float lr = 5.5f;
    const float lx = r.getX() + 15.0f, ly = r.getCentreY();
    if (on)
    {
        g.setColour (ledCol.withAlpha (0.28f)); g.fillEllipse (lx - lr * 2.0f, ly - lr * 2.0f, lr * 4.0f, lr * 4.0f);
        g.setGradientFill (juce::ColourGradient (ledCol.brighter (0.9f), lx - 1.5f, ly - 2.0f, ledCol.darker (0.2f), lx + lr, ly + lr, true));
    }
    else g.setColour (ledCol.darker (2.6f));
    g.fillEllipse (lx - lr, ly - lr, lr * 2.0f, lr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (lx - lr, ly - lr, lr * 2.0f, lr * 2.0f, 1.0f);
}

// push-button caption: centred in the space to the right of the indicator LED
void PanelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    r.removeFromLeft (26.0f);                    // the LED sits at x + 15 with a 5.5 px radius
    g.setColour (juce::Colour (0xffe8e8e8).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.setFont (bold (juce::jmin (14.0f, b.getHeight() * 0.42f)));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1, 0.8f);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& cap, Style style, juce::Colour accent)
    : attach (apvts, id, slider)
{
    float valueFont = 13.0f; int tbW = 70, tbH = 16, capSize = 12;
    if (style == Style::Big)   { valueFont = 19.0f; tbW = 110; tbH = 26; capSize = 13; }
    if (style == Style::Panel) { valueFont = 12.5f; tbW = 56;  tbH = 18; capSize = 11; }
    slider.getProperties().set ("valueFont", (double) valueFont);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, tbW, tbH);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setDoubleClickReturnValue (true, apvts.getParameter (id)->convertFrom0to1 (apvts.getParameter (id)->getDefaultValue()));
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    caption.setText (cap, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setFont (bold ((float) capSize));
    caption.setColour (juce::Label::textColourId, style == Style::Panel ? PanelLookAndFeel::ink() : dimText);
    addAndMakeVisible (caption);
    if (style == Style::Panel) slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff2b2b2b));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

//==============================================================================
Lever::Lever (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& cap, juce::StringArray positionLabels)
    : caption (cap), labels (std::move (positionLabels))
{
    attach = std::make_unique<juce::ParameterAttachment> (*apvts.getParameter (id), [this] (float v)
    {
        position = juce::jlimit (0, labels.size() - 1, (int) std::lround (v));
        repaint();
    });
    attach->sendInitialUpdate();
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> Lever::nutArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (18.0f);
    return r;
}

void Lever::mouseDown (const juce::MouseEvent& e)
{
    const int n = labels.size();
    int target = position;
    if (n == 2) target = 1 - position;
    else
    {
        const auto c = nutArea().getCentre();
        if (e.position.y < c.y - 4.0f) target = std::max (0, position - 1);
        else if (e.position.y > c.y + 4.0f) target = std::min (n - 1, position + 1);
    }
    if (target != position) attach->setValueAsCompleteGesture ((float) target);
}

void Lever::paint (juce::Graphics& g)
{
    const float a = dimmed ? 0.45f : 1.0f;
    const auto print = PanelLookAndFeel::ink();
    auto r = getLocalBounds().toFloat();
    g.setColour (print.withAlpha (0.95f * a));
    g.setFont (bold (11.0f));
    g.drawText (caption, r.removeFromTop (18.0f), juce::Justification::centred);

    const int n = labels.size();
    const float R = 13.0f;
    float nutX, cy = r.getCentreY();
    if (n == 2)
    {
        // two positions: labels printed above and below the nut (the selected one in full ink)
        nutX = r.getCentreX();
        g.setFont (bold (10.5f));
        for (int i = 0; i < 2; ++i)
        {
            const float ly = cy + (i == 0 ? -33.0f : 33.0f);
            g.setColour (print.withAlpha ((i == position ? 1.0f : 0.38f) * a));
            g.drawText (labels[i], juce::Rectangle<float> (r.getX() - 4.0f, ly - 7.0f, r.getWidth() + 8.0f, 14.0f), juce::Justification::centred);
        }
    }
    else
    {
        // three positions: labels in a column to the right of the nut
        nutX = r.getX() + R + 4.0f;
        const float spread = 22.0f;
        g.setFont (bold (10.5f));
        for (int i = 0; i < n; ++i)
        {
            const float ly = cy + (float) (i - 1) * spread;
            g.setColour (print.withAlpha ((i == position ? 1.0f : 0.38f) * a));
            g.drawText (labels[i], juce::Rectangle<float> (nutX + R + 3.0f, ly - 7.0f, r.getRight() - (nutX + R + 3.0f), 14.0f), juce::Justification::centredLeft);
        }
    }

    // mounting nut
    const juce::Point<float> c (nutX, cy);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f0).withAlpha (a), c.x - R * 0.5f, c.y - R * 0.7f,
                                             juce::Colour (0xff6c6c6c).withAlpha (a), c.x + R * 0.6f, c.y + R * 0.8f, true));
    juce::Path hex;
    hex.addPolygon (c, 6, R, juce::MathConstants<float>::pi / 6.0f);
    g.fillPath (hex);
    g.setColour (juce::Colour (0xff2a2a2a).withAlpha (a));
    g.strokePath (hex, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colour (0xff101010).withAlpha (a));
    g.fillEllipse (c.x - R * 0.42f, c.y - R * 0.42f, R * 0.84f, R * 0.84f);

    // lever: up (first position), straight out (middle of three: drawn short), down (last)
    float dir = 0.0f;
    if (n == 3) dir = position == 0 ? -1.0f : (position == 2 ? 1.0f : 0.0f);
    else dir = position == 0 ? -1.0f : 1.0f;
    const float len = dir == 0.0f ? 0.0f : R * 1.25f * dir;
    const juce::Point<float> tip (c.x, c.y + len);
    if (dir != 0.0f)
    {
        g.setColour (juce::Colour (0xff3a3a3a).withAlpha (a));
        g.drawLine (c.x + 1.5f, c.y + 1.5f, tip.x + 1.5f, tip.y + 1.5f, 5.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f2).withAlpha (a), c.x - 3.0f, c.y, juce::Colour (0xff8a8a8a).withAlpha (a), c.x + 3.0f, c.y, false));
        g.drawLine (c.x, c.y, tip.x, tip.y, 4.5f);
    }
    const float tr = dir == 0.0f ? 5.5f : 4.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (a), tip.x - 2.0f, tip.y - 2.0f, juce::Colour (0xff7a7a7a).withAlpha (a), tip.x + 3.0f, tip.y + 3.0f, true));
    g.fillEllipse (tip.x - tr, tip.y - tr, tr * 2.0f, tr * 2.0f);
    g.setColour (juce::Colour (0xff3a3a3a).withAlpha (0.6f * a));
    g.drawEllipse (tip.x - tr, tip.y - tr, tr * 2.0f, tr * 2.0f, 0.8f);
    if (isMouseOver()) { g.setColour (print.withAlpha (0.25f)); g.drawEllipse (c.x - R - 3.0f, c.y - R - 3.0f, 2 * R + 6.0f, 2 * R + 6.0f, 1.0f); }
}

//==============================================================================
void LedMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto bar = r;
    if (dbScale) bar = bar.removeFromTop (bar.getHeight() - 16.0f);
    g.setColour (juce::Colour (0xff0a0909));
    g.fillRoundedRectangle (bar, 3.0f);
    const float v = juce::jlimit (0.0f, 1.0f, level);
    auto fill = bar.reduced (2.0f);
    const float fullW = fill.getWidth();
    fill.setWidth (fullW * v);
    juce::ColourGradient grad (juce::Colour (0xff3fbf5a), fill.getX(), 0.0f, juce::Colour (0xffe03a2e), fill.getX() + fullW, 0.0f, false);
    grad.addColour (0.70, juce::Colour (0xff3fbf5a));
    grad.addColour (0.88, juce::Colour (0xffe3c230));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, 2.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (bar, 3.0f, 1.0f);
    if (dbScale)
    {
        g.setFont (plain (11.0f));
        for (int db : { -60, -48, -36, -24, -12, 0 })
        {
            const float x = bar.getX() + 2.0f + fullW * (float) (db + 60) / 60.0f;
            g.setColour (dimText);
            g.drawLine (x, bar.getBottom(), x, bar.getBottom() + 3.0f, 1.0f);
            const float lx = juce::jlimit (r.getX(), r.getRight() - 28.0f, x - 14.0f);
            g.drawText (juce::String (db), juce::Rectangle<float> (lx, bar.getBottom() + 2.0f, 28.0f, 13.0f), juce::Justification::centred);
        }
    }
}

//==============================================================================
void FacePlate::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto print = PanelLookAndFeel::ink();
    // silver brushed aluminium with a soft vertical light falloff
    juce::ColourGradient base (juce::Colour (0xffe6e7e8), 0.0f, 0.0f, juce::Colour (0xffb4b6b9), 0.0f, r.getBottom(), false);
    base.addColour (0.45, juce::Colour (0xffd3d5d7));
    g.setGradientFill (base);
    g.fillRoundedRectangle (r, 6.0f);
    juce::Random rnd (1992);
    for (int i = 0; i < 900; ++i)           // brushed grain: many fine horizontal strokes, light and dark
    {
        const float y = rnd.nextFloat() * r.getHeight(), x = rnd.nextFloat() * r.getWidth() - 60.0f, len = 60.0f + rnd.nextFloat() * 260.0f;
        const bool light = rnd.nextBool();
        const float al = 0.03f + rnd.nextFloat() * 0.05f;
        g.setColour (light ? juce::Colours::white.withAlpha (al * 1.4f) : juce::Colours::black.withAlpha (al * 0.6f));
        g.drawLine (x, y, x + len, y, 0.8f);
    }
    // broad diagonal sheen
    {
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), r.getWidth() * 0.15f, 0.0f, juce::Colours::white.withAlpha (0.0f), r.getWidth() * 0.85f, r.getHeight(), false);
        sheen.addColour (0.5, juce::Colours::white.withAlpha (0.18f));
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (r, 6.0f);
    }
    g.setColour (juce::Colour (0xff8d8f92));
    g.drawRoundedRectangle (r.reduced (0.75f), 6.0f, 1.5f);
    g.setColour (juce::Colour (0xfff7f7f7).withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (3.5f), 4.0f, 1.0f);

    // corner screws
    for (auto pt : { juce::Point<float> (11.0f, 11.0f), juce::Point<float> (r.getRight() - 11.0f, 11.0f),
                     juce::Point<float> (11.0f, r.getBottom() - 11.0f), juce::Point<float> (r.getRight() - 11.0f, r.getBottom() - 11.0f) })
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f2), pt.x - 3.0f, pt.y - 3.0f, juce::Colour (0xff5a5a5a), pt.x + 4.0f, pt.y + 4.0f, true));
        g.fillEllipse (pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff2a2a2a));
        const float a = (pt.x * 7.0f + pt.y * 3.0f);
        g.drawLine (pt.x - std::cos (a) * 3.6f, pt.y - std::sin (a) * 3.6f, pt.x + std::cos (a) * 3.6f, pt.y + std::sin (a) * 3.6f, 1.4f);
    }

    // printed group labels with bracket rules
    for (auto& grp : groups)
    {
        const auto s = grp.span.toFloat();
        const float y = s.getY() + 9.0f;
        const juce::Font labelFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.12f));
        g.setFont (labelFont);
        juce::GlyphArrangement ga;
        ga.addLineOfText (labelFont, grp.title, 0.0f, 0.0f);
        const float tw = ga.getBoundingBox (0, -1, true).getWidth() + 14.0f;
        const float cx = s.getCentreX();
        g.setColour (print.withAlpha (0.9f));
        g.drawLine (s.getX(), y, cx - tw * 0.5f, y, 1.3f);
        g.drawLine (cx + tw * 0.5f, y, s.getRight(), y, 1.3f);
        g.drawLine (s.getX(), y, s.getX(), y + 6.0f, 1.3f);
        g.drawLine (s.getRight(), y, s.getRight(), y + 6.0f, 1.3f);
        g.drawText (grp.title, juce::Rectangle<float> (cx - tw * 0.5f, y - 8.0f, tw, 16.0f), juce::Justification::centred);
    }
}

//==============================================================================
AmpUi::AmpUi (AmpDualRectProcessor& p)
    : proc (p),
      kInGain (p.apvts, ids::inGain, "INPUT GAIN", Knob::Style::Big, juce::Colour (0xff6fb3d9)),
      kGateThr (p.apvts, ids::gateThresh, "THRESHOLD", Knob::Style::Standard, juce::Colour (0xff6fb3d9)),
      kGateDec (p.apvts, ids::gateDecay, "DECAY", Knob::Style::Standard, juce::Colour (0xff6fb3d9)),
      kGateHpf (p.apvts, ids::gateHpf, "SC HPF", Knob::Style::Standard, juce::Colour (0xff6fb3d9)),
      kOrGain (p.apvts, ids::orGain, "GAIN", Knob::Style::Panel), kOrTreble (p.apvts, ids::orTreble, "TREBLE", Knob::Style::Panel),
      kOrMid (p.apvts, ids::orMid, "MID", Knob::Style::Panel), kOrBass (p.apvts, ids::orBass, "BASS", Knob::Style::Panel),
      kOrPres (p.apvts, ids::orPres, "PRESENCE", Knob::Style::Panel), kOrMaster (p.apvts, ids::orMaster, "MASTER", Knob::Style::Panel),
      kRdGain (p.apvts, ids::rdGain, "GAIN", Knob::Style::Panel), kRdTreble (p.apvts, ids::rdTreble, "TREBLE", Knob::Style::Panel),
      kRdMid (p.apvts, ids::rdMid, "MID", Knob::Style::Panel), kRdBass (p.apvts, ids::rdBass, "BASS", Knob::Style::Panel),
      kRdPres (p.apvts, ids::rdPres, "PRESENCE", Knob::Style::Panel), kRdMaster (p.apvts, ids::rdMaster, "MASTER", Knob::Style::Panel),
      kScary (p.apvts, ids::scaryGain, "GAIN", Knob::Style::Panel),
      lOrMode (p.apvts, ids::orMode, "MODE", { "CLN", "NORM", "MOD" }),
      lRdMode (p.apvts, ids::rdMode, "MODE", { "NORM", "VINT" }),
      lDeep (p.apvts, ids::scaryDeep, "BASS", { "NORM", "DEEP" }),
      lRect (p.apvts, ids::rectifier, "RECTIFIER", { "DIODE", "TUBE" }),
      lPower (p.apvts, ids::power, "POWER", { "BOLD", "SPONGY" }),
      lTubes (p.apvts, ids::tubes, "TUBES", { "6L6", "EL34" }),
      kLowCut (p.apvts, ids::lowCut, "LOW CUT", Knob::Style::Big, juce::Colour (0xff8fd18a)),
      kHighCut (p.apvts, ids::highCut, "HIGH CUT", Knob::Style::Big, juce::Colour (0xff8fd18a)),
      kOut (p.apvts, ids::outLevel, "OUTPUT", Knob::Style::Big, juce::Colour (0xff8fd18a))
{
    setLookAndFeel (&laf);
    addAndMakeVisible (facePlate);
    for (Component* c : std::initializer_list<Component*> { &kInGain, &kGateThr, &kGateDec, &kGateHpf, &bGate,
                                                          &bOrange, &bRed, &bTight, &bDefizz, &bMidShift,
                                                          &lOrMode, &lRdMode, &lDeep, &lRect, &lPower, &lTubes,
                                                          &bIr, &bLoad, &bDefault, &irName, &kLowCut, &kHighCut, &kOut, &cbQuality, &qualityLabel, &coreLabel,
                                                          &peakLabel, &meterCaption, &gateLed, &outMeter })
        addAndMakeVisible (c);
    for (Knob* k : allPlateKnobs()) { addAndMakeVisible (k); k->useLookAndFeel (&panelLaf); }
    for (auto* b : plateButtons()) { b->setLookAndFeel (&panelLaf); b->setClickingTogglesState (false); }
    bOrange.getProperties().set ("led", ledOrange);
    bRed.getProperties().set ("led", ledRed);
    bTight.getProperties().set ("led", ledTight);
    bDefizz.getProperties().set ("led", ledDefizz);
    bMidShift.getProperties().set ("led", ledMidShift);
    bMidShift.setTooltip ("Mid shift: tone-stack mid caps 20n -> 47n, so the MID knob reaches the 150-500 Hz low mids");
    bTight.setTooltip ("Tight: V1A cathode cap 1u -> 470n and coupling cap 20n -> 2n2");
    bDefizz.setTooltip ("De-fizz: 470p across V3A's plate load and a 22k grid stopper on the cathode follower");
    lDeep.setTooltip ("Mr. Scary bass: DEEP lowers the stage's low cut from ~72 Hz to ~7 Hz");
    kScary.getSlider().setTooltip ("Mr. Scary gain: 0 = stage out (stock amp), then from about unity up to about +22 dB");

    aGate = std::make_unique<BtnAtt> (p.apvts, ids::gateOn, bGate);
    aIr   = std::make_unique<BtnAtt> (p.apvts, ids::irOn, bIr);

    cbQuality.addItem ("Normal  (~96 kHz core)", 1);
    cbQuality.addItem ("High  (~192 kHz core)", 2);
    cbQuality.addItem ("Eco  (~48 kHz core, lighter CPU)", 3);
    aQuality = std::make_unique<CmbAtt> (p.apvts, ids::quality, cbQuality);
    qualityLabel.setText ("QUALITY", juce::dontSendNotification);
    qualityLabel.setFont (bold (12.0f));
    qualityLabel.setColour (juce::Label::textColourId, dimText);
    coreLabel.setFont (plain (12.0f));
    coreLabel.setColour (juce::Label::textColourId, dimText);

    // channel select: two exclusive buttons driving one choice parameter
    aChannel = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (ids::channel), [this] (float v)
    {
        const int ch = juce::jlimit (0, 1, (int) std::lround (v));
        bOrange.setToggleState (ch == 0, juce::dontSendNotification);
        bRed.setToggleState (ch == 1, juce::dontSendNotification);
        updateChannelDimming();
    });
    bOrange.onClick = [this] { aChannel->setValueAsCompleteGesture (0.0f); };
    bRed.onClick    = [this] { aChannel->setValueAsCompleteGesture (1.0f); };

    // mods: latching push-buttons with their own LEDs
    aTight = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (ids::tight), [this] (float v) { bTight.setToggleState (v > 0.5f, juce::dontSendNotification); bTight.repaint(); });
    aDefizz = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (ids::defizz), [this] (float v) { bDefizz.setToggleState (v > 0.5f, juce::dontSendNotification); bDefizz.repaint(); });
    bTight.onClick  = [this] { aTight->setValueAsCompleteGesture (bTight.getToggleState() ? 0.0f : 1.0f); };
    bDefizz.onClick = [this] { aDefizz->setValueAsCompleteGesture (bDefizz.getToggleState() ? 0.0f : 1.0f); };
    aMidShift = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (ids::midShift), [this] (float v) { bMidShift.setToggleState (v > 0.5f, juce::dontSendNotification); bMidShift.repaint(); });
    bMidShift.onClick = [this] { aMidShift->setValueAsCompleteGesture (bMidShift.getToggleState() ? 0.0f : 1.0f); };

    bLoad.onClick = [this] { chooseIR(); };
    bDefault.onClick = [this] { proc.useDefaultIR(); irName.setText (proc.getIRName(), juce::dontSendNotification); };
    irName.setText (proc.getIRName(), juce::dontSendNotification);
    irName.setColour (juce::Label::textColourId, dimText);
    irName.setFont (plain (12.0f));
    irName.setJustificationType (juce::Justification::centredLeft);

    gateLed.source = [this] { return 1.0f - proc.gateGainMeter.load(); };
    outMeter.dbScale = true;
    outMeter.source = [this] { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (proc.outPeakMeter.load(), -60.0f) + 60.0f) / 60.0f); };
    meterCaption.setText ("OUTPUT LEVEL  (dBFS)", juce::dontSendNotification);
    meterCaption.setFont (bold (12.0f));
    meterCaption.setColour (juce::Label::textColourId, dimText);
    peakLabel.setFont (bold (24.0f));
    peakLabel.setJustificationType (juce::Justification::centredRight);
    peakLabel.setColour (juce::Label::textColourId, textCol);

    setSize (kW, kH);
    aChannel->sendInitialUpdate();
    aTight->sendInitialUpdate();
    aDefizz->sendInitialUpdate();
    aMidShift->sendInitialUpdate();
    startTimerHz (24);
}

AmpUi::~AmpUi()
{
    stopTimer();
    for (Knob* k : allPlateKnobs()) k->useLookAndFeel (nullptr);
    for (auto* b : plateButtons()) b->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AmpUi::updateChannelDimming()
{
    const bool red = bRed.getToggleState();
    for (Knob* k : orangeKnobs()) k->setDimmed (red);
    for (Knob* k : redKnobs()) k->setDimmed (! red);
    lOrMode.setDimmed (red);
    lRdMode.setDimmed (! red);
}

void AmpUi::timerCallback()
{
    gateLed.level = gateLed.source();
    gateLed.repaint();
    outMeter.level = outMeter.source();
    outMeter.repaint();
    proc.gateGainMeter = 1.0f;     // re-armed by the audio thread each block

    const float pk = proc.outPeakMeter.load();
    if (pk < 0.001f) peakLabel.setText ("-inf", juce::dontSendNotification);
    else peakLabel.setText (juce::String (juce::Decibels::gainToDecibels (pk), 1) + " dB", juce::dontSendNotification);
    peakLabel.setColour (juce::Label::textColourId, pk >= 1.0f ? juce::Colour (0xffe8503a) : textCol);

    if (proc.engineBusy.load()) coreLabel.setText ("rebuilding amp model...", juce::dontSendNotification);
    else if (proc.sleepingMeter.load()) coreLabel.setText ("amp idle (silent input)  /  latency " + juce::String (proc.getLatencySamples()) + " smp", juce::dontSendNotification);
    else coreLabel.setText ("core " + juce::String (proc.coreRateMeter.load() / 1000.0, 1) + " kHz  /  latency " + juce::String (proc.getLatencySamples()) + " smp", juce::dontSendNotification);
    const auto n = proc.getIRName();
    if (irName.getText() != n) irName.setText (n, juce::dontSendNotification);
}

void AmpUi::chooseIR()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a cabinet impulse response", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile()) { proc.loadIR (f); irName.setText (proc.getIRName(), juce::dontSendNotification); }
        });
}

bool AmpUi::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac")) return true;
    return false;
}

void AmpUi::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac")) { proc.loadIR (juce::File (f)); irName.setText (proc.getIRName(), juce::dontSendNotification); break; }
}

//==============================================================================
void AmpUi::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0, 0, bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    auto section = [&] (juce::Rectangle<int> r, const juce::String& title, juce::Colour accent)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (accent);
        g.setFont (bold (13.0f));
        g.drawText (title, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 18, juce::Justification::centredLeft);
    };
    section (inputArea, "INPUT  /  NOISE GATE", juce::Colour (0xff6fb3d9));
    section (cabArea, "CABINET  (IR LOADER)", juce::Colour (0xff8fd18a));
    section (outArea, "OUTPUT", juce::Colour (0xff8fd18a));

    g.setColour (textCol);
    g.setFont (bold (22.0f));
    g.drawText ("AMP DUALRECT", 16, 10, 400, 28, juce::Justification::centredLeft);
    g.setColour (dimText);
    g.setFont (plain (12.0f));
    g.drawText ("circuit-level 100 W two-channel rectifier tube amp model  /  Mr. Scary, tight, de-fizz and mid shift mods", 16, 36, 760, 16, juce::Justification::centredLeft);
}

void AmpUi::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (38);

    auto row1 = area.removeFromTop (160);
    inputArea = row1.removeFromLeft (470);
    row1.removeFromLeft (10);
    cabArea = row1;
    area.removeFromTop (10);
    plateArea = area.removeFromTop (204);
    area.removeFromTop (10);
    outArea = area;

    // ---- input section: input gain is the big knob
    {
        auto r = inputArea.reduced (10); r.removeFromTop (22);
        auto left = r.removeFromLeft (76);
        bGate.setBounds (left.removeFromTop (28).reduced (2));
        gateLed.setBounds (left.removeFromTop (14).reduced (4, 2));
        kInGain.setBounds (r.removeFromLeft (140));
        const int w = r.getWidth() / 3;
        kGateThr.setBounds (r.removeFromLeft (w)); kGateDec.setBounds (r.removeFromLeft (w)); kGateHpf.setBounds (r);
    }
    // ---- cab section
    {
        auto r = cabArea.reduced (12); r.removeFromTop (22);
        auto top = r.removeFromTop (34);
        bIr.setBounds (top.removeFromLeft (90).reduced (2));
        top.removeFromLeft (8);
        bLoad.setBounds (top.removeFromLeft (110).reduced (2));
        bDefault.setBounds (top.removeFromLeft (90).reduced (2));
        r.removeFromTop (8);
        irName.setBounds (r.removeFromTop (22));
        r.removeFromTop (8);
        auto q = r.removeFromTop (26);
        qualityLabel.setBounds (q.removeFromLeft (70));
        cbQuality.setBounds (q.removeFromLeft (220));
        r.removeFromTop (6);
        coreLabel.setBounds (r.removeFromTop (20));
    }
    // ---- amp faceplate
    {
        facePlate.setBounds (plateArea);
        auto r = plateArea.withZeroOrigin().reduced (18, 10);       // plate-local, inside the bezel
        const int top = 36;                                            // first row below the group labels
        const int ox = plateArea.getX(), oy = plateArea.getY();
        std::vector<FacePlate::Group> groups;

        // CHANNEL buttons
        auto bank = r.removeFromLeft (112);
        r.removeFromLeft (14);
        bOrange.setBounds (ox + bank.getX(), oy + top + 8,  bank.getWidth(), 38);
        bRed.setBounds    (ox + bank.getX(), oy + top + 62, bank.getWidth(), 38);
        groups.push_back ({ "CHANNEL", juce::Rectangle<int> (bank.getX(), 10, bank.getWidth(), 20) });

        // levers: the 3-way one has its labels beside the nut, the 2-way ones above and below it
        const int gap = 14, lever3W = 66, lever2W = 56, modsW = 112, cellH = 134;
        const int cellW = (r.getWidth() - 4 * gap - lever3W - 5 * lever2W - modsW) / 13;
        int x = r.getX();
        auto placeKnob  = [&] (Component& k) { k.setBounds (ox + x, oy + top - 4, cellW, cellH); x += cellW; };
        // caption level with the knob captions, nut level with the knob centres
        auto placeLever = [&] (Component& l) { const int w = &l == &lOrMode ? lever3W : lever2W; l.setBounds (ox + x, oy + top - 4, w, 116); x += w; };
        auto close = [&] (const juce::String& title, int x0) { groups.push_back ({ title, juce::Rectangle<int> (x0, 10, x - x0, 20) }); x += gap; };

        int x0 = x;
        for (auto* k : orangeKnobs()) placeKnob (*k);
        placeLever (lOrMode);
        close ("ORANGE", x0);
        x0 = x;
        for (auto* k : redKnobs()) placeKnob (*k);
        placeLever (lRdMode);
        close ("RED", x0);
        x0 = x;
        placeKnob (kScary);
        placeLever (lDeep);
        close ("MR. SCARY", x0);
        x0 = x;
        // three mod buttons stacked
        bTight.setBounds    (ox + x, oy + top,      modsW, 32);
        bDefizz.setBounds   (ox + x, oy + top + 39, modsW, 32);
        bMidShift.setBounds (ox + x, oy + top + 78, modsW, 32);
        x += modsW;
        close ("MODS", x0);
        x0 = x;
        placeLever (lRect); placeLever (lPower); placeLever (lTubes);
        groups.push_back ({ "POWER", juce::Rectangle<int> (x0, 10, x - x0, 20) });

        facePlate.groups = groups;
        facePlate.repaint();
    }
    // ---- output section: big knobs and a readable meter
    {
        auto r = outArea.reduced (12); r.removeFromTop (22);
        const int w = 170;
        kLowCut.setBounds (r.removeFromLeft (w)); kHighCut.setBounds (r.removeFromLeft (w)); kOut.setBounds (r.removeFromLeft (w));
        r.removeFromLeft (24);
        r.removeFromRight (6);
        meterCaption.setBounds (r.removeFromTop (22));
        r.removeFromTop (10);
        peakLabel.setBounds (r.removeFromTop (34));
        r.removeFromTop (8);
        outMeter.setBounds (r.removeFromTop (46));
    }
}

//==============================================================================
AmpDualRectEditor::AmpDualRectEditor (AmpDualRectProcessor& p)
    : AudioProcessorEditor (&p), proc (p), ui (p)
{
    const int startWidth = juce::jlimit (800, 2800, proc.editorWidth.load());   // read before any resize can overwrite it
    addAndMakeVisible (ui);
    ui.setBounds (0, 0, AmpUi::kW, AmpUi::kH);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) AmpUi::kW / (double) AmpUi::kH);
    setResizeLimits (800, 800 * AmpUi::kH / AmpUi::kW, 2800, 2800 * AmpUi::kH / AmpUi::kW);
    setSize (startWidth, juce::roundToInt ((double) startWidth * AmpUi::kH / AmpUi::kW));
}

void AmpDualRectEditor::resized()
{
    const float s = (float) getWidth() / (float) AmpUi::kW;
    ui.setTransform (juce::AffineTransform::scale (s));
    proc.editorWidth = getWidth();
}
