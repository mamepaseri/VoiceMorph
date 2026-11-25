Attempting to perform the InitializeDefaultDrives operation on the 'FileSystem' provider failed.
#include "HybridVoicePath.h"

#include <algorithm>
#include <cmath>

bool HybridVoicePath::prepare (int channels,
                               double sampleRate,
                               int maximumBlockSizeIn,
                               int maximumLatencySamples,
                               int transformLatencySamplesIn) noexcept
{
    prepared = false;
    lastTransformSuccessful = false;

    if (channels < 1 || channels > 2 || maximumBlockSizeIn < 1
        || maximumLatencySamples < 0 || transformLatencySamplesIn < 0
        || transformLatencySamplesIn > maximumLatencySamples
        || ! std::isfinite (sampleRate) || sampleRate < 8000.0 || sampleRate > 192000.0)
        return false;

    try
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto index = static_cast<std::size_t> (ch);
            splitters[index].prepare (sampleRate);
            if (! recombiners[index].prepare (maximumLatencySamples, sampleRate))
                return false;
            recombiners[index].setLatencySamples (transformLatencySamplesIn);

            periodicScratch[index].assign (static_cast<std::size_t> (maximumBlockSizeIn), 0.0f);
            residualScratch[index].assign (static_cast<std::size_t> (maximumBlockSizeIn), 0.0f);
            transformedScratch[index].assign (static_cast<std::size_t> (maximumBlockSizeIn), 0.0f);
            transformInputPointers[index] = periodicScratch[index].data();
            transformOutputPointers[index] = transformedScratch[index].data();
        }
    }
    catch (...)
    {
        for (auto& buffer : periodicScratch) buffer.clear();
        for (auto& buffer : residualScratch) buffer.clear();
        for (auto& buffer : transformedScratch) buffer.clear();
        return false;
    }

    preparedChannels = channels;
    maximumBlockSize = maximumBlockSizeIn;
    transformLatencySamples = transformLatencySamplesIn;
    prepared = true;
    reset();
    return true;
}

void HybridVoicePath::reset() noexcept
{
    if (! prepared)
        return;

    for (int ch = 0; ch < preparedChannels; ++ch)
    {
        const auto index = static_cast<std::size_t> (ch);
        splitters[index].reset();
        recombiners[index].reset();
        std::fill (transformedScratch[index].begin(), transformedScratch[index].end(), 0.0f);
    }
    lastTransformSuccessful = false;
}

bool HybridVoicePath::processBlock (const float* const* input,
                                    float* const* output,
                                    int channels,
                                    int numSamples,
                                    const float* pitchHz,
                                    const float* voiceConfidence,
                                    const float* aperiodicConfidence,
                                    float voiceMixPercent,
                                    TransformBlock transform,
                                    void* transformContext,
                                    float breathPreservePercent,
                                    bool preserveConsonants,
                                    bool preserveTransients) noexcept
{
    if (! prepared || input == nullptr || output == nullptr || channels != preparedChannels
        || numSamples < 0 || numSamples > maximumBlockSize)
        return false;

    for (int ch = 0; ch < channels; ++ch)
        if (input[ch] == nullptr || output[ch] == nullptr)
            return false;

    const float breathPreserve = std::clamp (std::isfinite (breathPreservePercent)
        ? breathPreservePercent * 0.01f : 0.65f, 0.0f, 1.0f);

    // Build both stems before writing any output. This permits in-place operation
    // where input[ch] and output[ch] point to the same host buffer.
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto index = static_cast<std::size_t> (ch);
        auto& periodic = periodicScratch[index];
        auto& residual = residualScratch[index];
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float f0 = pitchHz != nullptr ? pitchHz[sample] : 0.0f;
            const float voicedRaw = voiceConfidence != nullptr ? voiceConfidence[sample] : 0.0f;
            const float aperiodicRaw = aperiodicConfidence != nullptr ? aperiodicConfidence[sample] : 0.0f;
            const float voiced = std::clamp (std::isfinite (voicedRaw) ? voicedRaw : 0.0f, 0.0f, 1.0f);
            const float aperiodic = std::clamp (std::isfinite (aperiodicRaw) ? aperiodicRaw : 0.0f, 0.0f, 1.0f);
            const float aperiodicProtection = preserveConsonants ? breathPreserve * aperiodic : 0.0f;
            const auto stems = splitters[index].processSample (input[ch][sample], f0, voiced,
                                                                preserveTransients, aperiodicProtection);
            // Classified aperiodic detail and detected transients are sent
            // around the pitch shifter. Uncertain pitched/unknown material is
            // sent through the transform instead of being returned as dry audio.
            const float breathResidualGain = preserveConsonants ? breathPreserve * aperiodic : 0.0f;
            const float transientResidualGain = preserveTransients ? stems.transient : 0.0f;
            // If classification is weak, the residual still contains source
            // voice. Move that uncertain material into the pitch/formant path;
            // only positively identified breath, consonants, and transients may
            // bypass the transform. This retains boundary energy without
            // leaking the unshifted vowel into a fully wet output.
            const float confidenceSquared = stems.classificationConfidence
                                          * stems.classificationConfidence;
            const float confidenceFallback = 1.0f - confidenceSquared * confidenceSquared;
            const float transientDetail = preserveTransients ? stems.transient : 0.0f;
            const float transformFallback = std::clamp (
                confidenceFallback * (1.0f - aperiodic) * (1.0f - transientDetail), 0.0f, 1.0f);
            periodic[static_cast<std::size_t> (sample)] = stems.periodic
                + stems.residual * transformFallback;
            const float residualGain = std::clamp (
                std::max (breathResidualGain, transientResidualGain) * (1.0f - transformFallback),
                0.0f, 1.0f);
            residual[static_cast<std::size_t> (sample)] = stems.residual * residualGain;
        }
    }

    lastTransformSuccessful = false;
    if (transform != nullptr)
    {
        lastTransformSuccessful = transform (transformContext,
                                             transformInputPointers.data(),
                                             transformOutputPointers.data(),
                                             channels,
                                             numSamples);
    }

    const float effectiveMix = lastTransformSuccessful ? voiceMixPercent : 0.0f;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto index = static_cast<std::size_t> (ch);
        const auto& residual = residualScratch[index];
        const auto& transformed = transformedScratch[index];
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto recombined = recombiners[index].processSample (
                lastTransformSuccessful ? transformed[static_cast<std::size_t> (sample)] : 0.0f,
                residual[static_cast<std::size_t> (sample)],
                input[ch][sample],
                effectiveMix,
                ! lastTransformSuccessful);
            output[ch][sample] = recombined.output;
        }
    }

    return true;
}
