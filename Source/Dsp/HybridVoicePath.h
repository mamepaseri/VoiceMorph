#pragma once

#include "AdaptivePeriodicSplitter.h"
#include "LatencyAlignedRecombiner.h"

#include <array>
#include <cstddef>
#include <vector>

// Block-level orchestration for a hybrid voice path:
//   input -> periodic/residual split -> external pitch/formant engine
//         -> latency-aligned residual + dry recombination.
//
// The transform callback must preserve a streaming sample timeline and report
// its total input+output latency through prepare(). This class never guesses it.
// All scratch memory is allocated by prepare(); processBlock() is allocation-free.
class HybridVoicePath final
{
public:
    using TransformBlock = bool (*) (void* context,
                                     const float* const* input,
                                     float* const* output,
                                     int channels,
                                     int numSamples) noexcept;

    bool prepare (int channels,
                  double sampleRate,
                  int maximumBlockSize,
                  int maximumLatencySamples,
                  int transformLatencySamples) noexcept;
    void reset() noexcept;

    // pitchHz / voiceConfidence may be null; null means unvoiced / preserve dry.
    // transform == nullptr or a false callback result safely falls back to the
    // delayed dry path. A false return from processBlock means invalid arguments
    // or a block larger than the preallocated capacity.
    bool processBlock (const float* const* input,
                       float* const* output,
                       int channels,
                       int numSamples,
                       const float* pitchHz,
                       const float* voiceConfidence,
                       const float* aperiodicConfidence,
                       float voiceMixPercent,
                       TransformBlock transform,
                       void* transformContext,
                       float breathPreservePercent = 65.0f,
                       bool preserveConsonants = true,
                       bool preserveTransients = true) noexcept;

    int getLatencySamples() const noexcept { return transformLatencySamples; }
    int getMaximumBlockSize() const noexcept { return maximumBlockSize; }
    int getNumChannels() const noexcept { return preparedChannels; }
    bool isPrepared() const noexcept { return prepared; }
    bool wasLastTransformSuccessful() const noexcept { return lastTransformSuccessful; }

private:
    int preparedChannels = 0;
    int maximumBlockSize = 0;
    int transformLatencySamples = 0;
    bool prepared = false;
    bool lastTransformSuccessful = false;

    std::array<AdaptivePeriodicSplitter, 2> splitters;
    std::array<LatencyAlignedRecombiner, 2> recombiners;
    std::array<std::vector<float>, 2> periodicScratch;
    std::array<std::vector<float>, 2> residualScratch;
    std::array<std::vector<float>, 2> transformedScratch;
    std::array<const float*, 2> transformInputPointers {};
    std::array<float*, 2> transformOutputPointers {};
};
