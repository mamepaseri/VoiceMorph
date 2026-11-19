#include "../Source/Dsp/SpeechFeatureAnalyzer.h"
#include "../Source/Dsp/AdaptivePeriodicSplitter.h"
#include "../Source/Dsp/LatencyAlignedRecombiner.h"
#include "../Source/Dsp/HybridVoicePath.h"
#include "../Source/Dsp/PitchSynchronousShifter.h"
#include "../Source/Dsp/SignalsmithPitchEngine.h"
#include "../Source/Dsp/SignalsmithHybridVoiceEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr double pi = 3.14159265358979323846;

    template <typename Generator>
    SpeechFeatures runSignal (SpeechFeatureAnalyzer& analyzer, int samples,
                              const Generator& generator)
    {
        SpeechFeatures latest;
        for (int i = 0; i < samples; ++i)
        {
            const float sample = generator (i);
            if (analyzer.processSample (sample))
                latest = analyzer.getFeatures();
        }
        return latest;
    }

    bool require (bool condition, const std::string& message)
    {
        std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
        return condition;
    }

    struct FixedDelayTransform
    {
        int latency = 0;
        std::size_t writeIndex = 0;
        std::array<std::vector<float>, 2> delay;

        explicit FixedDelayTransform (int latencyIn, int channels)
            : latency (latencyIn)
        {
            for (int ch = 0; ch < channels; ++ch)
                delay[static_cast<std::size_t> (ch)].assign (static_cast<std::size_t> (latency), 0.0f);
        }
    };

    bool fixedDelayTransform (void* context,
                              const float* const* input,
                              float* const* output,
                              int channels,
                              int numSamples) noexcept
    {
        auto& state = *static_cast<FixedDelayTransform*> (context);
        if (input == nullptr || output == nullptr || channels < 1 || channels > 2)
            return false;
        for (int ch = 0; ch < channels; ++ch)
            if (input[ch] == nullptr || output[ch] == nullptr
                || static_cast<int> (state.delay[static_cast<std::size_t> (ch)].size()) != state.latency)
                return false;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                if (state.latency == 0)
                    output[ch][i] = input[ch][i];
                else
                {
                    auto& ring = state.delay[static_cast<std::size_t> (ch)];
                    output[ch][i] = ring[state.writeIndex];
                    ring[state.writeIndex] = input[ch][i];
                }
            }
            if (state.latency > 0)
                state.writeIndex = (state.writeIndex + 1u) % static_cast<std::size_t> (state.latency);
        }
        return true;
    }


    struct CaptureTransformState
    {
        double periodicEnergy = 0.0;
        double sampleCount = 0.0;
    };

    bool captureTransform (void* context,
                           const float* const* input,
                           float* const* output,
                           int channels,
                           int numSamples) noexcept
    {
        if (context == nullptr || input == nullptr || output == nullptr || channels < 1 || channels > 2)
            return false;
        auto& state = *static_cast<CaptureTransformState*> (context);
        for (int ch = 0; ch < channels; ++ch)
        {
            if (input[ch] == nullptr || output[ch] == nullptr)
                return false;
            for (int i = 0; i < numSamples; ++i)
            {
                const float value = input[ch][i];
                state.periodicEnergy += static_cast<double> (value) * value;
                state.sampleCount += 1.0;
                output[ch][i] = value;
            }
        }
        return true;
    }

    bool failedTransform (void*, const float* const*, float* const*, int, int) noexcept
    {
        return false;
    }
}

int main()
{
    bool ok = true;
    SpeechFeatureAnalyzer analyzer;
    analyzer.prepare (sampleRate);

    // Harmonic voiced signal, intended F0 = 132 Hz.
    auto voiced = runSignal (analyzer, static_cast<int> (sampleRate * 0.8), [] (int n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        return static_cast<float> (0.22 * std::sin (2.0 * pi * 132.0 * t)
                                 + 0.10 * std::sin (2.0 * pi * 264.0 * t)
                                 + 0.04 * std::sin (2.0 * pi * 396.0 * t));
    });
    ok &= require (voiced.pitchHz > 122.0f && voiced.pitchHz < 142.0f,
                   "132 Hz harmonic voice: F0 within ±10 Hz (got " + std::to_string (voiced.pitchHz) + ")");
    ok &= require (voiced.voiced > 0.55f, "harmonic voice produces a strong voiced confidence");

    analyzer.reset();
    std::mt19937 random (12345);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    auto fricative = runSignal (analyzer, static_cast<int> (sampleRate * 0.8), [&] (int)
    {
        return noise (random) * 0.12f;
    });
    ok &= require (fricative.periodicity < 0.55f, "noise-like fricative is not classified as strongly periodic");
    ok &= require (fricative.aperiodic > 0.20f, "noise-like fricative receives an aperiodic score");

    analyzer.reset();
    auto silence = runSignal (analyzer, static_cast<int> (sampleRate * 0.5), [] (int) { return 0.0f; });
    ok &= require (silence.voiced < 0.05f, "silence does not trigger voiced confidence");
    ok &= require (silence.aperiodic < 0.05f, "silence does not trigger aperiodic confidence");
    ok &= require (silence.pitchHz == 0.0f, "silence clears stale F0 instead of publishing a tiny positive value");

    analyzer.reset();
    for (int n = 0; n < static_cast<int> (sampleRate * 0.25); ++n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        analyzer.processSample (static_cast<float> (0.20 * std::sin (2.0 * pi * 132.0 * t)));
    }
    SpeechFeatures afterVoicedSilence;
    for (int n = 0; n < static_cast<int> (sampleRate * 0.35); ++n)
        if (analyzer.processSample (0.0f))
            afterVoicedSilence = analyzer.getFeatures();
    ok &= require (afterVoicedSilence.voiced < 0.08f && afterVoicedSilence.pitchHz == 0.0f,
                   "F0 and voiced confidence decay to zero after a voiced-to-silence transition");

    analyzer.reset();
    float maximumTransient = 0.0f;
    for (int n = 0; n < static_cast<int> (sampleRate * 0.30); ++n)
    {
        const float sample = n == static_cast<int> (sampleRate * 0.10) ? 0.8f : 0.0f;
        if (analyzer.processSample (sample))
            maximumTransient = std::max (maximumTransient, analyzer.getFeatures().transient);
    }
    ok &= require (maximumTransient > 0.70f, "short impulse produces a strong transient indicator");

    analyzer.reset();
    auto lowVoice = runSignal (analyzer, static_cast<int> (sampleRate * 0.8), [] (int n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        return static_cast<float> (0.24 * std::sin (2.0 * pi * 82.0 * t)
                                 + 0.08 * std::sin (2.0 * pi * 164.0 * t));
    });
    ok &= require (lowVoice.pitchHz > 73.0f && lowVoice.pitchHz < 91.0f,
                   "82 Hz low voice remains inside the supported F0 range (got " + std::to_string (lowVoice.pitchHz) + ")");

    analyzer.reset();
    auto veryLowVoice = runSignal (analyzer, static_cast<int> (sampleRate * 0.8), [] (int n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        return static_cast<float> (0.24 * std::sin (2.0 * pi * 68.0 * t)
                                 + 0.07 * std::sin (2.0 * pi * 136.0 * t));
    });
    ok &= require (veryLowVoice.pitchHz > 64.0f && veryLowVoice.pitchHz < 72.0f,
                   "68 Hz low-register voice is tracked near the lower operating limit (got "
                   + std::to_string (veryLowVoice.pitchHz) + ")");

    analyzer.reset();
    auto quietVoice = runSignal (analyzer, static_cast<int> (sampleRate * 0.8), [] (int n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        return static_cast<float> (0.0010 * std::sin (2.0 * pi * 132.0 * t));
    });
    ok &= require (quietVoice.pitchHz > 122.0f && quietVoice.pitchHz < 142.0f && quietVoice.voiced > 0.20f,
                   "quiet -60 dBFS-class voiced signal remains trackable (got F0 "
                   + std::to_string (quietVoice.pitchHz) + ", voiced "
                   + std::to_string (quietVoice.voiced) + ")");

    auto pitchTransitionTimeMs = [] (float trackingPercent)
    {
        SpeechFeatureAnalyzer tracker;
        tracker.prepare (sampleRate);
        tracker.setTracking (trackingPercent);
        const int firstToneSamples = static_cast<int> (sampleRate * 0.70);
        for (int n = 0; n < firstToneSamples; ++n)
        {
            const double t = static_cast<double> (n) / sampleRate;
            tracker.processSample (static_cast<float> (0.20 * std::sin (2.0 * pi * 132.0 * t)));
        }

        const int secondToneSamples = static_cast<int> (sampleRate * 0.50);
        for (int n = 0; n < secondToneSamples; ++n)
        {
            const double t = static_cast<double> (n) / sampleRate;
            if (tracker.processSample (static_cast<float> (0.20 * std::sin (2.0 * pi * 220.0 * t))))
            {
                const float f0 = tracker.getFeatures().pitchHz;
                if (f0 >= 205.0f && f0 <= 235.0f)
                    return 1000.0f * static_cast<float> (n) / static_cast<float> (sampleRate);
            }
        }
        return 1000.0f;
    };
    const float slowTrackingMs = pitchTransitionTimeMs (0.0f);
    const float fastTrackingMs = pitchTransitionTimeMs (100.0f);
    ok &= require (fastTrackingMs < slowTrackingMs && fastTrackingMs < 100.0f,
                   "Tracking control follows a 132-to-220 Hz pitch step faster at higher settings (slow "
                   + std::to_string (slowTrackingMs) + " ms, fast "
                   + std::to_string (fastTrackingMs) + " ms)");

    AdaptivePeriodicSplitter splitter;
    splitter.prepare (sampleRate);
    double inputEnergy = 0.0;
    double periodicResidualEnergy = 0.0;
    double reconstructionErrorEnergy = 0.0;
    constexpr int splitSamples = static_cast<int> (sampleRate * 0.4);
    for (int n = 0; n < splitSamples; ++n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        const float input = static_cast<float> (0.30 * std::sin (2.0 * pi * 132.0 * t));
        const auto split = splitter.processSample (input, 132.0f, 1.0f);
        if (n > static_cast<int> (sampleRate * 0.15))
        {
            inputEnergy += static_cast<double> (input) * input;
            periodicResidualEnergy += static_cast<double> (split.residual) * split.residual;
        }
        reconstructionErrorEnergy += static_cast<double> (input - split.periodic - split.residual)
                                   * (input - split.periodic - split.residual);
    }
    const float residualRatio = static_cast<float> (std::sqrt (periodicResidualEnergy / inputEnergy));
    ok &= require (residualRatio < 0.03f, "one-period split isolates a stable 132 Hz sinusoid (residual ratio "
                   + std::to_string (residualRatio) + ")");
    ok &= require (reconstructionErrorEnergy < 1.0e-10, "periodic + residual paths reconstruct the input without loss");

    splitter.reset();
    float noiseStemEnergy = 0.0f;
    float noiseInputEnergy = 0.0f;
    random.seed (9981);
    for (int n = 0; n < static_cast<int> (sampleRate * 0.2); ++n)
    {
        const float input = noise (random) * 0.10f;
        const auto split = splitter.processSample (input, 132.0f, 0.0f);
        noiseStemEnergy += split.periodic * split.periodic;
        noiseInputEnergy += input * input;
    }
    ok &= require (noiseStemEnergy < 1.0e-12f && noiseInputEnergy > 0.0f,
                   "zero voiced confidence leaves noise entirely in the residual path");

    // A stale tracker can briefly report a voiced F0 during the start of /s/ or
    // breath. Local one-period correlation must reject that noise even when the
    // caller accidentally supplies a full voiced-confidence value.
    splitter.reset();
    double stalePitchNoiseInputEnergy = 0.0;
    double stalePitchNoisePeriodicEnergy = 0.0;
    random.seed (51981);
    for (int n = 0; n < static_cast<int> (sampleRate * 0.30); ++n)
    {
        const float input = noise (random) * 0.10f;
        const auto split = splitter.processSample (input, 132.0f, 1.0f);
        if (n > static_cast<int> (sampleRate * 0.12))
        {
            stalePitchNoiseInputEnergy += static_cast<double> (input) * input;
            stalePitchNoisePeriodicEnergy += static_cast<double> (split.periodic) * split.periodic;
        }
    }
    const float stalePitchNoiseRatio = static_cast<float> (std::sqrt (stalePitchNoisePeriodicEnergy / stalePitchNoiseInputEnergy));
    ok &= require (stalePitchNoiseRatio < 0.08f,
                   "local periodicity gate rejects noise even with a stale 132 Hz / 100% voiced hint (ratio "
                   + std::to_string (stalePitchNoiseRatio) + ")");

    splitter.reset();
    float impulsePeriodic = 0.0f;
    float impulseInput = 0.0f;
    for (int n = 0; n < static_cast<int> (sampleRate * 0.25); ++n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        float input = static_cast<float> (0.20 * std::sin (2.0 * pi * 132.0 * t));
        if (n == static_cast<int> (sampleRate * 0.18))
            input += 0.8f;
        const auto split = splitter.processSample (input, 132.0f, 1.0f);
        if (n == static_cast<int> (sampleRate * 0.18))
        {
            impulsePeriodic = std::abs (split.periodic);
            impulseInput = std::abs (input);
        }
    }
    ok &= require (impulsePeriodic < impulseInput * 0.15f,
                   "sample-domain transient gate keeps a plosive-like impulse out of the periodic stem");

    AdaptivePeriodicSplitter transientOpenSplitter;
    AdaptivePeriodicSplitter transientProtectedSplitter;
    transientOpenSplitter.prepare (sampleRate);
    transientProtectedSplitter.prepare (sampleRate);
    float transientOpenPeriodic = 0.0f;
    float transientProtectedPeriodic = 0.0f;
    const int transientTestIndex = static_cast<int> (sampleRate * 0.18);
    for (int n = 0; n < static_cast<int> (sampleRate * 0.25); ++n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        float input = static_cast<float> (0.20 * std::sin (2.0 * pi * 132.0 * t));
        if (n == transientTestIndex)
            input += 0.8f;
        const auto protectedSplit = transientProtectedSplitter.processSample (input, 132.0f, 1.0f, true);
        const auto openSplit = transientOpenSplitter.processSample (input, 132.0f, 1.0f, false);
        if (n == transientTestIndex)
        {
            transientProtectedPeriodic = std::abs (protectedSplit.periodic);
            transientOpenPeriodic = std::abs (openSplit.periodic);
        }
    }
    ok &= require (transientProtectedPeriodic < transientOpenPeriodic * 0.01f,
                   "Transient Preserve switch can gate plosive energy out of the periodic candidate");

    // Breath Preserve / Consonant Preserve controls change how much aperiodic
    // confidence is allowed to suppress the periodic candidate.
    const auto measurePeriodicEnergy = [] (float breathPreservePercent, bool preserveConsonants)
    {
        HybridVoicePath path;
        if (! path.prepare (1, sampleRate, 256, 0, 0))
            return 0.0;
        CaptureTransformState capture;
        std::array<float, 256> inputBlock {};
        std::array<float, 256> outputBlock {};
        std::array<float, 256> pitchBlock {};
        std::array<float, 256> voicedBlock {};
        std::array<float, 256> aperiodicBlock {};
        pitchBlock.fill (132.0f);
        voicedBlock.fill (1.0f);
        aperiodicBlock.fill (1.0f);
        std::array<const float*, 1> inputPointers { inputBlock.data() };
        std::array<float*, 1> outputPointers { outputBlock.data() };
        for (int offset = 0; offset < 8192; offset += 256)
        {
            for (int i = 0; i < 256; ++i)
            {
                const double t = static_cast<double> (offset + i) / sampleRate;
                inputBlock[static_cast<std::size_t> (i)] = static_cast<float> (0.20 * std::sin (2.0 * pi * 132.0 * t));
            }
            if (! path.processBlock (inputPointers.data(), outputPointers.data(), 1, 256,
                                     pitchBlock.data(), voicedBlock.data(), aperiodicBlock.data(),
                                     100.0f, captureTransform, &capture,
                                     breathPreservePercent, preserveConsonants, false))
                return 0.0;
        }
        return capture.sampleCount > 0.0 ? capture.periodicEnergy / capture.sampleCount : 0.0;
    };
    const double breathUnprotectedEnergy = measurePeriodicEnergy (0.0f, true);
    const double breathProtectedEnergy = measurePeriodicEnergy (100.0f, true);
    const double consonantSwitchOffEnergy = measurePeriodicEnergy (100.0f, false);
    ok &= require (breathUnprotectedEnergy > breathProtectedEnergy * 100.0,
                   "Breath Preserve control suppresses highly aperiodic material from the transformed stem");
    ok &= require (consonantSwitchOffEnergy > breathProtectedEnergy * 100.0,
                   "Consonant Preserve switch independently bypasses the aperiodic protection gate");

    // Verify sample-accurate alignment of the transformed voiced path with
    // delayed residual and dry-reference paths. This models a transform whose
    // latency is measured by its engine; it does not guess Signalsmith latency.
    LatencyAlignedRecombiner recombiner;
    constexpr int testLatency = 137;
    ok &= require (recombiner.prepare (testLatency, sampleRate),
                   "latency-aligned recombiner allocates its delay lines outside the audio callback");
    recombiner.setLatencySamples (testLatency);
    float firstNonZero = -1.0f;
    double recombinationError = 0.0;
    for (int n = 0; n < 420; ++n)
    {
        const float dry = (n == 0 ? 0.25f : (n == 80 ? -0.10f : 0.0f));
        const float voicedStem = (n == 0 ? 0.15f : (n == 80 ? -0.06f : 0.0f));
        const float residual = dry - voicedStem;
        const float delayedVoiced = (n >= testLatency ? (n - testLatency == 0 ? 0.15f
                                      : (n - testLatency == 80 ? -0.06f : 0.0f)) : 0.0f);
        const auto output = recombiner.processSample (delayedVoiced, residual, dry, 100.0f);
        if (std::abs (output.output) > 1.0e-6f && firstNonZero < 0.0f)
            firstNonZero = static_cast<float> (n);
        if (n >= testLatency)
        {
            const float expected = (n - testLatency == 0 ? 0.25f
                                  : (n - testLatency == 80 ? -0.10f : 0.0f));
            recombinationError += static_cast<double> (output.output - expected)
                                * (output.output - expected);
        }
    }
    ok &= require (firstNonZero == static_cast<float> (testLatency),
                   "preserved residual and transformed voice begin at the same host-visible sample");
    ok &= require (recombinationError < 1.0e-10,
                   "zero-semitone hybrid path reconstructs the delayed dry input exactly");

    // End-to-end hybrid block orchestration with a deterministic mock engine.
    // The mock adds exactly the latency declared to HybridVoicePath; the test
    // verifies that the transformed periodic stem and preserved residual align.
    constexpr int hybridLatency = 137;
    HybridVoicePath hybridPath;
    ok &= require (hybridPath.prepare (2, sampleRate, 256, hybridLatency, hybridLatency),
                   "hybrid path preallocates two-channel split, transform, and recombination buffers");
    FixedDelayTransform mockTransform (hybridLatency, 2);
    std::array<std::vector<float>, 2> hybridInput {
        std::vector<float> (2048, 0.0f), std::vector<float> (2048, 0.0f) };
    std::array<std::vector<float>, 2> hybridOutput {
        std::vector<float> (2048, 0.0f), std::vector<float> (2048, 0.0f) };
    std::vector<float> hybridPitch (2048, 132.0f);
    std::vector<float> hybridVoiced (2048, 1.0f);
    std::vector<float> hybridAperiodic (2048, 0.05f);
    for (int n = 0; n < 2048; ++n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        hybridInput[0][static_cast<std::size_t> (n)] = static_cast<float> (
            0.20 * std::sin (2.0 * pi * 132.0 * t) + 0.03 * std::sin (2.0 * pi * 264.0 * t));
        hybridInput[1][static_cast<std::size_t> (n)] = static_cast<float> (
            0.17 * std::sin (2.0 * pi * 132.0 * t + 0.31) + 0.025 * std::sin (2.0 * pi * 396.0 * t));
    }
    const std::array<int, 4> hybridBlockSizes { 63, 127, 256, 91 };
    int hybridOffset = 0;
    std::size_t hybridBlockIndex = 0;
    while (hybridOffset < 2048)
    {
        const int block = std::min (hybridBlockSizes[hybridBlockIndex % hybridBlockSizes.size()],
                                    2048 - hybridOffset);
        std::array<const float*, 2> inPointers {
            hybridInput[0].data() + hybridOffset, hybridInput[1].data() + hybridOffset };
        std::array<float*, 2> outPointers {
            hybridOutput[0].data() + hybridOffset, hybridOutput[1].data() + hybridOffset };
        ok &= require (hybridPath.processBlock (inPointers.data(), outPointers.data(), 2, block,
                    hybridPitch.data() + hybridOffset, hybridVoiced.data() + hybridOffset,
                    hybridAperiodic.data() + hybridOffset, 100.0f,
                    fixedDelayTransform, &mockTransform),
                    "hybrid path accepts a variable-size streaming block");
        hybridOffset += block;
        ++hybridBlockIndex;
    }
    double hybridReconstructionError = 0.0;
    double hybridReferenceEnergy = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = hybridLatency; n < 2048; ++n)
        {
            const float expected = hybridInput[static_cast<std::size_t> (ch)][static_cast<std::size_t> (n - hybridLatency)];
            const float actual = hybridOutput[static_cast<std::size_t> (ch)][static_cast<std::size_t> (n)];
            hybridReconstructionError += static_cast<double> (actual - expected) * (actual - expected);
            hybridReferenceEnergy += static_cast<double> (expected) * expected;
        }
    const double hybridRelativeError = std::sqrt (hybridReconstructionError / hybridReferenceEnergy);
    ok &= require (hybridPath.wasLastTransformSuccessful() && hybridRelativeError < 2.0e-5,
                   "periodic transform + preserved residual reconstruct a delayed stereo input (relative error "
                   + std::to_string (hybridRelativeError) + ")");

    HybridVoicePath fallbackPath;
    ok &= require (fallbackPath.prepare (1, sampleRate, 128, hybridLatency, hybridLatency),
                   "hybrid fallback path prepares its declared host latency");
    FixedDelayTransform unusedMock (hybridLatency, 1);
    std::array<float, 128> fallbackInput {};
    std::array<float, 128> fallbackOutput {};
    std::array<const float*, 1> fallbackInputPointers { fallbackInput.data() };
    std::array<float*, 1> fallbackOutputPointers { fallbackOutput.data() };
    std::array<float, 128> fallbackF0 {};
    std::array<float, 128> fallbackVoiced {};
    for (int n = 0; n < 512; n += 128)
    {
        for (int i = 0; i < 128; ++i)
        {
            fallbackInput[static_cast<std::size_t> (i)] = n + i == 7 ? 0.5f : 0.0f;
            fallbackF0[static_cast<std::size_t> (i)] = 132.0f;
            fallbackVoiced[static_cast<std::size_t> (i)] = 1.0f;
        }
        fallbackPath.processBlock (fallbackInputPointers.data(), fallbackOutputPointers.data(), 1, 128,
                                   fallbackF0.data(), fallbackVoiced.data(), nullptr, 100.0f,
                                   failedTransform, nullptr);
        for (int i = 0; i < 128; ++i)
        {
            const int absolute = n + i;
            const float expected = absolute == hybridLatency + 7 ? 0.5f : 0.0f;
            if (absolute >= hybridLatency)
                hybridReconstructionError += static_cast<double> (fallbackOutput[static_cast<std::size_t> (i)] - expected)
                                           * (fallbackOutput[static_cast<std::size_t> (i)] - expected);
        }
    }
    ok &= require (! fallbackPath.wasLastTransformSuccessful(),
                   "failed transform callback is detected instead of exposing stale processed audio");
    ok &= require (hybridReconstructionError < 1.0e-10,
                   "failed transform callback falls back to the correctly latency-aligned dry path");

    fallbackPath.reset();
    double fallbackSineError = 0.0;
    for (int n = 0; n < 1024; n += 128)
    {
        for (int i = 0; i < 128; ++i)
        {
            const double t = static_cast<double> (n + i) / sampleRate;
            fallbackInput[static_cast<std::size_t> (i)] = static_cast<float> (0.18 * std::sin (2.0 * pi * 132.0 * t));
        }
        fallbackPath.processBlock (fallbackInputPointers.data(), fallbackOutputPointers.data(), 1, 128,
                                   fallbackF0.data(), fallbackVoiced.data(), nullptr, 100.0f,
                                   failedTransform, nullptr);
        for (int i = 0; i < 128; ++i)
        {
            const int absolute = n + i;
            if (absolute >= hybridLatency)
            {
                const double t = static_cast<double> (absolute - hybridLatency) / sampleRate;
                const float expected = static_cast<float> (0.18 * std::sin (2.0 * pi * 132.0 * t));
                fallbackSineError += static_cast<double> (fallbackOutput[static_cast<std::size_t> (i)] - expected)
                                   * (fallbackOutput[static_cast<std::size_t> (i)] - expected);
            }
        }
    }
    ok &= require (fallbackSineError < 1.0e-10,
                   "failed transform uses an immediate exact dry fallback without a residual-only fade-in");
    ok &= require (! fallbackPath.processBlock (fallbackInputPointers.data(), fallbackOutputPointers.data(), 1, 129,
                    fallbackF0.data(), fallbackVoiced.data(), nullptr, 100.0f, nullptr, nullptr),
                   "oversized host block is rejected before touching preallocated scratch buffers");

    LatencyAlignedRecombiner boundedRecombiner;
    ok &= require (! boundedRecombiner.prepare (480001, 48000.0),
                   "invalid ten-second-plus transform latency is rejected before allocation");
    LatencyAlignedRecombiner invalidatedRecombiner;
    ok &= require (invalidatedRecombiner.prepare (64, sampleRate),
                   "recombiner can be prepared before a failed re-prepare");
    ok &= require (! invalidatedRecombiner.prepare (480001, 48000.0)
                   && invalidatedRecombiner.processSample (0.0f, 0.0f, 0.37f, 100.0f).output == 0.37f,
                   "failed re-prepare leaves no stale latency path marked active");

    HybridVoicePath inplacePath;
    ok &= require (inplacePath.prepare (1, sampleRate, 64, 0, 0),
                   "zero-latency in-place test path prepares");
    std::array<float, 64> inplaceBuffer {};
    std::array<float, 64> inplaceOriginal {};
    std::array<const float*, 1> inplaceInput { inplaceBuffer.data() };
    std::array<float*, 1> inplaceOutput { inplaceBuffer.data() };
    std::array<float, 64> inplacePitch {};
    std::array<float, 64> inplaceVoiced {};
    for (int i = 0; i < 64; ++i)
    {
        inplaceBuffer[static_cast<std::size_t> (i)] = static_cast<float> (0.1 * std::sin (2.0 * pi * 132.0 * i / sampleRate));
        inplaceOriginal[static_cast<std::size_t> (i)] = inplaceBuffer[static_cast<std::size_t> (i)];
    }
    ok &= require (inplacePath.processBlock (inplaceInput.data(), inplaceOutput.data(), 1, 64,
                    inplacePitch.data(), inplaceVoiced.data(), nullptr, 100.0f, nullptr, nullptr),
                   "hybrid path accepts in-place host buffers when the transform is unavailable");
    float inplaceMaximumError = 0.0f;
    for (int i = 0; i < 64; ++i)
        inplaceMaximumError = std::max (inplaceMaximumError,
            std::abs (inplaceBuffer[static_cast<std::size_t> (i)] - inplaceOriginal[static_cast<std::size_t> (i)]));
    ok &= require (inplaceMaximumError < 1.0e-7f,
                   "unavailable-transform in-place fallback preserves the original samples immediately");

    recombiner.reset();
    recombiner.setLatencySamples (0);
    RecombinedSample dryOnly;
    for (int n = 0; n < 3000; ++n)
        dryOnly = recombiner.processSample (0.20f, 0.10f, 0.40f, 0.0f);
    ok &= require (std::abs (dryOnly.output - 0.40f) < 0.001f,
                   "0% Voice Mix smoothly settles on the dry reference without changing its level");

    // Exercise the original pitch-synchronous overlap-add stage on a stable
    // harmonic input. The engine reports its fixed 46 ms-class look-ahead; the
    // first 150 ms are excluded from F0 measurement to avoid startup transients.
    // The default portable test configuration deliberately builds without
    // downloading third-party DSP. Confirm the adapter reports that honestly.
#if ! VOICEMORPH_HAS_SIGNALSMITH
    SignalsmithPitchEngine signalsmithReference;
    ok &= require (! signalsmithReference.prepare (1, sampleRate, 512),
                   "Signalsmith adapter clearly reports unavailable when the optional backend is not compiled");
    ok &= require (! signalsmithReference.isAvailable() && signalsmithReference.totalLatencySamples() == 0,
                   "unavailable Signalsmith adapter does not invent a latency value");

    SignalsmithHybridVoiceEngine hybridSignalsmithInvalid;
    ok &= require (! hybridSignalsmithInvalid.prepare (1, sampleRate, 128, 12.0f, 4.0f),
                   "hybrid Signalsmith facade rejects an invalid analysis-window configuration");

    SignalsmithHybridVoiceEngine hybridSignalsmith;
    ok &= require (hybridSignalsmith.prepare (1, sampleRate, 128),
                   "hybrid Signalsmith facade prepares a dry fallback when its optional backend is absent");
    ok &= require (! hybridSignalsmith.isTransformAvailable() && hybridSignalsmith.getLatencySamples() == 0,
                   "missing Signalsmith backend is exposed as unavailable with zero reported latency");
    std::array<float, 128> facadeInput {}, facadeOutput {};
    std::array<float, 128> facadeF0 {}, facadeVoiced {};
    std::array<const float*, 1> facadeInPointers { facadeInput.data() };
    std::array<float*, 1> facadeOutPointers { facadeOutput.data() };
    for (int i = 0; i < 128; ++i)
    {
        facadeInput[static_cast<std::size_t> (i)] = static_cast<float> (0.12 * std::sin (2.0 * pi * 132.0 * i / sampleRate));
        facadeF0[static_cast<std::size_t> (i)] = 132.0f;
        facadeVoiced[static_cast<std::size_t> (i)] = 1.0f;
    }
    ok &= require (hybridSignalsmith.processBlock (facadeInPointers.data(), facadeOutPointers.data(), 1, 128,
                    facadeF0.data(), facadeVoiced.data(), nullptr, 100.0f),
                   "hybrid Signalsmith facade runs its block path when only dry fallback is available");
    float facadeError = 0.0f;
    for (int i = 0; i < 128; ++i)
        facadeError = std::max (facadeError, std::abs (facadeOutput[static_cast<std::size_t> (i)]
                                                    - facadeInput[static_cast<std::size_t> (i)]));
    ok &= require (facadeError < 1.0e-7f && ! hybridSignalsmith.wasLastTransformSuccessful(),
                   "hybrid Signalsmith facade keeps exact dry audio when the optional engine is disabled");
#endif

    PitchSynchronousShifter psola;
    ok &= require (psola.prepare (sampleRate),
                   "PSOLA shifter prepares bounded rings outside the audio callback");
    psola.setPitchShiftSemitones (7.0f);
    std::vector<float> shiftedSignal;
    shiftedSignal.reserve (static_cast<std::size_t> (sampleRate * 0.9));
    constexpr float sourcePitchHz = 132.0f;
    for (int n = 0; n < static_cast<int> (sampleRate * 0.9); ++n)
    {
        const double t = static_cast<double> (n) / sampleRate;
        const float input = static_cast<float> (0.22 * std::sin (2.0 * pi * sourcePitchHz * t)
                                              + 0.08 * std::sin (2.0 * pi * sourcePitchHz * 2.0 * t)
                                              + 0.03 * std::sin (2.0 * pi * sourcePitchHz * 3.0 * t));
        shiftedSignal.push_back (psola.processSample (input, sourcePitchHz, 1.0f));
    }
    const int measureStart = static_cast<int> (sampleRate * 0.20);
    const int measureEnd = static_cast<int> (sampleRate * 0.80);
    int shiftedPositiveCrossings = 0;
    float priorShifted = shiftedSignal[static_cast<std::size_t> (measureStart - 1)];
    for (int n = measureStart; n < measureEnd; ++n)
    {
        const float current = shiftedSignal[static_cast<std::size_t> (n)];
        if (priorShifted <= 0.0f && current > 0.0f)
            ++shiftedPositiveCrossings;
        priorShifted = current;
    }
    const float measuredShiftedPitch = static_cast<float> (shiftedPositiveCrossings)
                                     / (static_cast<float> (measureEnd - measureStart) / static_cast<float> (sampleRate));
    const float expectedShiftedPitch = sourcePitchHz * std::exp2 (7.0f / 12.0f);
    ok &= require (std::abs (measuredShiftedPitch - expectedShiftedPitch) < 4.0f,
                   "PSOLA raises 132 Hz harmonic input by +7 semitones (measured "
                   + std::to_string (measuredShiftedPitch) + " Hz, expected "
                   + std::to_string (expectedShiftedPitch) + " Hz)");
    ok &= require (psola.latencySamples() > 0
                   && std::abs (static_cast<float> (psola.latencySamples()) / static_cast<float> (sampleRate) - 0.0462f) < 0.0015f,
                   "PSOLA fixed look-ahead remains approximately 46 ms at 48 kHz");

    psola.reset();
    psola.setPitchShiftSemitones (0.0f);
    float silentPsolaPeak = 0.0f;
    for (int n = 0; n < static_cast<int> (sampleRate * 0.25); ++n)
        silentPsolaPeak = std::max (silentPsolaPeak, std::abs (psola.processSample (0.0f, 132.0f, 0.0f)));
    ok &= require (silentPsolaPeak == 0.0f, "PSOLA produces no residual tone when the periodic stem is silent");

    for (const double rate : { 44100.0, 96000.0, 192000.0 })
    {
        analyzer.prepare (rate);
        const auto feature = runSignal (analyzer, static_cast<int> (rate * 0.55), [rate] (int n)
        {
            const double t = static_cast<double> (n) / rate;
            return static_cast<float> (0.20 * std::sin (2.0 * pi * 176.0 * t)
                                     + 0.06 * std::sin (2.0 * pi * 352.0 * t));
        });
        ok &= require (feature.pitchHz > 166.0f && feature.pitchHz < 186.0f,
                       std::to_string (static_cast<int> (rate)) + " Hz sample-rate path tracks a 176 Hz voice (got "
                       + std::to_string (feature.pitchHz) + ")");

        AdaptivePeriodicSplitter rateSplitter;
        rateSplitter.prepare (rate);
        double rateInputEnergy = 0.0;
        double rateResidualEnergy = 0.0;
        for (int n = 0; n < static_cast<int> (rate * 0.30); ++n)
        {
            const double t = static_cast<double> (n) / rate;
            const float input = static_cast<float> (0.20 * std::sin (2.0 * pi * 68.0 * t));
            const auto split = rateSplitter.processSample (input, 68.0f, 1.0f);
            if (n > static_cast<int> (rate * 0.15))
            {
                rateInputEnergy += static_cast<double> (input) * input;
                rateResidualEnergy += static_cast<double> (split.residual) * split.residual;
            }
        }
        const float rateResidualRatio = static_cast<float> (std::sqrt (rateResidualEnergy / rateInputEnergy));
        ok &= require (rateResidualRatio < 0.03f, std::to_string (static_cast<int> (rate))
                       + " Hz splitter retains low-F0 periodicity (residual ratio "
                       + std::to_string (rateResidualRatio) + ")");
    }

    return ok ? 0 : 1;
}
