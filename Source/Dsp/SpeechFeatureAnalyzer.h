// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstddef>

// Lightweight, streaming speech-feature tracker.
// - No heap allocations in processSample().
// - Designed for the audio callback; analysis is updated every ~10 ms.
// - This does NOT pitch-shift audio and does NOT claim to perfectly separate
//   voiced/unvoiced components. It provides conservative control signals for
//   a later pitch/formant engine and preservation path.
struct SpeechFeatures
{
    float rms = 0.0f;
    float pitchHz = 0.0f;
    float periodicity = 0.0f;
    float voiced = 0.0f;
    float aperiodic = 0.0f;
    float highBandRatio = 0.0f;
    float transient = 0.0f;
};

class SpeechFeatureAnalyzer final
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setTracking (float amountPercent) noexcept;

    // Returns true when a new feature frame has been calculated.
    bool processSample (float monoSample) noexcept;
    const SpeechFeatures& getFeatures() const noexcept { return features; }

private:
    static constexpr std::size_t ringSize = 512;
    static constexpr std::size_t analysisSize = 384;

    void pushDecimatedSample (float sample) noexcept;
    void analyseFrame() noexcept;

    double sampleRateHz = 48000.0;
    double decimatedRateHz = 12000.0;
    std::array<float, ringSize> ring {};
    std::size_t writeIndex = 0;
    std::size_t samplesInRing = 0;

    int decimationFactor = 4;
    int decimationCount = 0;
    float decimationSum = 0.0f;

    int hopSamples = 480;
    int samplesSinceAnalysis = 0;
    float dcPreviousInput = 0.0f;
    float dcPreviousOutput = 0.0f;
    float dcCoefficient = 0.0f;
    float lowpassState = 0.0f;
    float lowpassCoefficient = 0.0f;
    float previousDc = 0.0f;
    float frameEnergy = 0.0f;
    float frameHighEnergy = 0.0f;
    float framePeakDelta = 0.0f;

    float smoothedVoiced = 0.0f;
    float smoothedAperiodic = 0.0f;
    float smoothedPitchHz = 0.0f;
    float trackingAmount = 0.70f;
    SpeechFeatures features;
};
