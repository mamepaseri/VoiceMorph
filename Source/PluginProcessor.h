#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "Dsp/SpeechFeatureAnalyzer.h"
#include "Dsp/PitchContourMapper.h"
#include "Dsp/AdaptivePeriodicSplitter.h"
#include "Dsp/SignalsmithHybridVoiceEngine.h"

class VoiceMorphAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr size_t waveformSize = 512;
    using Waveform = std::array<float, waveformSize>;
    using PitchContour = std::array<float, waveformSize>;
    using APVTS = juce::AudioProcessorValueTreeState;

    VoiceMorphAudioProcessor();
    ~VoiceMorphAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VoiceMorph"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    APVTS apvts;
    void copyWaveforms (Waveform& input, Waveform& output) const noexcept;
    void copyPitchContours (PitchContour& inputHz, PitchContour& targetHz) const noexcept;
    void switchAB (bool toB);
    bool isBSlotActive() const noexcept { return activeBSlot; }
    bool isVoiceEngineConnected() const noexcept { return voiceEngineAvailable.load (std::memory_order_relaxed); }
    void getSpeechFeatures (SpeechFeatures& destination) const noexcept;
    void getSplitLevels (float& periodicRms, float& residualRms) const noexcept;

private:
    static APVTS::ParameterLayout createParameterLayout();
    static float readParam (const APVTS& state, const char* id, float fallback) noexcept;
    void publishSpeechFeatures() noexcept;

    std::array<std::atomic<float>, waveformSize> inputWaveform {};
    std::array<std::atomic<float>, waveformSize> outputWaveform {};
    std::atomic<size_t> waveformWriteIndex { 0 };
    std::array<std::atomic<float>, waveformSize> inputPitchContour {};
    std::array<std::atomic<float>, waveformSize> targetPitchContour {};
    std::atomic<size_t> pitchContourWriteIndex { 0 };

    SpeechFeatureAnalyzer speechAnalyzer;
    PitchContourMapper pitchContourMapper;
    float currentPitchDynamicsOffset = 0.0f;
    AdaptivePeriodicSplitter periodicSplitter;
    SignalsmithHybridVoiceEngine hybridVoiceEngine;
    std::vector<float> framePitch;
    std::vector<float> frameVoiced;
    std::vector<float> frameAperiodic;
    std::vector<float> frameInputMono;
    std::array<const float*, 2> hybridInputPointers {};
    std::array<float*, 2> hybridOutputPointers {};
    int hybridMaximumBlockSize = 1;
    std::atomic<bool> voiceEngineAvailable { false };
    float periodicEnergyAccumulator = 0.0f;
    float residualEnergyAccumulator = 0.0f;
    int splitSamplesInFrame = 0;
    std::atomic<float> publishedRms { 0.0f };
    std::atomic<float> publishedPitchHz { 0.0f };
    std::atomic<float> publishedPeriodicity { 0.0f };
    std::atomic<float> publishedVoiced { 0.0f };
    std::atomic<float> publishedAperiodic { 0.0f };
    std::atomic<float> publishedHighBandRatio { 0.0f };
    std::atomic<float> publishedTransient { 0.0f };
    std::atomic<float> publishedPeriodicRms { 0.0f };
    std::atomic<float> publishedResidualRms { 0.0f };

    double currentSampleRate = 44100.0;

    juce::ValueTree slotA;
    juce::ValueTree slotB;
    bool activeBSlot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceMorphAudioProcessor)
};


