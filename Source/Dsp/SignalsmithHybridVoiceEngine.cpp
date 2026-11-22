// SPDX-License-Identifier: AGPL-3.0-only
#include "SignalsmithHybridVoiceEngine.h"

#include <cmath>

bool SignalsmithHybridVoiceEngine::prepare (int channels,
                                            double sampleRate,
                                            int maximumBlockSize,
                                            float analysisBlockMilliseconds,
                                            float processingIntervalMilliseconds,
                                            bool spreadComputation) noexcept
{
    prepared = false;
    transformAvailable = false;
    latencySamples = 0;

    if (channels < 1 || channels > 2 || maximumBlockSize < 1
        || ! std::isfinite (sampleRate) || sampleRate < 8000.0 || sampleRate > 192000.0
        || ! std::isfinite (analysisBlockMilliseconds)
        || ! std::isfinite (processingIntervalMilliseconds)
        || analysisBlockMilliseconds < 24.0f || analysisBlockMilliseconds > 120.0f
        || processingIntervalMilliseconds < 4.0f
        || processingIntervalMilliseconds > analysisBlockMilliseconds * 0.5f)
        return false;

    try
    {
        transformAvailable = pitchEngine.prepare (channels, sampleRate, maximumBlockSize,
                                                   analysisBlockMilliseconds,
                                                   processingIntervalMilliseconds,
                                                   spreadComputation);
    }
    catch (...)
    {
        transformAvailable = false;
    }
    latencySamples = transformAvailable ? pitchEngine.totalLatencySamples() : 0;

    // A missing optional backend is not a fatal preparation error: the wrapper
    // remains available as a transparent dry path until the backend is enabled.
    if (! hybridPath.prepare (channels, sampleRate, maximumBlockSize,
                              latencySamples, latencySamples))
    {
        transformAvailable = false;
        latencySamples = 0;
        return false;
    }

    prepared = true;
    return true;
}

void SignalsmithHybridVoiceEngine::reset() noexcept
{
    if (! prepared)
        return;
    pitchEngine.reset();
    hybridPath.reset();
}

void SignalsmithHybridVoiceEngine::setParameters (float pitchSemitones,
                                                  float formantSemitones,
                                                  float formantBaseHz) noexcept
{
    if (prepared && transformAvailable)
        pitchEngine.setParameters (pitchSemitones, formantSemitones, formantBaseHz);
}

bool SignalsmithHybridVoiceEngine::processBlock (const float* const* input,
                                                 float* const* output,
                                                 int channels,
                                                 int numSamples,
                                                 const float* pitchHz,
                                                 const float* voiceConfidence,
                                                 const float* aperiodicConfidence,
                                                 float voiceMixPercent,
                                                 bool bypassTransform,
                                                 float breathPreservePercent,
                                                 bool preserveConsonants,
                                                 bool preserveTransients) noexcept
{
    if (! prepared)
        return false;

    return hybridPath.processBlock (input, output, channels, numSamples,
                                    pitchHz, voiceConfidence, aperiodicConfidence,
                                    voiceMixPercent,
                                    (! bypassTransform && transformAvailable)
                                        ? &SignalsmithHybridVoiceEngine::processSignalsmith : nullptr,
                                    &pitchEngine, breathPreservePercent,
                                    preserveConsonants, preserveTransients);
}

bool SignalsmithHybridVoiceEngine::wasLastTransformSuccessful() const noexcept
{
    return prepared && transformAvailable && hybridPath.wasLastTransformSuccessful();
}

bool SignalsmithHybridVoiceEngine::processSignalsmith (void* context,
                                                       const float* const* input,
                                                       float* const* output,
                                                       int channels,
                                                       int numSamples) noexcept
{
    if (context == nullptr)
        return false;
    return static_cast<SignalsmithPitchEngine*> (context)->process (input, output, channels, numSamples);
}
