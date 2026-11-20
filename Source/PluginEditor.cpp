#include "PluginEditor.h"
#include "ParameterMapping.h"

namespace
{
    struct UiPalette
    {
        juce::Colour background, panel, panelHighlight, edge, text, muted, accent, accentDim, screen, grid;
    };

    const UiPalette darkPalette {
        juce::Colour (0xff151a24), juce::Colour (0xff292f40), juce::Colour (0xff394257),
        juce::Colour (0xff707b91), juce::Colour (0xffedf0f4), juce::Colour (0xffa4adbd),
        juce::Colour (0xff39d6e5), juce::Colour (0xff1e7782), juce::Colour (0xff111722),
        juce::Colour (0xff354052)
    };
    const UiPalette lightPalette {
        juce::Colour (0xffcfd5d9), juce::Colour (0xffe4e8eb), juce::Colour (0xfff3f5f6),
        juce::Colour (0xff9aa5ab), juce::Colour (0xff101418), juce::Colour (0xff414b52),
        juce::Colour (0xff22c7d6), juce::Colour (0xff137783), juce::Colour (0xffffffff),
        juce::Colour (0xffc9d1d5)
    };

    const UiPalette& paletteFor (bool light) noexcept
    {
        return light ? lightPalette : darkPalette;
    }

    class AdvancedSettingsPanel final : public juce::Component
    {
    public:
        AdvancedSettingsPanel (VoiceMorphLookAndFeel& lookAndFeelIn,
                               juce::Label& rangeLabelIn, juce::ComboBox& rangeBoxIn,
                               juce::Label& intensityLabelIn, juce::Slider& intensitySliderIn,
                               juce::Label& mixLabelIn, juce::Slider& mixSliderIn,
                               juce::Label& qualityLabelIn, juce::ComboBox& qualityBoxIn,
                               juce::ToggleButton& bypassButtonIn)
            : lookAndFeel (lookAndFeelIn), rangeLabel (rangeLabelIn), rangeBox (rangeBoxIn),
              intensityLabel (intensityLabelIn), intensitySlider (intensitySliderIn),
              mixLabel (mixLabelIn), mixSlider (mixSliderIn), qualityLabel (qualityLabelIn),
              qualityBox (qualityBoxIn), bypassButton (bypassButtonIn)
        {
            setLookAndFeel (&lookAndFeel);
            addAndMakeVisible (rangeLabel);
            addAndMakeVisible (rangeBox);
            addAndMakeVisible (intensityLabel);
            addAndMakeVisible (intensitySlider);
            addAndMakeVisible (mixLabel);
            addAndMakeVisible (mixSlider);
            addAndMakeVisible (qualityLabel);
            addAndMakeVisible (qualityBox);
            addAndMakeVisible (bypassButton);
        }

        ~AdvancedSettingsPanel() override { setLookAndFeel (nullptr); }

        void paint (juce::Graphics& g) override
        {
            const auto& colours = paletteFor (lookAndFeel.isLightTheme());
            auto bounds = getLocalBounds().toFloat().reduced (1.0f);
            juce::ColourGradient surface (colours.panelHighlight, bounds.getX(), bounds.getY(),
                                          colours.background, bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill (surface);
            g.fillRoundedRectangle (bounds, 10.0f);
            g.setColour (colours.edge.withAlpha (0.82f));
            g.drawRoundedRectangle (bounds, 10.0f, 1.2f);
            g.setColour (colours.accent.withAlpha (0.72f));
            g.fillRoundedRectangle (bounds.getX() + 16.0f, bounds.getY() + 1.0f,
                                    42.0f, 2.0f, 1.0f);
            g.setColour (colours.text);
            g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
            g.drawText ("ADVANCED PROCESSING", getLocalBounds().reduced (18, 12).removeFromTop (28),
                        juce::Justification::left);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (18, 48);
            auto left = area.removeFromLeft (area.getWidth() / 2).reduced (8, 4);
            auto right = area.reduced (8, 4);
            rangeLabel.setBounds (left.removeFromTop (23));
            rangeBox.setBounds (left.removeFromTop (34));
            qualityLabel.setBounds (left.removeFromTop (38));
            qualityBox.setBounds (left.removeFromTop (34));
            intensityLabel.setBounds (right.removeFromTop (23));
            intensitySlider.setBounds (right.removeFromTop (92));
            mixLabel.setBounds (right.removeFromTop (23));
            mixSlider.setBounds (right.removeFromTop (92));
            bypassButton.setBounds (getLocalBounds().removeFromBottom (42).reduced (18, 4));
        }

    private:
        VoiceMorphLookAndFeel& lookAndFeel;
        juce::Label& rangeLabel;
        juce::ComboBox& rangeBox;
        juce::Label& intensityLabel;
        juce::Slider& intensitySlider;
        juce::Label& mixLabel;
        juce::Slider& mixSlider;
        juce::Label& qualityLabel;
        juce::ComboBox& qualityBox;
        juce::ToggleButton& bypassButton;
    };

    class AdvancedSettingsWindow final : public juce::DocumentWindow
    {
    public:
        AdvancedSettingsWindow (VoiceMorphLookAndFeel& lookAndFeel,
                                juce::Label& rangeLabel, juce::ComboBox& rangeBox,
                                juce::Label& intensityLabel, juce::Slider& intensitySlider,
                                juce::Label& mixLabel, juce::Slider& mixSlider,
                                juce::Label& qualityLabel, juce::ComboBox& qualityBox,
                                juce::ToggleButton& bypassButton)
            : juce::DocumentWindow ("VoiceMorph Advanced",
                                    paletteFor (lookAndFeel.isLightTheme()).background,
                                    juce::DocumentWindow::allButtons, true)
        {
            setLookAndFeel (&lookAndFeel);
            setUsingNativeTitleBar (false);
            setResizable (true, true);
            setResizeLimits (480, 320, 900, 700);
            auto panel = std::make_unique<AdvancedSettingsPanel> (
                lookAndFeel, rangeLabel, rangeBox, intensityLabel, intensitySlider,
                mixLabel, mixSlider, qualityLabel, qualityBox, bypassButton);
            setContentOwned (panel.release(), true);
            centreWithSize (560, 390);
        }

        ~AdvancedSettingsWindow() override { setLookAndFeel (nullptr); }
        void closeButtonPressed() override { setVisible (false); }
    };
}

VoiceMorphLookAndFeel::VoiceMorphLookAndFeel()
{
    setLightTheme (false);
}

void VoiceMorphLookAndFeel::setLightTheme (bool shouldUseLightTheme)
{
    lightTheme = shouldUseLightTheme;
    const auto& colours = paletteFor (lightTheme);
    setColour (juce::Slider::textBoxTextColourId, colours.text);
    setColour (juce::Slider::textBoxBackgroundColourId, colours.screen);
    setColour (juce::Slider::textBoxOutlineColourId, colours.edge.withAlpha (0.72f));
    setColour (juce::Label::textColourId, colours.text);
    setColour (juce::ComboBox::textColourId, colours.text);
    setColour (juce::ComboBox::backgroundColourId, colours.panelHighlight);
    setColour (juce::ComboBox::outlineColourId, colours.edge);
    setColour (juce::PopupMenu::backgroundColourId, colours.panel);
    setColour (juce::PopupMenu::textColourId, colours.text);
    setColour (juce::TextButton::textColourOffId, colours.text);
    setColour (juce::TextButton::textColourOnId, colours.accent);
}

void VoiceMorphLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float, float startAngle, float endAngle,
                                               juce::Slider& slider)
{
    const auto& colours = paletteFor (lightTheme);
    const float side = static_cast<float> (juce::jmin (width, height));
    const float cx = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const float cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float outerR = side * 0.46f;
    const float faceR = side * 0.335f;
    const float traceR = side * 0.363f;
    const bool lcdDial = slider.getComponentID() == "lcd-dial";
    const bool percentDial = slider.getComponentID() == "percent-dial";
    const bool bipolar = lcdDial;
    const bool showReadout = lcdDial || percentDial;
    const float proportion = static_cast<float> (slider.valueToProportionOfLength (slider.getValue()));
    const float angle = static_cast<float> (voice_morph::parameter_mapping::rotaryAngleFromProportion (
        proportion, startAngle, endAngle));

    g.setColour (juce::Colours::black.withAlpha (lightTheme ? 0.18f : 0.48f));
    g.fillEllipse (cx - outerR + 1.5f, cy - outerR + 4.0f, outerR * 2.0f, outerR * 2.0f);

    juce::ColourGradient metal (lightTheme ? juce::Colour (0xfffafbfb) : juce::Colour (0xfff3f4f5),
                                cx - outerR, cy - outerR,
                                lightTheme ? juce::Colour (0xff9da8ae) : juce::Colour (0xff454f5d),
                                cx + outerR, cy + outerR, false);
    g.setGradientFill (metal);
    g.fillEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
    g.setColour (lightTheme ? juce::Colour (0xff737f85) : juce::Colour (0xff090c12));
    g.drawEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f,
                   juce::jmax (1.0f, side * 0.012f));

    const float ringOuter = side * 0.393f;
    const float ringInner = side * 0.326f;
    g.setColour (lightTheme ? juce::Colour (0xffc4ccd0) : juce::Colour (0xff111722));
    g.fillEllipse (cx - ringOuter, cy - ringOuter, ringOuter * 2.0f, ringOuter * 2.0f);
    g.setColour (lightTheme ? juce::Colour (0xff849097) : juce::Colour (0xff070a10));
    g.fillEllipse (cx - ringInner, cy - ringInner, ringInner * 2.0f, ringInner * 2.0f);

    juce::ColourGradient faceGrad (lightTheme ? juce::Colour (0xffffffff) : juce::Colour (0xff3a4352),
                                   cx - faceR * 0.78f, cy - faceR,
                                   lightTheme ? juce::Colour (0xffe4e9ec) : juce::Colour (0xff151a23),
                                   cx + faceR, cy + faceR, false);
    g.setGradientFill (faceGrad);
    g.fillEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);
    g.setColour (lightTheme ? juce::Colour (0xffa8b1b6) : juce::Colour (0xff717c8d).withAlpha (0.9f));
    g.drawEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    const float tickInner = outerR * 0.84f, tickOuter = outerR * 0.965f;
    constexpr int ticks = 31;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (ticks - 1);
        const float a = startAngle + t * (endAngle - startAngle);
        const bool major = (i % 5 == 0);
        const float r0 = major ? tickInner - 1.5f : tickInner + 1.0f;
        const float r1 = major ? tickOuter + 0.8f : tickOuter - 1.0f;
        const juce::Point<float> p0 { cx + std::cos (a) * r0, cy + std::sin (a) * r0 };
        const juce::Point<float> p1 { cx + std::cos (a) * r1, cy + std::sin (a) * r1 };
        g.setColour (major ? colours.muted.withAlpha (0.92f) : colours.edge.withAlpha (0.82f));
        g.drawLine ({ p0, p1 }, major ? 1.5f : 0.9f);
    }

    auto drawArc = [&] (float from, float to, juce::Colour colour, float thickness)
    {
        if (std::abs (to - from) < 0.001f)
            return;
        juce::Path path;
        const int steps = juce::jmax (2, static_cast<int> (std::ceil (std::abs (to - from) * 12.0f)));
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float> (i) / static_cast<float> (steps);
            const float a = from + (to - from) * t;
            const juce::Point<float> point { cx + std::cos (a) * traceR, cy + std::sin (a) * traceR };
            if (i == 0) path.startNewSubPath (point); else path.lineTo (point);
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    };

    const float neutralAngle = voice_morph::parameter_mapping::rotaryAngleFromProportion (
        0.5, startAngle, endAngle);
    drawArc (startAngle, endAngle, colours.muted.withAlpha (lightTheme ? 0.32f : 0.35f),
             juce::jmax (2.0f, side * 0.024f));
    // Pitch and formant traces start at neutral, showing the actual signed change.
    // Unipolar controls show their amount from minimum to the current value.
    drawArc (bipolar ? neutralAngle : startAngle, angle, colours.accent,
             juce::jmax (3.0f, side * 0.042f));

    const juce::Point<float> markerStart { cx + std::cos (angle) * outerR * 0.835f,
                                           cy + std::sin (angle) * outerR * 0.835f };
    const juce::Point<float> markerEnd { cx + std::cos (angle) * outerR * 0.965f,
                                         cy + std::sin (angle) * outerR * 0.965f };
    g.setColour (colours.accent.withAlpha (0.28f));
    g.drawLine ({ markerStart, markerEnd }, juce::jmax (7.0f, side * 0.070f));
    g.setColour (colours.accent);
    g.drawLine ({ markerStart, markerEnd }, juce::jmax (4.0f, side * 0.040f));

    if (showReadout)
    {
        auto number = juce::Rectangle<float> (cx - faceR * 0.86f, cy - side * 0.105f,
                                               faceR * 1.72f, side * 0.20f);
        auto units = juce::Rectangle<float> (cx - faceR * 0.62f, cy + side * 0.085f,
                                              faceR * 1.24f, side * 0.13f);
        g.setColour (colours.text);
        g.setFont (juce::FontOptions (juce::jlimit (17.0f, 25.0f, side * 0.145f), juce::Font::bold));
        g.drawFittedText (juce::String (slider.getValue(), lcdDial ? 2 : 0), number.toNearestInt(),
                          juce::Justification::centred, 1);
        g.setColour (colours.accent);
        g.setFont (juce::FontOptions (juce::jmax (10.0f, side * 0.075f), juce::Font::bold));
        g.drawFittedText (lcdDial ? "ST" : "%", units.toNearestInt(), juce::Justification::centred, 1);
    }
}

void VoiceMorphLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                float sliderPos, float, float,
                                                juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (slider);
    const auto& colours = paletteFor (lightTheme);
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        const float top = static_cast<float> (y + 10);
        const float bottom = static_cast<float> (y + height - 8);
        for (int i = 0; i <= 12; ++i)
        {
            const float yy = top + (bottom - top) * static_cast<float> (i) / 12.0f;
            const float length = (i % 3 == 0) ? 8.0f : 5.0f;
            g.setColour (colours.muted.withAlpha (i % 3 == 0 ? 0.78f : 0.48f));
            g.drawLine (cx - 16.0f, yy, cx - 16.0f + length, yy, 1.0f);
            g.drawLine (cx + 16.0f - length, yy, cx + 16.0f, yy, 1.0f);
        }

        g.setColour (lightTheme ? juce::Colour (0xffaeb8bd) : juce::Colour (0xff080b11));
        g.fillRoundedRectangle (cx - 4.0f, top, 8.0f, bottom - top, 4.0f);
        g.setColour (colours.accent.withAlpha (0.22f));
        g.fillRoundedRectangle (cx - 2.0f, sliderPos, 4.0f, juce::jmax (0.0f, bottom - sliderPos), 2.0f);

        juce::Rectangle<float> cap (cx - 19.0f, sliderPos - 10.0f, 38.0f, 20.0f);
        g.setColour (juce::Colours::black.withAlpha (lightTheme ? 0.16f : 0.42f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 4.0f);
        juce::ColourGradient capGrad (lightTheme ? juce::Colour (0xfff9fafb) : juce::Colour (0xfff3f4f5),
                                      cap.getX(), cap.getY(),
                                      lightTheme ? juce::Colour (0xffaab4b9) : juce::Colour (0xff454f5d),
                                      cap.getX(), cap.getBottom(), false);
        g.setGradientFill (capGrad);
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (colours.accent);
        g.drawLine (cap.getX() + 4.0f, sliderPos, cap.getRight() - 4.0f, sliderPos, 2.0f);
    }
    else
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                     static_cast<float> (width), static_cast<float> (height)).reduced (6.0f);
        const float cy = bounds.getCentreY();
        g.setColour (colours.muted.withAlpha (0.35f));
        g.drawRoundedRectangle (bounds.withHeight (6.0f).withCentre (
                                    juce::Point<float> (bounds.getCentreX(), cy)), 3.0f, 1.0f);
        g.setColour (colours.accent);
        g.fillEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    }
}

void VoiceMorphLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                    const juce::Colour&, bool highlighted, bool down)
{
    const auto& colours = paletteFor (lightTheme);
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool toggled = button.getToggleState();
    const auto top = toggled ? colours.panelHighlight.brighter (0.13f) : colours.panelHighlight;
    const auto bottom = toggled ? colours.panel.darker (0.02f) : colours.panel.darker (0.16f);
    g.setColour (juce::Colours::black.withAlpha (lightTheme ? 0.16f : 0.38f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 7.0f);
    juce::ColourGradient grad (highlighted ? top.brighter (0.08f) : top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (toggled ? colours.accent.withAlpha (0.92f) : colours.edge.withAlpha (0.84f));
    g.drawRoundedRectangle (r, 7.0f, toggled ? 1.5f : 1.0f);
    if (toggled)
    {
        g.setColour (colours.accent);
        g.fillRoundedRectangle (r.getX() + 8.0f, r.getBottom() - 3.5f, r.getWidth() - 16.0f, 2.0f, 1.0f);
    }
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 7.0f);
    }
}

void VoiceMorphLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                     bool highlighted, bool down)
{
    const auto& colours = paletteFor (lightTheme);
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool active = button.getToggleState();
    juce::ColourGradient fill (active ? colours.panelHighlight.brighter (0.08f) : colours.panelHighlight,
                               bounds.getX(), bounds.getY(),
                               active ? colours.panel.darker (0.02f) : colours.panel.darker (0.16f),
                               bounds.getX(), bounds.getBottom(), false);
    g.setColour (juce::Colours::black.withAlpha (lightTheme ? 0.12f : 0.34f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 6.0f);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (active ? colours.accent.withAlpha (0.95f) : colours.edge.withAlpha (0.82f));
    g.drawRoundedRectangle (bounds, 6.0f, active ? 1.4f : 1.0f);
    if (active)
    {
        g.setColour (colours.accent);
        g.fillRoundedRectangle (bounds.getX() + 8.0f, bounds.getBottom() - 3.0f,
                                juce::jmax (0.0f, bounds.getWidth() - 16.0f), 2.0f, 1.0f);
    }
    if (highlighted || down)
    {
        g.setColour (colours.accent.withAlpha (down ? 0.12f : 0.07f));
        g.fillRoundedRectangle (bounds, 6.0f);
    }
    g.setColour (active ? colours.accent : colours.text);
    g.setFont (juce::FontOptions (juce::jmin (12.0f, bounds.getHeight() * 0.38f), juce::Font::bold));
    g.drawFittedText (button.getButtonText(), bounds.reduced (8.0f, 1.0f).toNearestInt(),
                      juce::Justification::centred, 2);
}

void VoiceMorphAudioProcessorEditor::PitchContourMonitor::paint (juce::Graphics& g)
{
    const auto& colours = paletteFor (lookAndFeel.isLightTheme());
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (lookAndFeel.isLightTheme() ? 0.12f : 0.42f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 10.0f);
    juce::ColourGradient glass (colours.screen.brighter (0.03f), bounds.getX(), bounds.getY(),
                                colours.screen.darker (0.07f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (colours.edge);
    g.drawRoundedRectangle (bounds, 10.0f, 1.2f);

    auto inside = bounds.reduced (16.0f, 12.0f);
    auto legend = inside.removeFromTop (27.0f);
    g.setFont (juce::FontOptions (juce::jmax (10.0f, getHeight() * 0.021f), juce::Font::bold));
    g.setColour (colours.accent);
    g.drawText ("PITCH CONTOUR", legend.removeFromLeft (180.0f).toNearestInt(), juce::Justification::left);
    g.setFont (juce::FontOptions (juce::jmax (8.0f, getHeight() * 0.016f)));
    g.setColour (colours.muted);
    g.drawText ("3.0 s  /  100 Hz", legend.removeFromLeft (126.0f).toNearestInt(), juce::Justification::left);

    auto legendRight = inside.getTopRight();
    const float legendY = inside.getY() + 9.0f;
    g.setColour (colours.muted);
    g.drawLine (legendRight.x - 205.0f, legendY, legendRight.x - 187.0f, legendY, 2.0f);
    g.setColour (colours.muted);
    g.drawText ("INPUT F0", juce::Rectangle<int> (static_cast<int> (legendRight.x - 182.0f),
                static_cast<int> (legendY - 9.0f), 76, 18), juce::Justification::left);
    g.setColour (colours.accent);
    g.drawLine (legendRight.x - 107.0f, legendY, legendRight.x - 89.0f, legendY, 2.0f);
    g.drawText ("TARGET F0", juce::Rectangle<int> (static_cast<int> (legendRight.x - 84.0f),
                static_cast<int> (legendY - 9.0f), 80, 18), juce::Justification::left);
    if (! processor.isVoiceEngineConnected())
    {
        g.setColour (colours.muted.withAlpha (0.8f));
        g.drawText ("ENGINE PENDING", juce::Rectangle<int> (static_cast<int> (legendRight.x - 196.0f),
                    static_cast<int> (legendY + 9.0f), 128, 15), juce::Justification::left);
    }

    auto lower = inside;
    lower.removeFromTop (4.0f);
    auto timeLabelArea = lower.removeFromBottom (38.0f);
    auto graph = lower;
    graph.removeFromLeft (45.0f);
    graph.removeFromRight (8.0f);
    graph = graph.reduced (0.0f, 1.0f);

    const std::array<float, 5> frequencies { 55.0f, 110.0f, 220.0f, 440.0f, 880.0f };
    const float logRange = std::log2 (880.0f / 55.0f);
    for (size_t i = 0; i < frequencies.size(); ++i)
    {
        const float hz = frequencies[i];
        const float normalized = static_cast<float> (std::log2 (hz / 55.0f) / logRange);
        const float yy = graph.getBottom() - normalized * graph.getHeight();
        g.setColour (colours.grid.withAlpha (0.78f));
        g.drawHorizontalLine (static_cast<int> (yy), graph.getX(), graph.getRight());
        g.setColour (colours.muted);
        g.setFont (juce::FontOptions (juce::jmax (8.0f, getHeight() * 0.015f)));
        g.drawText (juce::String (static_cast<int> (hz)),
                    juce::Rectangle<int> (static_cast<int> (inside.getX()),
                    static_cast<int> (yy - 8.0f), 39, 16), juce::Justification::centredRight);
    }
    g.setFont (juce::FontOptions (juce::jmax (8.0f, getHeight() * 0.015f)));
    g.setColour (colours.muted);
    const float tickWidth = graph.getWidth() / 6.0f;
    for (int tick = 0; tick <= 6; ++tick)
    {
        const float xx = graph.getX() + graph.getWidth() * static_cast<float> (tick) / 6.0f;
        g.setColour (colours.grid.withAlpha (0.62f));
        g.drawVerticalLine (static_cast<int> (xx), graph.getY(), graph.getBottom());
        g.setColour (colours.muted);
        const auto xLabel = juce::Rectangle<int> (static_cast<int> (xx - tickWidth * 0.42f),
                              static_cast<int> (timeLabelArea.getY()), static_cast<int> (tickWidth * 0.84f), 16);
        g.drawText (juce::String (static_cast<double> (tick) * 0.5, 1), xLabel,
                    tick == 0 ? juce::Justification::left
                              : (tick == 6 ? juce::Justification::right : juce::Justification::centred));
    }
    g.setColour (colours.muted);
    g.drawText ("TIME (s)", juce::Rectangle<int> (static_cast<int> (graph.getX()),
                static_cast<int> (timeLabelArea.getY() + 17.0f), static_cast<int> (graph.getWidth()), 16),
                juce::Justification::centred);

    VoiceMorphAudioProcessor::PitchContour inputHz {}, targetHz {};
    processor.copyPitchContours (inputHz, targetHz);
    auto drawContour = [&] (const auto& samples, juce::Colour colour)
    {
        juce::Path path;
        bool penDown = false;
        bool hasPoint = false;
        constexpr size_t visibleSamples = 300; // 100 analyzer frames per second, newest 3 seconds.
        const size_t firstVisibleSample = samples.size() - visibleSamples;
        for (size_t i = firstVisibleSample; i < samples.size(); ++i)
        {
            const float hz = samples[i];
            const float t = static_cast<float> (i - firstVisibleSample)
                          / static_cast<float> (visibleSamples - 1);
            if (! std::isfinite (hz) || hz < 55.0f || hz > 880.0f)
            {
                penDown = false;
                continue;
            }
            const float normalized = static_cast<float> (std::log2 (hz / 55.0f) / logRange);
            const juce::Point<float> point {
                graph.getX() + t * graph.getWidth(),
                graph.getBottom() - normalized * graph.getHeight()
            };
            if (! penDown) path.startNewSubPath (point); else path.lineTo (point);
            penDown = true;
            hasPoint = true;
        }
        if (hasPoint)
        {
            g.setColour (colour.withAlpha (0.94f));
            g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }
    };
    drawContour (inputHz, colours.muted);
    drawContour (targetHz, colours.accent);
}

VoiceMorphAudioProcessorEditor::VoiceMorphAudioProcessorEditor (VoiceMorphAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), pitchMonitor (p, lookAndFeel)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (980, 640, 1600, 1060);
    setSize (1220, 810);

    titleLabel.setText ("VOICEMORPH", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, paletteFor (lookAndFeel.isLightTheme()).text);
    addAndMakeVisible (titleLabel);

    statusLabel.setText ("ANALYZER  /  WAITING FOR INPUT", juce::dontSendNotification);
    statusLabel.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    statusLabel.setColour (juce::Label::textColourId, paletteFor (lookAndFeel.isLightTheme()).accent);
    addAndMakeVisible (statusLabel);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    presetLabel.setColour (juce::Label::textColourId, paletteFor (lookAndFeel.isLightTheme()).muted);
    addAndMakeVisible (presetLabel);
    presetBox.addItemList ({ "Default", "Natural Lift", "Soft / Airy", "Warm / Dark", "Character" }, 1);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    juce::Button* buttons[] = { &prevPresetButton, &nextPresetButton, &aButton, &bButton, &advancedButton,
                                  &darkThemeButton, &lightThemeButton, &bypassButton, &consonantButton, &transientButton,
                                  &naturalModeButton, &balancedModeButton, &preciseModeButton };
    for (auto* button : buttons)
        addAndMakeVisible (*button);
    prevPresetButton.onClick = [this] { presetBox.setSelectedItemIndex (juce::jmax (0, presetBox.getSelectedItemIndex() - 1)); };
    nextPresetButton.onClick = [this] { presetBox.setSelectedItemIndex (juce::jmin (presetBox.getNumItems() - 1, presetBox.getSelectedItemIndex() + 1)); };
    aButton.setClickingTogglesState (false);
    bButton.setClickingTogglesState (false);
    aButton.setToggleState (true, juce::dontSendNotification);
    bButton.setToggleState (false, juce::dontSendNotification);
    aButton.onClick = [this] { processor.switchAB (false); aButton.setToggleState (true, juce::dontSendNotification); bButton.setToggleState (false, juce::dontSendNotification); };
    bButton.onClick = [this] { processor.switchAB (true); bButton.setToggleState (true, juce::dontSendNotification); aButton.setToggleState (false, juce::dontSendNotification); };
    advancedButton.onClick = [this] { showAdvancedSettings(); };
    darkThemeButton.setClickingTogglesState (false);
    lightThemeButton.setClickingTogglesState (false);
    darkThemeButton.onClick = [this] { lookAndFeel.setLightTheme (false); refreshTheme(); };
    lightThemeButton.onClick = [this] { lookAndFeel.setLightTheme (true); refreshTheme(); };
    naturalModeButton.onClick = [this] { setVoiceMode (0); };
    balancedModeButton.onClick = [this] { setVoiceMode (1); };
    preciseModeButton.onClick = [this] { setVoiceMode (2); };

    auto setupLabel = [this] (juce::Label& label, const juce::String& name)
    {
        label.setText (name, juce::dontSendNotification);
        label.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, paletteFor (lookAndFeel.isLightTheme()).muted);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    };
    setupLabel (inputLabel, "INPUT"); setupLabel (outputLabel, "OUTPUT");
    setupLabel (pitchLabel, "PITCH"); setupLabel (formantLabel, "FORMANT");
    setupLabel (trackingLabel, "TRACKING"); setupLabel (pitchDynamicsLabel, "PITCH DYNAMICS");
    setupLabel (breathLabel, "BREATH");
    setupLabel (mixLabel, "VOICE MIX"); setupLabel (voiceModeLabel, "VOICE MODE");
    setupLabel (deEssLabel, "SIBILANCE"); setupLabel (deEssMixLabel, "DE-ESSER MIX");
    setupLabel (deEssRangeLabel, "DE-ESSER RANGE"); setupLabel (qualityLabel, "PROCESSING");

    for (auto* slider : { &inputGainSlider, &outputGainSlider, &voiceMixSlider, &pitchSlider, &formantSlider,
                          &trackingSlider, &pitchDynamicsSlider, &breathSlider })
        addAndMakeVisible (*slider);
    configureFader (inputGainSlider);
    configureFader (outputGainSlider);
    configureRotary (voiceMixSlider, "%");
    configureRotary (pitchSlider, " st", true);
    configureRotary (formantSlider, " st", true);
    configureRotary (trackingSlider, "%");
    configureRotary (pitchDynamicsSlider, "%");
    pitchDynamicsSlider.setComponentID ("percent-dial");
    pitchDynamicsSlider.setTooltip ("Expands or softens expressive F0 movement around the phrase pitch. 0% = neutral, 100% = 1.45x.");
    configureRotary (breathSlider, "%");
    configureRotary (deEssIntensitySlider, "%");
    configureRotary (deEssMixSlider, "%");

    refreshVoiceModeButtons();

    deEssRangeBox.addItemList ({ "Low-Hi  1-4 kHz", "Mid-Hi  2-4 kHz", "High  5-10 kHz", "Hi-End  5-20 kHz" }, 1);
    qualityBox.addItemList ({ "LIVE", "BALANCED", "STUDIO" }, 1);
    addAndMakeVisible (pitchMonitor);

    inputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "inputGain", inputGainSlider);
    outputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "outputGain", outputGainSlider);
    voiceMixAttachment = std::make_unique<SliderAttachment> (processor.apvts, "voiceMix", voiceMixSlider);
    pitchAttachment = std::make_unique<SliderAttachment> (processor.apvts, "pitchSemitones", pitchSlider);
    formantAttachment = std::make_unique<SliderAttachment> (processor.apvts, "formantSemitones", formantSlider);
    trackingAttachment = std::make_unique<SliderAttachment> (processor.apvts, "tracking", trackingSlider);
    pitchDynamicsAttachment = std::make_unique<SliderAttachment> (processor.apvts, "pitchDynamics", pitchDynamicsSlider);
    breathAttachment = std::make_unique<SliderAttachment> (processor.apvts, "breathPreserve", breathSlider);
    deEssIntensityAttachment = std::make_unique<SliderAttachment> (processor.apvts, "deEssIntensity", deEssIntensitySlider);
    deEssMixAttachment = std::make_unique<SliderAttachment> (processor.apvts, "deEssMix", deEssMixSlider);
    bypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "bypass", bypassButton);
    consonantAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "consonantPreserve", consonantButton);
    transientAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "transientPreserve", transientButton);
    deEssBypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "deEssBypass", deEssBypassButton);
    deEssRangeAttachment = std::make_unique<ComboAttachment> (processor.apvts, "deEssRange", deEssRangeBox);
    qualityAttachment = std::make_unique<ComboAttachment> (processor.apvts, "qualityMode", qualityBox);

    refreshTheme();
    startTimerHz (30);
}

VoiceMorphAudioProcessorEditor::~VoiceMorphAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoiceMorphAudioProcessorEditor::configureRotary (juce::Slider& slider, const juce::String& suffix, bool lcdReadout)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    // Put the neutral mark at 12 o'clock to match the centered dial preview.
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 0.75f,
                                juce::MathConstants<float>::pi * 2.25f, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setTextValueSuffix (suffix);
    slider.setComponentID (lcdReadout ? "lcd-dial" : "small-dial");
    slider.setMouseDragSensitivity (170);
    slider.setLookAndFeel (&lookAndFeel);
}

void VoiceMorphAudioProcessorEditor::configureFader (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 64, 22);
    slider.setTextValueSuffix (" dB");
    slider.setLookAndFeel (&lookAndFeel);
}

void VoiceMorphAudioProcessorEditor::addSectionLabel (juce::Label& label, const juce::String& name)
{
    label.setText (name, juce::dontSendNotification);
    addAndMakeVisible (label);
}

void VoiceMorphAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    const auto& colours = paletteFor (lookAndFeel.isLightTheme());
    g.setColour (juce::Colours::black.withAlpha (lookAndFeel.isLightTheme() ? 0.10f : 0.28f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), corner);
    juce::ColourGradient grad (colours.panelHighlight, r.getX(), r.getY(),
                               colours.panel.darker (lookAndFeel.isLightTheme() ? 0.02f : 0.20f),
                               r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, corner);
    g.setColour (colours.edge.withAlpha (0.72f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void VoiceMorphAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto& colours = paletteFor (lookAndFeel.isLightTheme());
    g.fillAll (colours.background);
    auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    auto body = bounds;
    body.removeFromTop (64.0f);
    body.removeFromTop (10.0f);

    const float rightWidth = 164.0f;
    const float leftWidth = 220.0f;
    auto right = body.removeFromRight (rightWidth);
    body.removeFromRight (10.0f);
    auto left = body.removeFromLeft (leftWidth);
    body.removeFromLeft (10.0f);
    auto centre = body;

    drawPanel (g, { bounds.getX(), bounds.getY(), bounds.getWidth(), 54.0f }, 10.0f);
    drawPanel (g, left, 12.0f);
    drawPanel (g, centre, 12.0f);
    drawPanel (g, right, 12.0f);

    g.setColour (colours.edge.withAlpha (0.6f));
    g.drawVerticalLine (static_cast<int> (left.getRight() + 5.0f), left.getY() + 10.0f, left.getBottom() - 10.0f);
    g.drawVerticalLine (static_cast<int> (centre.getRight() + 5.0f), centre.getY() + 10.0f, centre.getBottom() - 10.0f);
}

void VoiceMorphAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto header = r.removeFromTop (54);
    titleLabel.setBounds (header.removeFromLeft (194).reduced (4, 6));
    statusLabel.setBounds (header.removeFromLeft (168).reduced (4, 15));
    presetLabel.setBounds (header.removeFromLeft (52).reduced (1, 0));
    presetBox.setBounds (header.removeFromLeft (178).reduced (2, 10));
    prevPresetButton.setBounds (header.removeFromLeft (30).reduced (2, 8));
    nextPresetButton.setBounds (header.removeFromLeft (30).reduced (2, 8));
    header.removeFromLeft (12);
    bypassButton.setBounds (header.removeFromRight (90).reduced (2, 8));
    header.removeFromRight (8);
    bButton.setBounds (header.removeFromRight (34).reduced (2, 8));
    aButton.setBounds (header.removeFromRight (34).reduced (2, 8));
    header.removeFromRight (10);
    auto themeArea = header.removeFromRight (138).reduced (2, 8);
    darkThemeButton.setBounds (themeArea.removeFromLeft (themeArea.getWidth() / 2));
    lightThemeButton.setBounds (themeArea);

    r.removeFromTop (10);
    auto body = r;
    auto right = body.removeFromRight (164);
    body.removeFromRight (10);
    auto left = body.removeFromLeft (220);
    body.removeFromLeft (10);
    auto centre = body;

    auto leftContent = left.reduced (12, 10);
    const int leftInnerHeight = leftContent.getHeight();
    auto pitchArea = leftContent.removeFromTop (juce::roundToInt (leftInnerHeight * 0.37f));
    auto formantArea = leftContent.removeFromTop (juce::roundToInt (leftInnerHeight * 0.32f));
    auto mixArea = leftContent;
    pitchLabel.setBounds (pitchArea.removeFromTop (22));
    const int pitchSide = juce::jmin (184, pitchArea.getWidth() - 8, pitchArea.getHeight() - 8);
    pitchSlider.setBounds (pitchArea.withSizeKeepingCentre (pitchSide, pitchSide));
    formantLabel.setBounds (formantArea.removeFromTop (21));
    const int formantSide = juce::jmin (154, formantArea.getWidth() - 12, formantArea.getHeight() - 6);
    formantSlider.setBounds (formantArea.withSizeKeepingCentre (formantSide, formantSide));
    mixLabel.setBounds (mixArea.removeFromTop (20));
    const int mixSide = juce::jmin (108, mixArea.getWidth() - 18, mixArea.getHeight() - 4);
    voiceMixSlider.setBounds (mixArea.withSizeKeepingCentre (mixSide, mixSide));

    auto centreContent = centre.reduced (14, 13);
    auto controls = centreContent.removeFromBottom (205);
    centreContent.removeFromBottom (10);
    pitchMonitor.setBounds (centreContent);

    auto bottomRow = controls;
    auto statusRow = bottomRow.removeFromBottom (34);
    auto advancedArea = statusRow.removeFromRight (126);
    advancedButton.setBounds (advancedArea.reduced (1, 2));
    consonantButton.setBounds (statusRow.removeFromLeft (214).reduced (1, 2));
    transientButton.setBounds (statusRow.removeFromLeft (214).reduced (1, 2));

    const int controlColumnWidth = controls.getWidth() / 4;
    auto trackingArea = controls.removeFromLeft (controlColumnWidth);
    auto dynamicsArea = controls.removeFromLeft (controlColumnWidth);
    auto breathArea = controls.removeFromLeft (controlColumnWidth);
    auto modeArea = controls;
    trackingLabel.setBounds (trackingArea.removeFromTop (20));
    pitchDynamicsLabel.setBounds (dynamicsArea.removeFromTop (20));
    breathLabel.setBounds (breathArea.removeFromTop (20));
    const int trackingDialSide = juce::jmin (116, trackingArea.getWidth() - 12, trackingArea.getHeight() - 2);
    trackingSlider.setBounds (trackingArea.withSizeKeepingCentre (trackingDialSide, trackingDialSide));
    const int dynamicsDialSide = juce::jmin (116, dynamicsArea.getWidth() - 12, dynamicsArea.getHeight() - 2);
    pitchDynamicsSlider.setBounds (dynamicsArea.withSizeKeepingCentre (dynamicsDialSide, dynamicsDialSide));
    const int breathDialSide = juce::jmin (116, breathArea.getWidth() - 12, breathArea.getHeight() - 2);
    breathSlider.setBounds (breathArea.withSizeKeepingCentre (breathDialSide, breathDialSide));
    voiceModeLabel.setBounds (modeArea.removeFromTop (20));
    auto modeButtons = modeArea.withSizeKeepingCentre (juce::jmin (300, modeArea.getWidth() - 12), 46);
    const int segmentWidth = modeButtons.getWidth() / 3;
    naturalModeButton.setBounds (modeButtons.removeFromLeft (segmentWidth));
    balancedModeButton.setBounds (modeButtons.removeFromLeft (segmentWidth));
    preciseModeButton.setBounds (modeButtons);

    auto rightContent = right.reduced (8, 10);
    auto inputArea = rightContent.removeFromLeft (rightContent.getWidth() / 2);
    auto outputArea = rightContent;
    inputLabel.setBounds (inputArea.removeFromTop (22));
    outputLabel.setBounds (outputArea.removeFromTop (22));
    inputGainSlider.setBounds (inputArea.reduced (2, 2));
    outputGainSlider.setBounds (outputArea.reduced (2, 2));
}

void VoiceMorphAudioProcessorEditor::showAdvancedSettings()
{
    if (advancedWindow == nullptr)
    {
        advancedWindow = std::make_unique<AdvancedSettingsWindow> (
            lookAndFeel, deEssRangeLabel, deEssRangeBox, deEssLabel, deEssIntensitySlider,
            deEssMixLabel, deEssMixSlider, qualityLabel, qualityBox, deEssBypassButton);
    }
    advancedWindow->setVisible (true);
    advancedWindow->toFront (true);
    advancedWindow->repaint();
}

void VoiceMorphAudioProcessorEditor::refreshVoiceModeButtons()
{
    const auto* rawValue = processor.apvts.getRawParameterValue ("voiceMode");
    const int index = voice_morph::parameter_mapping::choiceValueToIndex (
        rawValue != nullptr ? rawValue->load (std::memory_order_relaxed) : 0.0f, 3);
    naturalModeButton.setToggleState (index == 0, juce::dontSendNotification);
    balancedModeButton.setToggleState (index == 1, juce::dontSendNotification);
    preciseModeButton.setToggleState (index == 2, juce::dontSendNotification);
}

void VoiceMorphAudioProcessorEditor::setVoiceMode (int index)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter ("voiceMode")))
    {
        const float value = static_cast<float> (juce::jlimit (0, 2, index));
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
    refreshVoiceModeButtons();
}

void VoiceMorphAudioProcessorEditor::refreshTheme()
{
    const auto& colours = paletteFor (lookAndFeel.isLightTheme());
    darkThemeButton.setToggleState (! lookAndFeel.isLightTheme(), juce::dontSendNotification);
    lightThemeButton.setToggleState (lookAndFeel.isLightTheme(), juce::dontSendNotification);

    for (auto* label : { &titleLabel, &presetLabel, &inputLabel, &outputLabel, &pitchLabel,
                         &formantLabel, &trackingLabel, &pitchDynamicsLabel, &breathLabel, &mixLabel, &voiceModeLabel,
                         &deEssLabel, &deEssMixLabel, &deEssRangeLabel, &qualityLabel })
        label->setColour (juce::Label::textColourId, colours.muted);
    titleLabel.setColour (juce::Label::textColourId, colours.text);
    statusLabel.setColour (juce::Label::textColourId, colours.accent);

    for (auto* button : { &prevPresetButton, &nextPresetButton, &aButton, &bButton, &advancedButton,
                          &darkThemeButton, &lightThemeButton, &naturalModeButton, &balancedModeButton,
                          &preciseModeButton })
    {
        button->setColour (juce::TextButton::textColourOffId, colours.text);
        button->setColour (juce::TextButton::textColourOnId, colours.accent);
        button->repaint();
    }
    for (auto* slider : { &inputGainSlider, &outputGainSlider, &voiceMixSlider, &pitchSlider,
                          &formantSlider, &trackingSlider, &pitchDynamicsSlider, &breathSlider, &deEssIntensitySlider, &deEssMixSlider })
        slider->repaint();

    presetBox.setColour (juce::ComboBox::textColourId, colours.text);
    presetBox.setColour (juce::ComboBox::backgroundColourId, colours.panelHighlight);
    presetBox.setColour (juce::ComboBox::outlineColourId, colours.edge);
    deEssRangeBox.setColour (juce::ComboBox::textColourId, colours.text);
    deEssRangeBox.setColour (juce::ComboBox::backgroundColourId, colours.panelHighlight);
    deEssRangeBox.setColour (juce::ComboBox::outlineColourId, colours.edge);
    qualityBox.setColour (juce::ComboBox::textColourId, colours.text);
    qualityBox.setColour (juce::ComboBox::backgroundColourId, colours.panelHighlight);
    qualityBox.setColour (juce::ComboBox::outlineColourId, colours.edge);

    if (advancedWindow != nullptr)
    {
        advancedWindow->setBackgroundColour (colours.background);
        advancedWindow->repaint();
    }
    pitchMonitor.repaint();
    repaint();
}

void VoiceMorphAudioProcessorEditor::timerCallback()
{
    SpeechFeatures features;
    processor.getSpeechFeatures (features);
    refreshVoiceModeButtons();
    const auto& colours = paletteFor (lookAndFeel.isLightTheme());

    if (features.rms < 0.0006f)
    {
        statusLabel.setText ("ANALYZER  /  WAITING FOR INPUT", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, colours.muted);
    }
    else if (features.voiced >= 0.25f && features.pitchHz > 0.0f)
    {
        statusLabel.setText ("ANALYZER  /  F0 " + juce::String (features.pitchHz, 0)
                             + " Hz  /  " + juce::String (features.voiced * 100.0f, 0) + "%",
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, colours.accent);
    }
    else
    {
        statusLabel.setText ("ANALYZER  /  APERIODIC "
                             + juce::String (features.aperiodic * 100.0f, 0) + "%",
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, colours.muted);
    }
    pitchMonitor.repaint();
}

void VoiceMorphAudioProcessorEditor::applyPreset (int index)
{
    auto setFloat = [this] (const char* id, float value)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    auto setChoice = [this] (const char* id, int indexValue, int count)
    {
        if (auto* p = processor.apvts.getParameter (id))
            p->setValueNotifyingHost (count <= 1 ? 0.0f : static_cast<float> (indexValue) / static_cast<float> (count - 1));
    };

    switch (index)
    {
        case 1: setFloat ("pitchSemitones", 2.0f); setFloat ("formantSemitones", 1.5f); setFloat ("pitchDynamics", 85.0f); setFloat ("tracking", 72.0f); setFloat ("breathPreserve", 75.0f); setChoice ("voiceMode", 0, 3); break;
        case 2: setFloat ("pitchSemitones", 2.5f); setFloat ("formantSemitones", 2.5f); setFloat ("pitchDynamics", 58.0f); setFloat ("tracking", 62.0f); setFloat ("breathPreserve", 90.0f); setChoice ("voiceMode", 0, 3); break;
        case 3: setFloat ("pitchSemitones", 1.0f); setFloat ("formantSemitones", -0.5f); setFloat ("pitchDynamics", 48.0f); setFloat ("tracking", 78.0f); setFloat ("breathPreserve", 55.0f); setChoice ("voiceMode", 1, 3); break;
        case 4: setFloat ("pitchSemitones", 4.0f); setFloat ("formantSemitones", 3.0f); setFloat ("pitchDynamics", 90.0f); setFloat ("tracking", 50.0f); setFloat ("breathPreserve", 70.0f); setChoice ("voiceMode", 2, 3); break;
        default: setFloat ("pitchSemitones", 0.0f); setFloat ("formantSemitones", 0.0f); setFloat ("pitchDynamics", 75.0f); setFloat ("tracking", 70.0f); setFloat ("breathPreserve", 65.0f); setChoice ("voiceMode", 0, 3); break;
    }
}

