#pragma once

#include <array>
#include <cstddef>

struct SplitSample
{
    float periodic = 0.0f;
    float residual = 0.0f;
};

// First-pass, one-period predictive split. For strongly periodic voiced audio,
// a fractional one-period delayed copy is phase-aligned with the current sample.
// Averaging the two estimates a periodic component; the exact residual is kept
// as the detail path. Confidence fades the split back to the untouched input.
//
// This is NOT a perfect voiced/unvoiced separator. Real speech has jitter,
// changing F0, and simultaneous harmonic/noisy content; the parameters must be
// validated on dry microphone recordings before any stem is sent to a shifter.
class AdaptivePeriodicSplitter final
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    SplitSample processSample (float input,
                               float pitchHz,
                               float voiceConfidence,
                               bool preserveTransients = true) noexcept;

private:
    static constexpr std::size_t delayBufferSize = 4096;

    float readFractionalDelay (float delaySamples) const noexcept;

    double sampleRateHz = 48000.0;
    std::array<float, delayBufferSize> delayBuffer {};
    std::size_t writeIndex = 0;
    std::size_t samplesWritten = 0;
    float currentDelaySamples = 0.0f;
    float delaySmoothingCoefficient = 0.0f;
    float correlationSmoothingCoefficient = 0.0f;
    float correlatedInputEnergy = 0.0f;
    float correlatedDelayEnergy = 0.0f;
    float correlatedCrossEnergy = 0.0f;
    float amplitudeAttackCoefficient = 0.0f;
    float amplitudeReleaseCoefficient = 0.0f;
    float amplitudeEnvelope = 0.0f;
    float previousInput = 0.0f;
};
