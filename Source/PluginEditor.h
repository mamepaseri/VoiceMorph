#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class VoiceMorphLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VoiceMorphLookAndFeel();
    void setLightTheme (bool shouldUseLightTheme);
    bool isLightTheme() const noexcept { return lightTheme; }
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
private:
    bool lightTheme = false;
};

class VoiceMorphAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit VoiceMorphAudioProcessorEditor (VoiceMorphAudioProcessor&);
    ~VoiceMorphAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class PitchContourMonitor final : public juce::Component
    {
    public:
        PitchContourMonitor (VoiceMorphAudioProcessor& p, VoiceMorphLookAndFeel& l) : processor (p), lookAndFeel (l) {}
        void paint (juce::Graphics&) override;
    private:
        VoiceMorphAudioProcessor& processor;
        VoiceMorphLookAndFeel& lookAndFeel;
    };

    void timerCallback() override;
    void configureRotary (juce::Slider&, const juce::String& suffix = {}, bool lcdReadout = false);
    void configureFader (juce::Slider&);
    void addSectionLabel (juce::Label&, const juce::String&);
    void applyPreset (int index);
    void showAdvancedSettings();
    void refreshVoiceModeButtons();
    void setVoiceMode (int index);
    void refreshTheme();
    void drawPanel (juce::Graphics&, juce::Rectangle<float>, float corner = 12.0f);

    VoiceMorphAudioProcessor& processor;
    VoiceMorphLookAndFeel lookAndFeel;

    juce::Label titleLabel, statusLabel, presetLabel;
    juce::ComboBox presetBox, deEssRangeBox, qualityBox;
    juce::TextButton prevPresetButton { "<" }, nextPresetButton { ">" }, aButton { "A" }, bButton { "B" },
                    advancedButton { "ADVANCED" }, darkThemeButton { "DARK" }, lightThemeButton { "LIGHT" };
    juce::ToggleButton bypassButton { "BYPASS" }, consonantButton { "CONSONANT PRESERVE" }, transientButton { "TRANSIENT PRESERVE" }, deEssBypassButton { "DE-ESSER OFF" };
    juce::Slider inputGainSlider, outputGainSlider, voiceMixSlider, pitchSlider, formantSlider,
                 trackingSlider, breathSlider, deEssIntensitySlider, deEssMixSlider;
    juce::TextButton naturalModeButton { "NATURAL" }, balancedModeButton { "BALANCED" }, preciseModeButton { "PRECISE" };
    std::unique_ptr<juce::DocumentWindow> advancedWindow;
    juce::Label inputLabel, outputLabel, pitchLabel, formantLabel, trackingLabel, breathLabel,
               mixLabel, voiceModeLabel, deEssLabel, deEssMixLabel, deEssRangeLabel, qualityLabel;
    PitchContourMonitor pitchMonitor;

    using SliderAttachment = VoiceMorphAudioProcessor::APVTS::SliderAttachment;
    using ButtonAttachment = VoiceMorphAudioProcessor::APVTS::ButtonAttachment;
    using ComboAttachment = VoiceMorphAudioProcessor::APVTS::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> inputAttachment, outputAttachment, voiceMixAttachment,
        pitchAttachment, formantAttachment, trackingAttachment, breathAttachment,
        deEssIntensityAttachment, deEssMixAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, consonantAttachment, transientAttachment, deEssBypassAttachment;
    std::unique_ptr<ComboAttachment> deEssRangeAttachment, qualityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceMorphAudioProcessorEditor)
};

