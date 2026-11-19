#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class VoiceMorphLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VoiceMorphLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override;
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
    class WaveformMonitor final : public juce::Component
    {
    public:
        explicit WaveformMonitor (VoiceMorphAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;
    private:
        VoiceMorphAudioProcessor& processor;
    };

    void timerCallback() override;
    void configureRotary (juce::Slider&, const juce::String& suffix = {});
    void configureFader (juce::Slider&);
    void addSectionLabel (juce::Label&, const juce::String&);
    void applyPreset (int index);
    void showAdvancedSettings();
    void refreshVoiceModeButton();
    void drawPanel (juce::Graphics&, juce::Rectangle<float>, float corner = 12.0f);

    VoiceMorphAudioProcessor& processor;
    VoiceMorphLookAndFeel lookAndFeel;

    juce::Label titleLabel, statusLabel, presetLabel;
    juce::ComboBox presetBox, deEssRangeBox, qualityBox;
    juce::TextButton prevPresetButton { "‹" }, nextPresetButton { "›" }, aButton { "A" }, bButton { "B" }, advancedButton { "ADVANCED" };
    juce::ToggleButton bypassButton { "BYPASS" }, consonantButton { "CONSONANT PRESERVE" }, transientButton { "TRANSIENT PRESERVE" }, deEssBypassButton { "DE-ESSER OFF" };
    juce::Slider inputGainSlider, outputGainSlider, voiceMixSlider, pitchSlider, formantSlider,
                 trackingSlider, breathSlider, deEssIntensitySlider, deEssMixSlider;
    juce::TextButton voiceModeButton { "NATURAL" };
    juce::Component::SafePointer<juce::CallOutBox> advancedCallout;
    juce::Label inputLabel, outputLabel, pitchLabel, formantLabel, trackingLabel, breathLabel,
               mixLabel, voiceModeLabel, deEssLabel, deEssMixLabel, deEssRangeLabel, qualityLabel;
    WaveformMonitor waveform;

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
