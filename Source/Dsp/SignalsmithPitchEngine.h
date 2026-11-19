#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>

#ifndef VOICEMORPH_HAS_SIGNALSMITH
#define VOICEMORPH_HAS_SIGNALSMITH 0
#endif

#if VOICEMORPH_HAS_SIGNALSMITH
#include <signalsmith-stretch/signalsmith-stretch.h>
#endif

// A reusable adapter around Signalsmith Stretch. In production it is invoked only
// on the periodic candidate produced by HybridVoicePath; breaths, frication and
// transient detail remain on the delayed residual path. The full-band engine is
// still only a baseline and must not be fed the original microphone signal.
class SignalsmithPitchEngine final
{
public:
    SignalsmithPitchEngine();
    ~SignalsmithPitchEngine();

    SignalsmithPitchEngine (const SignalsmithPitchEngine&) = delete;
    SignalsmithPitchEngine& operator= (const SignalsmithPitchEngine&) = delete;

    bool prepare (int channels, double sampleRate, int maxBlockSize,
                  float analysisBlockMilliseconds = 48.0f,
                  float processingIntervalMilliseconds = 12.0f,
                  bool spreadComputation = false);
    void reset() noexcept;

    // Call from the audio thread immediately before process(), or marshal changes
    // from atomics in the host processor. formantBaseHz should be a robust F0
    // estimate; 140 Hz is only a neutral initial estimate, not a tracker.
    void setParameters (float pitchSemitones,
                        float formantSemitones,
                        float formantBaseHz) noexcept;

    // The pointer arrays must contain at least `channels` entries. Input and
    // output must not alias. All scratch storage is allocated in prepare().
    bool process (const float* const* input,
                  float* const* output,
                  int channels,
                  int numSamples) noexcept;

    int inputLatencySamples() const noexcept;
    int outputLatencySamples() const noexcept;
    int totalLatencySamples() const noexcept;
    int configuredBlockSamples() const noexcept;
    int configuredIntervalSamples() const noexcept;
    bool isAvailable() const noexcept;

private:
#if VOICEMORPH_HAS_SIGNALSMITH
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    int preparedChannels = 0;
    int scratchCapacity = 0;
    int blockSizeSamples = 0;
    int intervalSizeSamples = 0;
    int totalLatency = 0;
    std::vector<std::vector<float>> inputScratch;
    std::vector<std::vector<float>> outputScratch;
    std::vector<float*> inputPointers;
    std::vector<float*> outputPointers;
    double sampleRateHz = 44100.0;
    float lastPitchSemitones = -1000.0f;
    float lastFormantSemitones = -1000.0f;
    float lastFormantBaseHz = -1.0f;
    bool prepared = false;
#endif
};
