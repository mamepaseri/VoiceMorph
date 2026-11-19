#include "PluginEditor.h"
#include "ParameterMapping.h"

namespace
{
    const juce::Colour bg        { 0xff252b34 };
    const juce::Colour panel     { 0xff303944 };
    const juce::Colour panelHi   { 0xff3b4652 };
    const juce::Colour edge      { 0xff65717e };
    const juce::Colour text      { 0xffe7edf2 };
    const juce::Colour muted     { 0xffa6b2bd };
    const juce::Colour cyan      { 0xff39d6e5 };
    const juce::Colour cyanDim   { 0xff1e7782 };
}

VoiceMorphLookAndFeel::VoiceMorphLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff20262e));
    setColour (juce::Slider::textBoxOutlineColourId, edge.withAlpha (0.55f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff202731));
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
}

void VoiceMorphLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float sliderPos, float startAngle, float endAngle,
                                               juce::Slider& slider)
{
    juce::ignoreUnused (slider);
    const float side = static_cast<float> (juce::jmin (width, height));
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float outerR = side * 0.46f;
    const float ringR = side * 0.385f;
    const float faceR = side * 0.335f;

    // Soft panel shadow and machined outer ring.
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillEllipse (cx - outerR + 1.5f, cy - outerR + 4.0f, outerR * 2.0f, outerR * 2.0f);
    juce::ColourGradient metal (juce::Colour (0xffe0e5e9), cx - outerR, cy - outerR,
                                juce::Colour (0xff56616c), cx + outerR, cy + outerR, false);
    g.setGradientFill (metal);
    g.fillEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
    g.setColour (juce::Colour (0xff11161d));
    g.drawEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f, juce::jmax (1.0f, side * 0.012f));

    // Dark graphite inset and inner bevel.
    g.setColour (juce::Colour (0xff121820));
    g.fillEllipse (cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f);
    juce::ColourGradient faceGrad (juce::Colour (0xff46515e), cx - faceR * 0.8f, cy - faceR,
                                   juce::Colour (0xff242d38), cx + faceR, cy + faceR, false);
    g.setGradientFill (faceGrad);
    g.fillEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);
    g.setColour (juce::Colour (0xff788491).withAlpha (0.7f));
    g.drawEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    // Radial scale ticks.
    const float tickInner = outerR * 0.79f;
    const float tickOuter = outerR * 0.91f;
    const int ticks = 31;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (ticks - 1);
        const float a = startAngle + t * (endAngle - startAngle);
        const bool major = (i % 5 == 0);
        const float r0 = major ? tickInner - 1.5f : tickInner + 1.0f;
        const float r1 = major ? tickOuter + 1.0f : tickOuter - 1.0f;
        const juce::Point<float> p0 { cx + std::cos (a) * r0, cy + std::sin (a) * r0 };
        const juce::Point<float> p1 { cx + std::cos (a) * r1, cy + std::sin (a) * r1 };
        g.setColour (major ? muted : edge);
        g.drawLine ({ p0, p1 }, major ? 1.6f : 0.9f);
    }

    // Cyan active arc only. No warm/orange accents.
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    juce::Path arc;
    arc.addCentredArc (cx, cy, outerR * 0.80f, outerR * 0.80f, 0.0f,
                       startAngle, angle, true);
    g.setColour (cyan.withAlpha (0.95f));
    g.strokePath (arc, juce::PathStrokeType (juce::jmax (2.2f, side * 0.025f),
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Small position marker on the knob face.
    const float markerR = faceR * 0.77f;
    const float mx = cx + std::cos (angle) * markerR;
    const float my = cy + std::sin (angle) * markerR;
    g.setColour (juce::Colour (0xfff0f5f7));
    g.fillEllipse (mx - side * 0.018f, my - side * 0.018f, side * 0.036f, side * 0.036f);
    g.setColour (cyan.withAlpha (0.14f));
    g.drawEllipse (cx - outerR * 0.985f, cy - outerR * 0.985f,
                   outerR * 1.97f, outerR * 1.97f, 1.0f);
}

void VoiceMorphLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                float sliderPos, float, float,
                                                juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (slider);
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = x + width * 0.5f;
        const float top = static_cast<float> (y + 7);
        const float bottom = static_cast<float> (y + height - 7);
        g.setColour (juce::Colour (0xff151b22));
        g.fillRoundedRectangle (cx - 5.0f, top, 10.0f, bottom - top, 5.0f);
        g.setColour (cyan.withAlpha (0.20f));
        g.fillRoundedRectangle (cx - 2.0f, sliderPos, 4.0f, juce::jmax (0.0f, bottom - sliderPos), 2.0f);
        juce::Rectangle<float> cap (cx - 18.0f, sliderPos - 9.0f, 36.0f, 18.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 4.0f);
        juce::ColourGradient capGrad (juce::Colour (0xffe5eaee), cap.getX(), cap.getY(),
                                      juce::Colour (0xff7d8994), cap.getX(), cap.getBottom(), false);
        g.setGradientFill (capGrad);
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (juce::Colour (0xff18212a));
        g.drawRoundedRectangle (cap, 4.0f, 1.0f);
        g.setColour (cyan);
        g.drawLine (cx - 12.0f, sliderPos, cx + 12.0f, sliderPos, 1.2f);
    }
    else
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
    }
}

void VoiceMorphLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                    const juce::Colour&, bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool toggled = button.getToggleState();
    const auto top = toggled ? juce::Colour (0xff3f6871) : juce::Colour (0xff424d59);
    const auto bottom = toggled ? juce::Colour (0xff263d47) : juce::Colour (0xff252e38);
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 7.0f);
    juce::ColourGradient grad (highlighted ? top.brighter (0.12f) : top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (toggled ? cyan.withAlpha (0.9f) : edge.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 7.0f, toggled ? 1.4f : 1.0f);
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.13f)); g.fillRoundedRectangle (r, 7.0f); }
}

void VoiceMorphAudioProcessorEditor::WaveformMonitor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff1a222b));
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (edge.withAlpha (0.75f));
    g.drawRoundedRectangle (bounds, 10.0f, 1.0f);

    auto plot = bounds.reduced (18.0f, 12.0f);
    g.setColour (juce::Colour (0xff34414d).withAlpha (0.5f));
    for (int i = 1; i < 8; ++i)
    {
        const float xx = plot.getX() + plot.getWidth() * static_cast<float> (i) / 8.0f;
        g.drawVerticalLine (static_cast<int> (xx), plot.getY(), plot.getBottom());
    }
    g.drawHorizontalLine (static_cast<int> (plot.getCentreY()), plot.getX(), plot.getRight());

    VoiceMorphAudioProcessor::Waveform in {}, out {};
    processor.copyWaveforms (in, out);
    const float laneHeight = plot.getHeight() * 0.42f;
    const float firstMid = plot.getY() + laneHeight * 0.55f;
    const float secondMid = plot.getBottom() - laneHeight * 0.48f;
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (muted);
    g.drawText ("INPUT", juce::Rectangle<int> (static_cast<int> (plot.getX()), static_cast<int> (plot.getY()), 48, 16), juce::Justification::left);
    g.setColour (cyan);
    g.drawText ("OUTPUT", juce::Rectangle<int> (static_cast<int> (plot.getX()), static_cast<int> (plot.getBottom() - 16), 58, 16), juce::Justification::left);

    auto drawTrace = [&] (const auto& samples, float mid, juce::Colour colour)
    {
        juce::Path path;
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const float t = static_cast<float> (i) / static_cast<float> (samples.size() - 1);
            const float xx = plot.getX() + t * plot.getWidth();
            const float yy = mid - juce::jlimit (-1.0f, 1.0f, samples[i] * 2.4f) * laneHeight * 0.45f;
            if (i == 0) path.startNewSubPath (xx, yy); else path.lineTo (xx, yy);
        }
        g.setColour (colour.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    drawTrace (in, firstMid, muted);
    drawTrace (out, secondMid, cyan);
}

VoiceMorphAudioProcessorEditor::VoiceMorphAudioProcessorEditor (VoiceMorphAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), waveform (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (980, 620, 1600, 980);
    setSize (1180, 720);

    titleLabel.setText ("VOICEMORPH", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (23.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (titleLabel);

    statusLabel.setText ("ANALYZER  •  WAITING FOR INPUT", juce::dontSendNotification);
    statusLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    statusLabel.setColour (juce::Label::textColourId, cyan);
    addAndMakeVisible (statusLabel);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    presetLabel.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (presetLabel);
    presetBox.addItemList ({ "Default", "Natural Lift", "Soft / Airy", "Warm / Dark", "Character" }, 1);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    juce::Button* buttons[] = { &prevPresetButton, &nextPresetButton, &aButton, &bButton, &advancedButton,
                                  &bypassButton, &consonantButton, &transientButton, &deEssBypassButton };
    for (auto* button : buttons)
        addAndMakeVisible (*button);
    prevPresetButton.onClick = [this] { presetBox.setSelectedItemIndex (juce::jmax (0, presetBox.getSelectedItemIndex() - 1)); };
    nextPresetButton.onClick = [this] { presetBox.setSelectedItemIndex (juce::jmin (presetBox.getNumItems() - 1, presetBox.getSelectedItemIndex() + 1)); };
    aButton.setClickingTogglesState (false);
    bButton.setClickingTogglesState (false);
    aButton.onClick = [this] { processor.switchAB (false); aButton.setColour (juce::TextButton::buttonColourId, cyanDim); bButton.setColour (juce::TextButton::buttonColourId, panel); };
    bButton.onClick = [this] { processor.switchAB (true); bButton.setColour (juce::TextButton::buttonColourId, cyanDim); aButton.setColour (juce::TextButton::buttonColourId, panel); };
    advancedButton.onClick = [this] { advancedVisible = ! advancedVisible; resized(); repaint(); };

    auto setupLabel = [this] (juce::Label& label, const juce::String& name)
    {
        label.setText (name, juce::dontSendNotification);
        label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, muted);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    };
    setupLabel (inputLabel, "INPUT"); setupLabel (outputLabel, "OUTPUT");
    setupLabel (pitchLabel, "PITCH"); setupLabel (formantLabel, "FORMANT");
    setupLabel (trackingLabel, "TRACKING"); setupLabel (breathLabel, "BREATH");
    setupLabel (mixLabel, "VOICE MIX"); setupLabel (voiceModeLabel, "VOICE MODE");
    setupLabel (deEssLabel, "SIBILANCE"); setupLabel (deEssMixLabel, "DE-ESSER MIX");
    setupLabel (deEssRangeLabel, "DE-ESSER RANGE"); setupLabel (qualityLabel, "PROCESSING");

    for (auto* slider : { &inputGainSlider, &outputGainSlider, &voiceMixSlider, &pitchSlider, &formantSlider,
                          &trackingSlider, &breathSlider, &voiceModeSlider, &deEssIntensitySlider, &deEssMixSlider })
        addAndMakeVisible (*slider);
    configureFader (inputGainSlider);
    configureFader (outputGainSlider);
    configureRotary (voiceMixSlider, "%");
    configureRotary (pitchSlider, " st");
    configureRotary (formantSlider, " st");
    configureRotary (trackingSlider, "%");
    configureRotary (breathSlider, "%");
    configureRotary (voiceModeSlider);
    configureRotary (deEssIntensitySlider, "%");
    configureRotary (deEssMixSlider, "%");

    // AudioParameterChoice exposes the three choices as 0, 1, and 2.
    // Keep the slider, its text display, and typed values on that same scale.
    voiceModeSlider.setRange (0.0, 2.0, 1.0);
    voiceModeSlider.textFromValueFunction = [] (double v)
    {
        const int index = voice_morph::parameter_mapping::choiceValueToIndex (v, 3);
        return juce::StringArray { "NATURAL", "BALANCED", "PRECISE" }[index];
    };
    voiceModeSlider.valueFromTextFunction = [] (const juce::String& s)
    {
        if (s.containsIgnoreCase ("PRECISE")) return voice_morph::parameter_mapping::choiceIndexToValue (2, 3);
        if (s.containsIgnoreCase ("BALANCED")) return voice_morph::parameter_mapping::choiceIndexToValue (1, 3);
        return voice_morph::parameter_mapping::choiceIndexToValue (0, 3);
    };

    deEssRangeBox.addItemList ({ "Low-Hi  1-4 kHz", "Mid-Hi  2-4 kHz", "High  5-10 kHz", "Hi-End  5-20 kHz" }, 1);
    qualityBox.addItemList ({ "LIVE", "BALANCED", "STUDIO" }, 1);
    addAndMakeVisible (deEssRangeBox);
    addAndMakeVisible (qualityBox);
    addAndMakeVisible (waveform);

    inputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "inputGain", inputGainSlider);
    outputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "outputGain", outputGainSlider);
    voiceMixAttachment = std::make_unique<SliderAttachment> (processor.apvts, "voiceMix", voiceMixSlider);
    pitchAttachment = std::make_unique<SliderAttachment> (processor.apvts, "pitchSemitones", pitchSlider);
    formantAttachment = std::make_unique<SliderAttachment> (processor.apvts, "formantSemitones", formantSlider);
    trackingAttachment = std::make_unique<SliderAttachment> (processor.apvts, "tracking", trackingSlider);
    breathAttachment = std::make_unique<SliderAttachment> (processor.apvts, "breathPreserve", breathSlider);
    voiceModeAttachment = std::make_unique<SliderAttachment> (processor.apvts, "voiceMode", voiceModeSlider);
    deEssIntensityAttachment = std::make_unique<SliderAttachment> (processor.apvts, "deEssIntensity", deEssIntensitySlider);
    deEssMixAttachment = std::make_unique<SliderAttachment> (processor.apvts, "deEssMix", deEssMixSlider);
    bypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "bypass", bypassButton);
    consonantAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "consonantPreserve", consonantButton);
    transientAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "transientPreserve", transientButton);
    deEssBypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "deEssBypass", deEssBypassButton);
    deEssRangeAttachment = std::make_unique<ComboAttachment> (processor.apvts, "deEssRange", deEssRangeBox);
    qualityAttachment = std::make_unique<ComboAttachment> (processor.apvts, "qualityMode", qualityBox);

    startTimerHz (30);
}

VoiceMorphAudioProcessorEditor::~VoiceMorphAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoiceMorphAudioProcessorEditor::configureRotary (juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 22);
    slider.setTextValueSuffix (suffix);
    slider.setMouseDragSensitivity (170);
    slider.setLookAndFeel (&lookAndFeel);
}

void VoiceMorphAudioProcessorEditor::configureFader (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 74, 22);
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
    g.setColour (juce::Colours::black.withAlpha (0.24f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), corner);
    juce::ColourGradient grad (panel.brighter (0.10f), r.getX(), r.getY(), panel.darker (0.20f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, corner);
    g.setColour (edge.withAlpha (0.58f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void VoiceMorphAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    drawPanel (g, { bounds.getX(), bounds.getY(), bounds.getWidth(), 54.0f }, 10.0f);
    drawPanel (g, { bounds.getX(), bounds.getY() + 62.0f, 150.0f, bounds.getHeight() - 62.0f }, 12.0f);
    drawPanel (g, { bounds.getX() + 160.0f, bounds.getY() + 62.0f, bounds.getWidth() - 160.0f, bounds.getHeight() - 62.0f }, 12.0f);

    g.setColour (edge.withAlpha (0.55f));
    const float x = bounds.getX() + 174.0f;
    g.drawVerticalLine (static_cast<int> (x), bounds.getY() + 75.0f, bounds.getBottom() - 14.0f);

    g.setColour (muted);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("VOICE SHAPING", getLocalBounds().reduced (190, 70).removeFromTop (18), juce::Justification::left);

    if (! processor.isVoiceEngineConnected())
    {
        auto tag = juce::Rectangle<float> (getWidth() - 238.0f, 78.0f, 205.0f, 22.0f);
        g.setColour (juce::Colour (0xff3f5964));
        g.fillRoundedRectangle (tag, 6.0f);
        g.setColour (cyan);
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText ("PITCH / FORMANT ENGINE PENDING", tag.toNearestInt(), juce::Justification::centred);
    }

    if (advancedVisible)
    {
        auto drawer = juce::Rectangle<float> (getWidth() - 360.0f, getHeight() - 90.0f, 330.0f, 52.0f);
        g.setColour (juce::Colour (0xff1a222b));
        g.fillRoundedRectangle (drawer, 8.0f);
        g.setColour (cyan.withAlpha (0.75f));
        g.drawRoundedRectangle (drawer, 8.0f, 1.0f);
    }
}

void VoiceMorphAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto header = r.removeFromTop (54);
    titleLabel.setBounds (header.removeFromLeft (215).reduced (4, 5));
    statusLabel.setBounds (header.removeFromLeft (205).reduced (4, 14));
    presetLabel.setBounds (header.removeFromLeft (58));
    presetBox.setBounds (header.removeFromLeft (175).reduced (2, 10));
    prevPresetButton.setBounds (header.removeFromLeft (34).reduced (2, 8));
    nextPresetButton.setBounds (header.removeFromLeft (34).reduced (2, 8));
    header.removeFromLeft (18);
    aButton.setBounds (header.removeFromLeft (34).reduced (2, 8));
    bButton.setBounds (header.removeFromLeft (34).reduced (2, 8));
    header.removeFromLeft (18);
    bypassButton.setBounds (header.removeFromRight (102).reduced (2, 8));

    r.removeFromTop (10);
    auto left = r.removeFromLeft (150);
    auto inputArea = left.removeFromTop (left.getHeight() / 2 - 4);
    auto outputArea = left.removeFromBottom (left.getHeight());
    inputLabel.setBounds (inputArea.removeFromTop (24));
    inputGainSlider.setBounds (inputArea.reduced (24, 3));
    outputLabel.setBounds (outputArea.removeFromTop (24));
    outputGainSlider.setBounds (outputArea.reduced (24, 3));

    r.removeFromLeft (10);
    auto main = r;
    auto top = main.removeFromTop (230);
    const int half = top.getWidth() / 2;
    auto pitchArea = top.removeFromLeft (half).reduced (18, 4);
    auto formantArea = top.reduced (18, 4);
    pitchLabel.setBounds (pitchArea.removeFromTop (24));
    pitchSlider.setBounds (pitchArea.withSizeKeepingCentre (juce::jmin (205, pitchArea.getWidth()), juce::jmin (184, pitchArea.getHeight())));
    formantLabel.setBounds (formantArea.removeFromTop (24));
    formantSlider.setBounds (formantArea.withSizeKeepingCentre (juce::jmin (205, formantArea.getWidth()), juce::jmin (184, formantArea.getHeight())));

    main.removeFromTop (4);
    waveform.setBounds (main.removeFromTop (145).reduced (2, 0));
    main.removeFromTop (9);

    auto controls = main.removeFromTop (160);
    const int col = controls.getWidth() / 5;
    auto c1 = controls.removeFromLeft (col);
    auto c2 = controls.removeFromLeft (col);
    auto c3 = controls.removeFromLeft (col);
    auto c4 = controls.removeFromLeft (col);
    auto c5 = controls;
    auto placeSmall = [] (juce::Rectangle<int> area, juce::Label& label, juce::Slider& slider)
    {
        label.setBounds (area.removeFromTop (20));
        slider.setBounds (area.withSizeKeepingCentre (juce::jmin (116, area.getWidth()), juce::jmin (128, area.getHeight())));
    };
    placeSmall (c1.reduced (4, 0), trackingLabel, trackingSlider);
    placeSmall (c2.reduced (4, 0), breathLabel, breathSlider);
    placeSmall (c3.reduced (4, 0), mixLabel, voiceMixSlider);
    placeSmall (c4.reduced (4, 0), deEssLabel, deEssIntensitySlider);
    placeSmall (c5.reduced (4, 0), voiceModeLabel, voiceModeSlider);

    main.removeFromTop (4);
    auto bottom = main.removeFromTop (52);
    consonantButton.setBounds (bottom.removeFromLeft (250).reduced (2, 4));
    transientButton.setBounds (bottom.removeFromLeft (250).reduced (2, 4));
    advancedButton.setBounds (bottom.removeFromRight (100).reduced (2, 4));

    if (advancedVisible)
    {
        auto drawer = getLocalBounds().removeFromBottom (86).removeFromRight (360).reduced (18, 8);
        deEssRangeLabel.setBounds (drawer.removeFromLeft (120).removeFromTop (18));
        deEssRangeBox.setBounds (drawer.removeFromLeft (190).removeFromTop (26));
        deEssMixLabel.setBounds (drawer.removeFromLeft (0));
        deEssMixSlider.setBounds (drawer.removeFromLeft (0));
        qualityLabel.setBounds (drawer.removeFromLeft (0));
        qualityBox.setBounds (getLocalBounds().removeFromBottom (58).removeFromRight (130).reduced (5));
        deEssBypassButton.setBounds (getLocalBounds().removeFromBottom (58).removeFromRight (130).translated (-138, 0).reduced (5));
    }
    else
    {
        deEssRangeLabel.setBounds (0, 0, 0, 0);
        deEssRangeBox.setBounds (0, 0, 0, 0);
        deEssMixLabel.setBounds (0, 0, 0, 0);
        deEssMixSlider.setBounds (0, 0, 0, 0);
        qualityLabel.setBounds (0, 0, 0, 0);
        qualityBox.setBounds (0, 0, 0, 0);
        deEssBypassButton.setBounds (0, 0, 0, 0);
    }
}

void VoiceMorphAudioProcessorEditor::timerCallback()
{
    SpeechFeatures features;
    processor.getSpeechFeatures (features);

    if (features.rms < 0.0006f)
    {
        statusLabel.setText ("ANALYZER  •  WAITING FOR INPUT", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, muted);
    }
    else if (features.voiced >= 0.25f && features.pitchHz > 0.0f)
    {
        statusLabel.setText ("ANALYZER  •  F0 " + juce::String (features.pitchHz, 0)
                             + " Hz / " + juce::String (features.voiced * 100.0f, 0) + "%",
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, cyan);
    }
    else
    {
        statusLabel.setText ("ANALYZER  •  APERIODIC "
                             + juce::String (features.aperiodic * 100.0f, 0) + "%",
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, muted.brighter (0.25f));
    }

    waveform.repaint();
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
        case 1: setFloat ("pitchSemitones", 2.0f); setFloat ("formantSemitones", 1.5f); setFloat ("tracking", 72.0f); setFloat ("breathPreserve", 75.0f); setChoice ("voiceMode", 0, 3); break;
        case 2: setFloat ("pitchSemitones", 2.5f); setFloat ("formantSemitones", 2.5f); setFloat ("tracking", 62.0f); setFloat ("breathPreserve", 90.0f); setChoice ("voiceMode", 0, 3); break;
        case 3: setFloat ("pitchSemitones", 1.0f); setFloat ("formantSemitones", -0.5f); setFloat ("tracking", 78.0f); setFloat ("breathPreserve", 55.0f); setChoice ("voiceMode", 1, 3); break;
        case 4: setFloat ("pitchSemitones", 4.0f); setFloat ("formantSemitones", 3.0f); setFloat ("tracking", 50.0f); setFloat ("breathPreserve", 70.0f); setChoice ("voiceMode", 2, 3); break;
        default: setFloat ("pitchSemitones", 0.0f); setFloat ("formantSemitones", 0.0f); setFloat ("tracking", 70.0f); setFloat ("breathPreserve", 65.0f); setChoice ("voiceMode", 0, 3); break;
    }
}
