#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Pitch-synchronous overlap-add shifter for an already-extracted periodic
// voice stem. It changes synthesis-mark spacing while keeping each source grain
// at its original sample length, so local harmonic/formant structure is not
// resampled. This is a pitch-only prototype; independent formant movement is a
// separate stage and is intentionally not claimed here.
//
// A fixed look-ahead allows each 2-period source grain to be complete before it
// is written into the future output ring. All buffers are allocated in prepare();
// processSample() is allocation-free.
class PitchSynchronousShifter final
{
public:
    bool prepare (double sampleRate, float minimumPitchHz = 65.0f,
                  float maximumPitchHz = 450.0f) noexcept;
    void reset() noexcept;
    void setPitchShiftSemitones (float semitones) noexcept;

    float processSample (float periodicInput, float pitchHz,
                        float voicedConfidence) noexcept;

    int latencySamples() const noexcept { return fixedLatencySamples; }
    float currentPitchRatio() const noexcept { return smoothedPitchRatio; }

private:
    static constexpr std::size_t markHistorySize = 64;

    std::size_t ringIndex (std::int64_t absoluteIndex) const noexcept;
    void addPitchMark (std::int64_t sampleIndex) noexcept;
    bool findNearestPitchMark (std::int64_t targetIndex,
                               float maximumDistance,
                               std::int64_t& result) const noexcept;
    void scheduleGrain (std::int64_t synthesisIndex,
                        float grainPitchHz) noexcept;

    double sampleRateHz = 48000.0;
    float minimumPitchHz = 65.0f;
    float maximumPitchHz = 450.0f;
    float maximumPeriodSamples = 0.0f;
    float ratioSmoothingCoefficient = 0.0f;
    float pitchSmoothingCoefficient = 0.0f;
    float smoothedPitchHz = 0.0f;
    float targetPitchRatio = 1.0f;
    float smoothedPitchRatio = 1.0f;
    float synthesisPhase = 0.0f;
    float previousInput = 0.0f;

    std::vector<float> inputRing;
    std::vector<float> pitchRing;
    std::vector<float> confidenceRing;
    std::vector<float> outputAccumulation;
    std::vector<float> outputWeights;
    std::vector<std::uint8_t> synthesisEvents;

    std::array<std::int64_t, markHistorySize> pitchMarks {};
    std::size_t markWriteIndex = 0;
    std::size_t markCount = 0;
    std::int64_t sampleClock = 0;
    int lookAheadSamples = 0;
    int fixedLatencySamples = 0;
    bool hasSynthesisPhase = false;
    bool prepared = false;
};
