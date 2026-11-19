#include "SignalsmithPitchEngine.h"

#include <algorithm>
#include <cmath>
#include <vector>

SignalsmithPitchEngine::SignalsmithPitchEngine() = default;
SignalsmithPitchEngine::~SignalsmithPitchEngine() = default;

bool SignalsmithPitchEngine::prepare (int channels, double sampleRate, int maxBlockSize,
                                      float analysisBlockMilliseconds,
                                      float processingIntervalMilliseconds,
                                      bool spreadComputation)
{
#if VOICEMORPH_HAS_SIGNALSMITH
    if (channels < 1 || channels > 2 || ! std::isfinite (sampleRate)
        || sampleRate < 8000.0 || sampleRate > 192000.0 || maxBlockSize < 1
        || ! std::isfinite (analysisBlockMilliseconds)
        || ! std::isfinite (processingIntervalMilliseconds)
        || analysisBlockMilliseconds < 24.0f || analysisBlockMilliseconds > 120.0f
        || processingIntervalMilliseconds < 4.0f
        || processingIntervalMilliseconds > analysisBlockMilliseconds * 0.5f)
        return false;

    prepared = false;
    preparedChannels = channels;
    sampleRateHz = sampleRate;

    // Manual 48 ms / 12 ms setup is the first low-latency candidate. The
    // actual total must still be queried and measured on the built dependency;
    // it is not a claim that the end-to-end plug-in is already below 50 ms.
    blockSizeSamples = std::max (1, static_cast<int> (std::lround (
        sampleRate * static_cast<double> (analysisBlockMilliseconds) * 0.001)));
    intervalSizeSamples = std::max (1, static_cast<int> (std::lround (
        sampleRate * static_cast<double> (processingIntervalMilliseconds) * 0.001)));
    if (intervalSizeSamples >= blockSizeSamples)
        return false;
    stretch.configure (channels, blockSizeSamples, intervalSizeSamples, spreadComputation);
    totalLatency = stretch.inputLatency() + stretch.outputLatency();
    lastPitchSemitones = -1000.0f;
    lastFormantSemitones = -1000.0f;
    lastFormantBaseHz = -1.0f;
    scratchCapacity = std::max ({ maxBlockSize, stretch.blockSamples(), blockSizeSamples + intervalSizeSamples });

    inputScratch.assign (static_cast<size_t> (channels),
                         std::vector<float> (static_cast<size_t> (scratchCapacity), 0.0f));
    outputScratch.assign (static_cast<size_t> (channels),
                          std::vector<float> (static_cast<size_t> (scratchCapacity), 0.0f));
    inputPointers.resize (static_cast<size_t> (channels));
    outputPointers.resize (static_cast<size_t> (channels));
    for (int ch = 0; ch < channels; ++ch)
    {
        inputPointers[static_cast<size_t> (ch)] = inputScratch[static_cast<size_t> (ch)].data();
        outputPointers[static_cast<size_t> (ch)] = outputScratch[static_cast<size_t> (ch)].data();
    }

    stretch.reset();
    prepared = true;
    setParameters (0.0f, 0.0f, 140.0f);
    return true;
#else
    (void) channels;
    (void) sampleRate;
    (void) maxBlockSize;
    (void) analysisBlockMilliseconds;
    (void) processingIntervalMilliseconds;
    (void) spreadComputation;
    return false;
#endif
}

void SignalsmithPitchEngine::reset() noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    if (prepared)
        stretch.reset();
#endif
}

void SignalsmithPitchEngine::setParameters (float pitchSemitones,
                                            float formantSemitones,
                                            float formantBaseHz) noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    if (! prepared)
        return;

    pitchSemitones = std::clamp (std::isfinite (pitchSemitones) ? pitchSemitones : 0.0f,
                                 -24.0f, 24.0f);
    formantSemitones = std::clamp (std::isfinite (formantSemitones) ? formantSemitones : 0.0f,
                                  -12.0f, 12.0f);
    formantBaseHz = std::clamp (std::isfinite (formantBaseHz) ? formantBaseHz : 140.0f,
                                45.0f, 1000.0f);

    if (pitchSemitones != lastPitchSemitones)
    {
        stretch.setTransposeSemitones (pitchSemitones);
        lastPitchSemitones = pitchSemitones;
    }

    if (formantSemitones != lastFormantSemitones)
    {
        // `false` keeps this as a direct, independent formant-envelope move.
        stretch.setFormantSemitones (formantSemitones, false);
        lastFormantSemitones = formantSemitones;
    }

    if (formantBaseHz != lastFormantBaseHz)
    {
        // Signalsmith expects its formant base normalised to sample rate.
        stretch.setFormantBase (formantBaseHz / static_cast<float> (sampleRateHz));
        lastFormantBaseHz = formantBaseHz;
    }
#else
    (void) pitchSemitones;
    (void) formantSemitones;
    (void) formantBaseHz;
#endif
}

bool SignalsmithPitchEngine::process (const float* const* input,
                                      float* const* output,
                                      int channels,
                                      int numSamples) noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    if (! prepared || input == nullptr || output == nullptr
        || channels != preparedChannels || numSamples < 0)
        return false;

    int offset = 0;
    while (offset < numSamples)
    {
        const int count = std::min (scratchCapacity, numSamples - offset);
        for (int ch = 0; ch < channels; ++ch)
        {
            if (input[ch] == nullptr || output[ch] == nullptr)
                return false;
            auto& in = inputScratch[static_cast<size_t> (ch)];
            std::copy_n (input[ch] + offset, count, in.data());
        }

        stretch.process (inputPointers.data(), count, outputPointers.data(), count);

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto& out = outputScratch[static_cast<size_t> (ch)];
            std::copy_n (out.data(), count, output[ch] + offset);
        }
        offset += count;
    }
    return true;
#else
    (void) input;
    (void) output;
    (void) channels;
    (void) numSamples;
    return false;
#endif
}

int SignalsmithPitchEngine::inputLatencySamples() const noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    return prepared ? stretch.inputLatency() : 0;
#else
    return 0;
#endif
}

int SignalsmithPitchEngine::outputLatencySamples() const noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    return prepared ? stretch.outputLatency() : 0;
#else
    return 0;
#endif
}

int SignalsmithPitchEngine::totalLatencySamples() const noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    return prepared ? totalLatency : 0;
#else
    return 0;
#endif
}

int SignalsmithPitchEngine::configuredBlockSamples() const noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    return prepared ? blockSizeSamples : 0;
#else
    return 0;
#endif
}

int SignalsmithPitchEngine::configuredIntervalSamples() const noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    return prepared ? intervalSizeSamples : 0;
#else
    return 0;
#endif
}

bool SignalsmithPitchEngine::isAvailable() const noexcept
{
#if VOICEMORPH_HAS_SIGNALSMITH
    return prepared;
#else
    return false;
#endif
}
