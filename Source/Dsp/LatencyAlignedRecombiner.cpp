// SPDX-License-Identifier: AGPL-3.0-only
#include "LatencyAlignedRecombiner.h"

#include <algorithm>
#include <cmath>

bool LatencyAlignedRecombiner::prepare (int maximumLatencySamples, double sampleRate) noexcept
{
    prepared = false;

    // Ten seconds is an intentionally generous upper bound for a live voice
    // plug-in. It also prevents an invalid host/engine latency from forcing an
    // unbounded allocation inside a noexcept prepare method.
    if (maximumLatencySamples < 0 || ! std::isfinite (sampleRate)
        || sampleRate < 8000.0 || sampleRate > 192000.0
        || static_cast<double> (maximumLatencySamples) > sampleRate * 10.0)
        return false;

    const auto capacity = static_cast<std::size_t> (maximumLatencySamples) + 1u;
    try
    {
        residualDelay.assign (capacity, 0.0f);
        dryDelay.assign (capacity, 0.0f);
    }
    catch (...)
    {
        residualDelay.clear();
        dryDelay.clear();
        prepared = false;
        return false;
    }
    latencySamples = std::clamp (latencySamples, 0, maximumLatencySamples);
    mixSmoothingCoefficient = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.010f));
    prepared = true;
    reset();
    return true;
}

void LatencyAlignedRecombiner::reset() noexcept
{
    std::fill (residualDelay.begin(), residualDelay.end(), 0.0f);
    std::fill (dryDelay.begin(), dryDelay.end(), 0.0f);
    writeIndex = 0;
    samplesWritten = 0;
    smoothedVoiceMix = 1.0f;
}

void LatencyAlignedRecombiner::setLatencySamples (int samples) noexcept
{
    if (! prepared)
        return;
    latencySamples = std::clamp (samples, 0, static_cast<int> (residualDelay.size()) - 1);
    reset();
}

float LatencyAlignedRecombiner::readDelayed (const std::vector<float>& buffer) const noexcept
{
    if (latencySamples == 0)
        return buffer[writeIndex];

    if (samplesWritten < static_cast<std::size_t> (latencySamples))
        return 0.0f;

    const std::size_t size = buffer.size();
    const std::size_t readIndex = (writeIndex + size - static_cast<std::size_t> (latencySamples)) % size;
    return buffer[readIndex];
}

RecombinedSample LatencyAlignedRecombiner::processSample (float transformedVoiced,
                                                           float preservedResidual,
                                                           float originalInput,
                                                           float voiceMixPercent,
                                                           bool forceDryFallback) noexcept
{
    if (! prepared || residualDelay.empty())
        return { 0.0f, originalInput };

    const float safeResidual = std::isfinite (preservedResidual) ? preservedResidual : 0.0f;
    const float safeDry = std::isfinite (originalInput) ? originalInput : 0.0f;
    const float delayedResidual = latencySamples == 0 ? safeResidual : readDelayed (residualDelay);
    const float delayedDry = latencySamples == 0 ? safeDry : readDelayed (dryDelay);

    residualDelay[writeIndex] = safeResidual;
    dryDelay[writeIndex] = safeDry;
    writeIndex = (writeIndex + 1u) % residualDelay.size();
    samplesWritten = std::min (samplesWritten + 1u, residualDelay.size());

    const float targetMix = std::clamp (std::isfinite (voiceMixPercent) ? voiceMixPercent * 0.01f : 0.0f,
                                        0.0f, 1.0f);
    if (forceDryFallback)
        smoothedVoiceMix = 0.0f;
    else
        smoothedVoiceMix += mixSmoothingCoefficient * (targetMix - smoothedVoiceMix);

    const float transformed = (std::isfinite (transformedVoiced) ? transformedVoiced : 0.0f)
                            + delayedResidual;
    const float output = forceDryFallback
        ? delayedDry
        : delayedDry + (transformed - delayedDry) * smoothedVoiceMix;
    return { transformed, output };
}
