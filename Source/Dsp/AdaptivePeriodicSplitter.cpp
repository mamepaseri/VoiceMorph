#include "AdaptivePeriodicSplitter.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float minimumPitchHz = 65.0f;
    constexpr float maximumPitchHz = 450.0f;
}

void AdaptivePeriodicSplitter::prepare (double sampleRate) noexcept
{
    sampleRateHz = std::clamp (sampleRate, 8000.0, 192000.0);
    delaySmoothingCoefficient = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRateHz) * 0.008f));
    correlationSmoothingCoefficient = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRateHz) * 0.012f));
    amplitudeAttackCoefficient = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRateHz) * 0.002f));
    amplitudeReleaseCoefficient = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRateHz) * 0.050f));
    reset();
}

void AdaptivePeriodicSplitter::reset() noexcept
{
    delayBuffer.fill (0.0f);
    writeIndex = 0;
    samplesWritten = 0;
    currentDelaySamples = static_cast<float> (sampleRateHz / 140.0);
    correlatedInputEnergy = 0.0f;
    correlatedDelayEnergy = 0.0f;
    correlatedCrossEnergy = 0.0f;
    amplitudeEnvelope = 0.0f;
    previousInput = 0.0f;
}

float AdaptivePeriodicSplitter::readFractionalDelay (float delaySamples) const noexcept
{
    // The write index points to the slot that will receive the current sample.
    const float readPosition = static_cast<float> (writeIndex) - delaySamples;
    const int base = static_cast<int> (std::floor (readPosition));
    const float fraction = readPosition - static_cast<float> (base);
    const auto at = [this] (int index) noexcept
    {
        int wrapped = index % static_cast<int> (delayBufferSize);
        if (wrapped < 0)
            wrapped += static_cast<int> (delayBufferSize);
        return delayBuffer[static_cast<std::size_t> (wrapped)];
    };

    // Four-point Lagrange interpolation gives better phase accuracy than linear
    // interpolation for the higher harmonics that contribute to vowel timbre.
    const float xm1 = at (base - 1);
    const float x0 = at (base);
    const float x1 = at (base + 1);
    const float x2 = at (base + 2);
    const float c0 = -fraction * (fraction - 1.0f) * (fraction - 2.0f) / 6.0f;
    const float c1 = (fraction + 1.0f) * (fraction - 1.0f) * (fraction - 2.0f) / 2.0f;
    const float c2 = -(fraction + 1.0f) * fraction * (fraction - 2.0f) / 2.0f;
    const float c3 = (fraction + 1.0f) * fraction * (fraction - 1.0f) / 6.0f;
    return c0 * xm1 + c1 * x0 + c2 * x1 + c3 * x2;
}

SplitSample AdaptivePeriodicSplitter::processSample (float input,
                                                      float pitchHz,
                                                      float voiceConfidence,
                                                      bool preserveTransients) noexcept
{
    const float safeInput = std::isfinite (input) ? input : 0.0f;
    const bool validPitch = std::isfinite (pitchHz) && pitchHz >= minimumPitchHz && pitchHz <= maximumPitchHz;

    // Apply transient protection in the sample domain. The 10 ms analysis frame
    // necessarily reports its transient peak after that frame has elapsed; this
    // immediate slope/envelope detector protects the actual plosive/click sample.
    const float magnitude = std::abs (safeInput);
    const float envelopeCoefficient = magnitude > amplitudeEnvelope
        ? amplitudeAttackCoefficient : amplitudeReleaseCoefficient;
    amplitudeEnvelope += envelopeCoefficient * (magnitude - amplitudeEnvelope);
    const float slopeRatio = std::abs (safeInput - previousInput) / (amplitudeEnvelope + 1.0e-5f);
    previousInput = safeInput;
    const float sampleTransient = std::clamp ((slopeRatio - 0.80f) / 1.60f, 0.0f, 1.0f);
    const float targetDelay = validPitch
        ? static_cast<float> (sampleRateHz) / std::clamp (pitchHz, minimumPitchHz, maximumPitchHz)
        : currentDelaySamples;

    // 8 ms one-pole tracking keeps normal vibrato responsive without making
    // single-frame F0 estimates produce abrupt sample-to-sample delay jumps.
    if (validPitch)
        currentDelaySamples += (targetDelay - currentDelaySamples) * delaySmoothingCoefficient;

    const int requiredHistory = static_cast<int> (std::ceil (currentDelaySamples)) + 4;
    float periodicEstimate = 0.0f;
    float localPeriodicity = 0.0f;
    if (validPitch && samplesWritten >= static_cast<std::size_t> (requiredHistory))
    {
        const float delayed = readFractionalDelay (currentDelaySamples);
        periodicEstimate = 0.5f * (safeInput + delayed);

        // A tracked F0 alone is not enough to identify a periodic sample: a
        // consonant can follow a voiced vowel while the 10 ms tracker still
        // reports the previous pitch. Measure whether the current sample and
        // its one-period predecessor actually agree over a short local window.
        // Uncorrelated breath/frication therefore stays in the residual path,
        // even during the analyzer's voiced-confidence release.
        const float corrAlpha = correlationSmoothingCoefficient;
        correlatedInputEnergy += corrAlpha * (safeInput * safeInput - correlatedInputEnergy);
        correlatedDelayEnergy += corrAlpha * (delayed * delayed - correlatedDelayEnergy);
        correlatedCrossEnergy += corrAlpha * (safeInput * delayed - correlatedCrossEnergy);
        const float denominator = std::sqrt (correlatedInputEnergy * correlatedDelayEnergy + 1.0e-12f);
        localPeriodicity = denominator > 1.0e-6f
            ? std::clamp (correlatedCrossEnergy / denominator, 0.0f, 1.0f) : 0.0f;
    }
    else
    {
        // Do not carry a stale periodicity estimate across unvoiced gaps.
        correlatedInputEnergy = 0.0f;
        correlatedDelayEnergy = 0.0f;
        correlatedCrossEnergy = 0.0f;
    }

    const float baseConfidence = validPitch && std::isfinite (voiceConfidence)
        ? std::clamp (voiceConfidence, 0.0f, 1.0f) : 0.0f;
    const float correlationGate = std::clamp ((localPeriodicity - 0.18f) / 0.72f, 0.0f, 1.0f);
    const float transientGate = preserveTransients ? (1.0f - sampleTransient) : 1.0f;
    const float confidence = baseConfidence * transientGate * correlationGate;
    const float periodic = confidence * periodicEstimate;
    const float residual = safeInput - periodic;

    delayBuffer[writeIndex] = safeInput;
    writeIndex = (writeIndex + 1) % delayBufferSize;
    samplesWritten = std::min (samplesWritten + 1, delayBufferSize);

    return { periodic, residual, sampleTransient };
}
