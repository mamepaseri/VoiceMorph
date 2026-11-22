// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "HybridVoicePath.h"
#include "SignalsmithPitchEngine.h"

// Preferred Signalsmith-backed hybrid engine facade. It binds the optional
// full-band Signalsmith processor to the periodic-only input supplied by
// HybridVoicePath, then restores the preserved residual at matching latency.
// If Signalsmith is not compiled or the transform call fails, it outputs a
// correctly timed dry signal instead of exposing a partially processed stem.
class SignalsmithHybridVoiceEngine final
{
public:
    bool prepare (int channels,
                  double sampleRate,
                  int maximumBlockSize,
                  float analysisBlockMilliseconds = 48.0f,
                  float processingIntervalMilliseconds = 12.0f,
                  bool spreadComputation = false) noexcept;
    void reset() noexcept;

    void setParameters (float pitchSemitones,
                        float formantSemitones,
                        float formantBaseHz) noexcept;

    bool processBlock (const float* const* input,
                       float* const* output,
                       int channels,
                       int numSamples,
                       const float* pitchHz,
                       const float* voiceConfidence,
                       const float* aperiodicConfidence,
                       float voiceMixPercent,
                       bool bypassTransform = false,
                       float breathPreservePercent = 65.0f,
                       bool preserveConsonants = true,
                       bool preserveTransients = true) noexcept;

    bool isPrepared() const noexcept { return prepared; }
    bool isTransformAvailable() const noexcept { return transformAvailable; }
    bool wasLastTransformSuccessful() const noexcept;
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    static bool processSignalsmith (void* context,
                                    const float* const* input,
                                    float* const* output,
                                    int channels,
                                    int numSamples) noexcept;

    SignalsmithPitchEngine pitchEngine;
    HybridVoicePath hybridPath;
    bool prepared = false;
    bool transformAvailable = false;
    int latencySamples = 0;
};
