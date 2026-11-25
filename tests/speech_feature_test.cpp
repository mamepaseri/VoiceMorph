#include "../Source/Dsp/SpeechFeatureAnalyzer.h"
#include "../Source/Dsp/PitchContourMapper.h"
#include "../Source/Dsp/AdaptivePeriodicSplitter.h"
#include "../Source/Dsp/LatencyAlignedRecombiner.h"
#include "../Source/Dsp/HybridVoicePath.h"
#include "../Source/Dsp/PitchSynchronousShifter.h"
#include "../Source/Dsp/SignalsmithPitchEngine.h"
#include "../Source/Dsp/SignalsmithHybridVoiceEngine.h"
#include "../Source/ParameterMapping.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

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

    struct ScaledTransform
    {
        float gain = 1.0f;
    };

    bool scaledTransform (void* context,
                          const float* const* input,
                          float* const* output,
                          int channels,
                          int numSamples) noexcept
    {
        if (context == nullptr || input == nullptr || output == nullptr || channels < 1 || channels > 2)
            return false;
        const float gain = static_cast<ScaledTransform*> (context)->gain;
        for (int ch = 0; ch < channels; ++ch)
        {
            if (input[ch] == nullptr || output[ch] == nullptr)
                return false;
            for (int i = 0; i < numSamples; ++i)
                output[ch][i] = input[ch][i] * gain;
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

#if VOICEMORPH_HAS_SIGNALSMITH
    struct SignalsmithToneRun
    {
        bool success = false;
        int inputLatency = 0;
        int outputLatency = 0;
        std::vector<float> output;
    };

    SignalsmithToneRun renderSignalsmithTone()
    {
        SignalsmithToneRun result;
        SignalsmithPitchEngine engine;
        constexpr int blockSize = 256;
        const int totalSamples = static_cast<int> (sampleRate * 1.2);
        if (! engine.prepare (1, sampleRate, blockSize))
            return result;

        result.inputLatency = engine.inputLatencySamples();
        result.outputLatency = engine.outputLatencySamples();
        result.output.resize (static_cast<std::size_t> (totalSamples), 0.0f);
        engine.setParameters (7.0f, 0.0f, 132.0f);

        std::array<float, blockSize> inputBlock {}, outputBlock {};
        std::array<const float*, 1> inputPointers { inputBlock.data() };
        std::array<float*, 1> outputPointers { outputBlock.data() };
        for (int offset = 0; offset < totalSamples; )
        {
            const int count = std::min (blockSize, totalSamples - offset);
            for (int i = 0; i < count; ++i)
            {
                const int absolute = offset + i;
                const double time = static_cast<double> (absolute) / sampleRate;
                inputBlock[static_cast<std::size_t> (i)] = static_cast<float> (
                    0.22 * std::sin (2.0 * pi * 132.0 * time)
                    + 0.08 * std::sin (2.0 * pi * 264.0 * time)
                    + 0.03 * std::sin (2.0 * pi * 396.0 * time));
            }
            if (! engine.process (inputPointers.data(), outputPointers.data(), 1, count))
                return result;
            std::copy_n (outputBlock.data(), count, result.output.data() + offset);
            offset += count;
        }

        result.success = true;
        return result;
    }

    struct HybridVoiceToneRun
    {
        bool prepared = false;
        bool everyTransformSucceeded = true;
        float measuredPitchHz = 0.0f;
        float outputRms = 0.0f;
        float analyzedPitchHz = 0.0f;
    };

    HybridVoiceToneRun renderHybridVoiceTone (bool useAnalyzer)
    {
        HybridVoiceToneRun result;
        constexpr int blockSize = 480; // Match the processor's 10 ms control hops.
        const int totalSamples = static_cast<int> (sampleRate * 1.2);
        SignalsmithHybridVoiceEngine engine;
        SpeechFeatureAnalyzer analyzer;
        analyzer.prepare (sampleRate);
        analyzer.setTracking (70.0f);
        result.prepared = engine.prepare (1, sampleRate, blockSize)
                       && engine.isTransformAvailable();
        if (! result.prepared)
            return result;

        engine.setParameters (7.0f, 0.0f, 132.0f);
        std::vector<float> input (static_cast<std::size_t> (blockSize), 0.0f);
        std::vector<float> output (static_cast<std::size_t> (blockSize), 0.0f);
        std::vector<float> pitch (static_cast<std::size_t> (blockSize), 132.0f);
        std::vector<float> voiced (static_cast<std::size_t> (blockSize), 1.0f);
        std::vector<float> aperiodic (static_cast<std::size_t> (blockSize), 0.0f);
        std::array<const float*, 1> inputPointers { input.data() };
        std::array<float*, 1> outputPointers { output.data() };
        std::vector<float> rendered (static_cast<std::size_t> (totalSamples), 0.0f);
        std::vector<float> analyzedPitches;

        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const int absolute = offset + i;
                const double time = static_cast<double> (absolute) / sampleRate;
                input[static_cast<std::size_t> (i)] = static_cast<float> (
                    0.22 * std::sin (2.0 * pi * 132.0 * time)
                    + 0.08 * std::sin (2.0 * pi * 264.0 * time)
                    + 0.03 * std::sin (2.0 * pi * 396.0 * time));
                if (useAnalyzer)
                {
                    analyzer.processSample (input[static_cast<std::size_t> (i)]);
                    const auto& features = analyzer.getFeatures();
                    pitch[static_cast<std::size_t> (i)] = features.pitchHz;
                    voiced[static_cast<std::size_t> (i)] = features.voiced;
                    aperiodic[static_cast<std::size_t> (i)] = features.aperiodic;
                    if (features.pitchHz >= 65.0f && features.pitchHz <= 450.0f
                        && features.voiced >= 0.25f)
                        analyzedPitches.push_back (features.pitchHz);
                }
            }

            if (! engine.processBlock (inputPointers.data(), outputPointers.data(), 1, blockSize,
                                       pitch.data(), voiced.data(), aperiodic.data(), 100.0f,
                                       false, useAnalyzer ? 65.0f : 0.0f,
                                       useAnalyzer, useAnalyzer)
                || ! engine.wasLastTransformSuccessful())
            {
                result.everyTransformSucceeded = false;
                break;
            }
            std::copy_n (output.data(), blockSize, rendered.data() + offset);
        }

        if (! result.everyTransformSucceeded)
            return result;

        constexpr int measureStart = static_cast<int> (sampleRate * 0.25);
        constexpr int measureEnd = static_cast<int> (sampleRate * 1.05);
        int crossings = 0;
        double energy = 0.0;
        float previous = rendered[static_cast<std::size_t> (measureStart - 1)];
        for (int n = measureStart; n < measureEnd; ++n)
        {
            const float current = rendered[static_cast<std::size_t> (n)];
            if (previous <= 0.0f && current > 0.0f)
                ++crossings;
            energy += static_cast<double> (current) * current;
            previous = current;
        }
        const float duration = static_cast<float> (measureEnd - measureStart) / static_cast<float> (sampleRate);
        result.measuredPitchHz = static_cast<float> (crossings) / duration;
        result.outputRms = static_cast<float> (std::sqrt (energy / static_cast<double> (measureEnd - measureStart)));
        if (! analyzedPitches.empty())
        {
            std::sort (analyzedPitches.begin(), analyzedPitches.end());
            result.analyzedPitchHz = analyzedPitches[analyzedPitches.size() / 2];
        }
        return result;
    }

    struct LiveBlockStressResult
    {
        bool prepared = false;
        bool processed = false;
        bool finite = true;
        int successfulBlocks = 0;
    };

    LiveBlockStressResult runLiveBlockStress()
    {
        LiveBlockStressResult result;
        constexpr int hostBlockSize = 2048;
        constexpr int controlHopSize = 480; // 10 ms at 48 kHz, matching the processor.
        constexpr int hostBlocks = 3072;     // More than two minutes of continuous audio.

        SignalsmithHybridVoiceEngine engine;
        result.prepared = engine.prepare (2, sampleRate, hostBlockSize);
        if (! result.prepared || ! engine.isTransformAvailable())
            return result;

        std::array<std::array<float, hostBlockSize>, 2> audio {};
        std::array<const float*, 2> inputPointers { audio[0].data(), audio[1].data() };
        std::array<float*, 2> outputPointers { audio[0].data(), audio[1].data() };
        std::array<float, controlHopSize> pitch {};
        std::array<float, controlHopSize> voiced {};
        std::array<float, controlHopSize> aperiodic {};
        std::int64_t absoluteSample = 0;

        for (int block = 0; block < hostBlocks; ++block)
        {
            if (block > 0 && (block % 1024) == 0)
            {
                // Device reconfiguration in a DAW may prepare the same processor
                // repeatedly without replacing the plug-in instance.
                engine.reset();
                if (! engine.prepare (2, sampleRate, hostBlockSize))
                    return result;
            }

            for (int i = 0; i < hostBlockSize; ++i)
            {
                const double t = static_cast<double> (absoluteSample + i) / sampleRate;
                const float voice = static_cast<float> (0.20 * std::sin (2.0 * pi * 132.0 * t)
                                                      + 0.07 * std::sin (2.0 * pi * 264.0 * t)
                                                      + 0.025 * std::sin (2.0 * pi * 396.0 * t));
                const float breath = ((absoluteSample + i) % static_cast<std::int64_t> (sampleRate * 2.0)
                                      < static_cast<std::int64_t> (sampleRate * 0.12))
                    ? static_cast<float> (0.035 * std::sin (2.0 * pi * 5200.0 * t)) : 0.0f;
                audio[0][static_cast<std::size_t> (i)] = voice + breath;
                audio[1][static_cast<std::size_t> (i)] = voice * 0.91f + breath * 0.83f;
            }

            for (int offset = 0; offset < hostBlockSize; )
            {
                const int count = std::min (controlHopSize, hostBlockSize - offset);
                const int controlRegion = static_cast<int> ((absoluteSample + offset) / controlHopSize) % 8;
                const float pitchShift = (block / 64) % 2 == 0 ? 7.0f : -5.0f;
                const float formantShift = (block / 96) % 2 == 0 ? 0.0f : 2.0f;
                engine.setParameters (pitchShift, formantShift, 140.0f);

                for (int i = 0; i < count; ++i)
                {
                    const auto index = static_cast<std::size_t> (i);
                    // Include aperiodic/unvoiced stretches and short boundary
                    // transitions as seen in speech, alongside stable vowels.
                    const bool unvoiced = controlRegion == 0 || controlRegion == 7;
                    pitch[index] = unvoiced ? 0.0f : 132.0f;
                    voiced[index] = unvoiced ? 0.04f : 0.86f;
                    aperiodic[index] = unvoiced ? 0.88f : 0.12f;
                }

                if (! engine.processBlock (inputPointers.data(), outputPointers.data(), 2, count,
                                           pitch.data(), voiced.data(), aperiodic.data(), 100.0f,
                                           false, 65.0f, true, true))
                    return result;
                ++result.successfulBlocks;
                offset += count;
            }

            for (const auto& channel : audio)
                result.finite &= std::all_of (channel.begin(), channel.end(),
                    [] (float value) { return std::isfinite (value); });
            if (! result.finite)
                return result;
            absoluteSample += hostBlockSize;
        }

        result.processed = true;
        return result;
    }
#endif

int main()
{
    bool ok = true;
    for (int choice = 0; choice < 3; ++choice)
    {
        const double sliderValue = voice_morph::parameter_mapping::choiceIndexToValue (choice, 3);
        ok &= require (voice_morph::parameter_mapping::choiceValueToIndex (sliderValue, 3) == choice,
                       "voice mode slider value round-trips to choice index " + std::to_string (choice));
    }

    ok &= require (std::abs (voice_morph::parameter_mapping::shiftedFrequencyFromSemitones (220.0, 12.0) - 440.0) < 1.0e-9,
                   "pitch contour target doubles after a +12 semitone shift");
    ok &= require (std::abs (voice_morph::parameter_mapping::shiftedFrequencyFromSemitones (220.0, -12.0) - 110.0) < 1.0e-9,
                   "pitch contour target halves after a -12 semitone shift");
    ok &= require (voice_morph::parameter_mapping::shiftedFrequencyFromSemitones (0.0, 12.0) == 0.0,
                   "pitch contour leaves silence unvoiced");
    ok &= require (voice_morph::parameter_mapping::shiftedFrequencyFromSemitones (220.0, 0.0) == 220.0,
                   "neutral pitch setting keeps the target contour aligned with input F0");

    constexpr double rotaryStart = 0.75 * pi;
    constexpr double rotaryEnd = 2.25 * pi;
    ok &= require (std::abs (voice_morph::parameter_mapping::rotaryAngleFromProportion (0.0, rotaryStart, rotaryEnd) - rotaryStart) < 1.0e-9,
                   "rotary marker maps minimum value to its configured start angle");
    ok &= require (std::abs (voice_morph::parameter_mapping::rotaryAngleFromProportion (0.5, rotaryStart, rotaryEnd)
                             - (rotaryStart + rotaryEnd) * 0.5) < 1.0e-9,
                   "rotary marker maps midpoint value to the dial midpoint");
    ok &= require (std::abs (voice_morph::parameter_mapping::rotaryAngleFromProportion (0.5, rotaryStart, rotaryEnd)
                             - 1.5 * pi) < 1.0e-9,
                   "the neutral rotary marker sits at 12 o'clock, matching the dial");
    ok &= require (std::abs (voice_morph::parameter_mapping::rotaryAngleFromProportion (1.0, rotaryStart, rotaryEnd) - rotaryEnd) < 1.0e-9,
                   "rotary marker maps maximum value to its configured end angle");
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

    // At 100% wet, low-confidence input must survive even when Breath Preserve
    // is zero. The analyzer can lag behind a real fricative or quiet onset.
    std::array<float, 1024> syntheticBreath {};
    std::mt19937 residualRandom (54321);
    std::uniform_real_distribution<float> residualNoise (-0.1f, 0.1f);
    double syntheticBreathEnergy = 0.0;
    for (auto& sample : syntheticBreath)
    {
        sample = residualNoise (residualRandom);
        syntheticBreathEnergy += static_cast<double> (sample) * sample;
    }
    const auto processBreathResidual = [&] (float breathPreservePercent)
    {
        HybridVoicePath path;
        if (! path.prepare (1, sampleRate, 128, 0, 0))
            return std::pair<double, double> { -1.0, -1.0 };
        FixedDelayTransform identityTransform (0, 1);
        std::array<float, 128> blockIn {}, blockOut {}, noPitch {}, unvoiced {}, aperiodic {};
        unvoiced.fill (0.0f);
        aperiodic.fill (0.0f);
        std::array<const float*, 1> inPointers { blockIn.data() };
        std::array<float*, 1> outPointers { blockOut.data() };
        double outputEnergy = 0.0;
        for (int offset = 0; offset < 1024; offset += 128)
        {
            for (int i = 0; i < 128; ++i)
                blockIn[static_cast<std::size_t> (i)] = syntheticBreath[static_cast<std::size_t> (offset + i)];
            if (! path.processBlock (inPointers.data(), outPointers.data(), 1, 128,
                                     noPitch.data(), unvoiced.data(), aperiodic.data(), 100.0f,
                                     fixedDelayTransform, &identityTransform,
                                     breathPreservePercent, true, false))
                return std::pair<double, double> { -1.0, -1.0 };
            for (float sample : blockOut)
                outputEnergy += static_cast<double> (sample) * sample;
        }
        return std::pair<double, double> { outputEnergy, syntheticBreathEnergy };
    };
    const auto noBreathResidual = processBreathResidual (0.0f);
    const auto fullBreathResidual = processBreathResidual (100.0f);
    ok &= require (noBreathResidual.first > noBreathResidual.second * 0.99
                   && noBreathResidual.first < noBreathResidual.second * 1.01,
                   "low-confidence unvoiced audio survives at 100% Voice Mix when Breath Preserve is zero");
    ok &= require (fullBreathResidual.first > fullBreathResidual.second * 0.99
                   && fullBreathResidual.first < fullBreathResidual.second * 1.01,
                   "low-confidence fallback keeps full unvoiced energy with Breath Preserve at 100%");

    // When voicing confidence is absent, source voice must be transformed,
    // never sent through as an unpitched residual at 100% Voice Mix.
    HybridVoicePath uncertainVoicePath;
    ok &= require (uncertainVoicePath.prepare (1, sampleRate, 128, 0, 0),
                   "uncertain-voice no-leak path prepares");
    ScaledTransform noLeakTransform { 0.4f };
    std::array<float, 128> uncertainInput {}, uncertainOutput {}, unknownPitch {}, unknownVoicing {}, unknownAperiodic {};
    std::array<const float*, 1> uncertainInPointers { uncertainInput.data() };
    std::array<float*, 1> uncertainOutPointers { uncertainOutput.data() };
    double uncertainInputEnergy = 0.0;
    double uncertainOutputEnergy = 0.0;
    bool uncertainBlocksSucceeded = true;
    for (int offset = 0; offset < 4096; offset += 128)
    {
        for (int i = 0; i < 128; ++i)
        {
            const float sample = 0.2f * static_cast<float> (std::sin (
                2.0 * pi * 132.0 * static_cast<double> (offset + i) / sampleRate));
            uncertainInput[static_cast<std::size_t> (i)] = sample;
            if (offset + i >= 1024)
                uncertainInputEnergy += static_cast<double> (sample) * sample;
        }
        uncertainBlocksSucceeded &= uncertainVoicePath.processBlock (
            uncertainInPointers.data(), uncertainOutPointers.data(), 1, 128,
            unknownPitch.data(), unknownVoicing.data(), unknownAperiodic.data(), 100.0f,
            scaledTransform, &noLeakTransform, 0.0f, true, false);
        if (offset >= 1024)
            for (float sample : uncertainOutput)
                uncertainOutputEnergy += static_cast<double> (sample) * sample;
    }
    ok &= require (uncertainBlocksSucceeded, "uncertain voiced onset is accepted at 100% Voice Mix");
    const double uncertainOutputRatio = uncertainInputEnergy > 0.0
        ? std::sqrt (uncertainOutputEnergy / uncertainInputEnergy) : 0.0;
    ok &= require (uncertainOutputRatio > 0.38 && uncertainOutputRatio < 0.42,
                   "low-confidence voiced audio follows the transform instead of leaking dry at 100% Voice Mix (gain ratio "
                   + std::to_string (uncertainOutputRatio) + ")");

    // Reproduce stale pitch/voicing controls at a vowel-to-frication boundary.
    // Once the local one-period correlation rejects the new noise, the
    // confidence fallback must carry it through even though the analyzer still
    // reports the previous voiced frame.
    HybridVoicePath staleTrackerPath;
    ok &= require (staleTrackerPath.prepare (1, sampleRate, 128, 0, 0),
                   "stale-tracker transition path prepares");
    FixedDelayTransform staleTrackerTransform (0, 1);
    std::array<float, 128> staleInput {}, staleOutput {}, stalePitch {}, staleVoiced {}, staleAperiodic {};
    stalePitch.fill (132.0f);
    staleVoiced.fill (1.0f);
    std::array<const float*, 1> staleInPointers { staleInput.data() };
    std::array<float*, 1> staleOutPointers { staleOutput.data() };
    for (int offset = 0; offset < 4096; offset += 128)
    {
        for (int i = 0; i < 128; ++i)
        {
            const double time = static_cast<double> (offset + i) / sampleRate;
            staleInput[static_cast<std::size_t> (i)] = static_cast<float> (0.15 * std::sin (2.0 * pi * 132.0 * time));
        }
        staleTrackerPath.processBlock (staleInPointers.data(), staleOutPointers.data(), 1, 128,
                                       stalePitch.data(), staleVoiced.data(), staleAperiodic.data(), 100.0f,
                                       fixedDelayTransform, &staleTrackerTransform, 0.0f, true, false);
    }

    double fricationInputEnergy = 0.0;
    double fricationOutputEnergy = 0.0;
    std::mt19937 fricationRandom (8765);
    std::uniform_real_distribution<float> fricationNoise (-0.08f, 0.08f);
    for (int offset = 0; offset < 1024; offset += 128)
    {
        for (int i = 0; i < 128; ++i)
        {
            const float sample = fricationNoise (fricationRandom);
            staleInput[static_cast<std::size_t> (i)] = sample;
            if (offset + i >= 512)
                fricationInputEnergy += static_cast<double> (sample) * sample;
        }
        staleTrackerPath.processBlock (staleInPointers.data(), staleOutPointers.data(), 1, 128,
                                       stalePitch.data(), staleVoiced.data(), staleAperiodic.data(), 100.0f,
                                       fixedDelayTransform, &staleTrackerTransform, 0.0f, true, false);
        if (offset >= 512)
            for (float sample : staleOutput)
                fricationOutputEnergy += static_cast<double> (sample) * sample;
    }
    const double fricationEnergyRatio = fricationInputEnergy > 0.0
        ? fricationOutputEnergy / fricationInputEnergy : 0.0;
    ok &= require (fricationEnergyRatio > 0.70,
                   "stale voiced controls cannot gate away a fricative after local periodicity falls (energy ratio "
                   + std::to_string (fricationEnergyRatio) + ")");

    // Alternating valid and missing F0 frames stress the pitch-confidence gate.
    // With no reliable pitch, the residual path should carry the original input
    // instead of producing periodic-stem dropouts.
    HybridVoicePath unstablePitchPath;
    ok &= require (unstablePitchPath.prepare (1, sampleRate, 128, 0, 0),
                   "unstable-pitch transition path prepares");
    FixedDelayTransform unstablePitchTransform (0, 1);
    std::array<float, 128> unstableInput {}, unstableOutput {}, unstablePitch {}, unstableVoiced {}, unstableAperiodic {};
    std::array<const float*, 1> unstableInPointers { unstableInput.data() };
    std::array<float*, 1> unstableOutPointers { unstableOutput.data() };
    double unstableInputEnergy = 0.0;
    double unstableOutputEnergy = 0.0;
    for (int block = 0; block < 48; ++block)
    {
        const bool pitchAvailable = block % 2 == 0;
        for (int i = 0; i < 128; ++i)
        {
            const int absoluteSample = block * 128 + i;
            const float sample = static_cast<float> (0.14 * std::sin (2.0 * pi * 132.0 * absoluteSample / sampleRate));
            unstableInput[static_cast<std::size_t> (i)] = sample;
            unstablePitch[static_cast<std::size_t> (i)] = pitchAvailable ? 132.0f : 0.0f;
            unstableVoiced[static_cast<std::size_t> (i)] = pitchAvailable ? 1.0f : 0.0f;
            unstableInputEnergy += static_cast<double> (sample) * sample;
        }
        unstablePitchPath.processBlock (unstableInPointers.data(), unstableOutPointers.data(), 1, 128,
                                       unstablePitch.data(), unstableVoiced.data(), unstableAperiodic.data(), 100.0f,
                                       fixedDelayTransform, &unstablePitchTransform, 0.0f, false, false);
        for (float sample : unstableOutput)
            unstableOutputEnergy += static_cast<double> (sample) * sample;
    }
    ok &= require (unstableOutputEnergy > unstableInputEnergy * 0.70,
                   "intermittent F0 loss falls back to audio instead of causing a level collapse");

    // Compare input/output level at quiet, medium, and normal input amplitudes.
    // The quiet case deliberately has no trusted F0; the others exercise a
    // partially trusted and a confident periodic split.
    const std::array<float, 3> levelTestAmplitudes { 0.001f, 0.01f, 0.20f };
    for (const float amplitude : levelTestAmplitudes)
    {
        HybridVoicePath levelPath;
        const bool prepared = levelPath.prepare (1, sampleRate, 128, 0, 0);
        FixedDelayTransform levelTransform (0, 1);
        std::array<float, 128> levelInput {}, levelOutput {}, levelPitch {}, levelVoiced {}, levelAperiodic {};
        const float confidence = amplitude < 0.002f ? 0.0f : (amplitude < 0.05f ? 0.60f : 1.0f);
        levelPitch.fill (confidence > 0.0f ? 132.0f : 0.0f);
        levelVoiced.fill (confidence);
        std::array<const float*, 1> levelInPointers { levelInput.data() };
        std::array<float*, 1> levelOutPointers { levelOutput.data() };
        double levelInputEnergy = 0.0;
        double levelOutputEnergy = 0.0;
        for (int offset = 0; offset < 8192; offset += 128)
        {
            for (int i = 0; i < 128; ++i)
            {
                const int absoluteSample = offset + i;
                const float sample = amplitude * static_cast<float> (
                    std::sin (2.0 * pi * 132.0 * absoluteSample / sampleRate));
                levelInput[static_cast<std::size_t> (i)] = sample;
            }
            levelPath.processBlock (levelInPointers.data(), levelOutPointers.data(), 1, 128,
                                    levelPitch.data(), levelVoiced.data(), levelAperiodic.data(), 100.0f,
                                    fixedDelayTransform, &levelTransform, 0.0f, false, false);
            if (offset >= 4096)
                for (int i = 0; i < 128; ++i)
                {
                    const auto index = static_cast<std::size_t> (i);
                    levelInputEnergy += static_cast<double> (levelInput[index]) * levelInput[index];
                    levelOutputEnergy += static_cast<double> (levelOutput[index]) * levelOutput[index];
                }
        }
        const double levelRatio = levelInputEnergy > 0.0
            ? std::sqrt (levelOutputEnergy / levelInputEnergy) : 0.0;
        ok &= require (prepared && levelRatio > 0.75 && levelRatio < 1.10,
                       "input amplitude " + std::to_string (amplitude)
                       + " retains output level without automatic gain correction (ratio "
                       + std::to_string (levelRatio) + ")");
    }

    // Exercise the complete streaming analyzer -> split -> transform -> mix
    // route on a quiet onset, an unvoiced consonant, a resumed vowel, and a
    // fading tail. The transform is an identity mock so this isolates routing.
    constexpr int preRollSamples = 3840;
    constexpr int onsetVowelSamples = 5760;
    constexpr int fricativeSamples = 1920;
    constexpr int resumedVowelSamples = 4800;
    constexpr int fadingTailSamples = 2880;
    constexpr int trailingSilenceSamples = 1440;
    const int transitionLength = preRollSamples + onsetVowelSamples + fricativeSamples
                               + resumedVowelSamples + fadingTailSamples + trailingSilenceSamples;
    std::vector<float> transitionInput (static_cast<std::size_t> (transitionLength), 0.0f);
    std::vector<float> transitionOutput (static_cast<std::size_t> (transitionLength), 0.0f);
    std::mt19937 transitionNoiseRandom (9123);
    std::uniform_real_distribution<float> transitionNoise (-1.0f, 1.0f);
    const int onsetStart = preRollSamples;
    for (int i = 0; i < onsetVowelSamples; ++i)
    {
        const float fadeIn = std::min (1.0f, static_cast<float> (i + 1) / 240.0f);
        const double time = static_cast<double> (i) / sampleRate;
        transitionInput[static_cast<std::size_t> (onsetStart + i)] = fadeIn * static_cast<float> (
            0.16 * std::sin (2.0 * pi * 132.0 * time) + 0.04 * std::sin (2.0 * pi * 264.0 * time));
    }
    const int fricativeStart = onsetStart + onsetVowelSamples;
    for (int i = 0; i < fricativeSamples; ++i)
    {
        const float fadeIn = std::min (1.0f, static_cast<float> (i + 1) / 240.0f);
        const float fadeOut = std::min (1.0f, static_cast<float> (fricativeSamples - i) / 240.0f);
        const double time = static_cast<double> (i) / sampleRate;
        transitionInput[static_cast<std::size_t> (fricativeStart + i)] = 0.06f * fadeIn * fadeOut
            * (0.75f * transitionNoise (transitionNoiseRandom)
               + 0.25f * static_cast<float> (std::sin (2.0 * pi * 5500.0 * time)));
    }
    const int resumedStart = fricativeStart + fricativeSamples;
    for (int i = 0; i < resumedVowelSamples; ++i)
    {
        const double time = static_cast<double> (i) / sampleRate;
        transitionInput[static_cast<std::size_t> (resumedStart + i)] = static_cast<float> (
            0.14 * std::sin (2.0 * pi * 132.0 * time) + 0.035 * std::sin (2.0 * pi * 264.0 * time));
    }
    const int tailStart = resumedStart + resumedVowelSamples;
    for (int i = 0; i < fadingTailSamples; ++i)
    {
        const float fade = 1.0f - static_cast<float> (i) / static_cast<float> (fadingTailSamples);
        const double time = static_cast<double> (i) / sampleRate;
        transitionInput[static_cast<std::size_t> (tailStart + i)] = fade * static_cast<float> (
            0.14 * std::sin (2.0 * pi * 132.0 * time) + 0.035 * std::sin (2.0 * pi * 264.0 * time));
    }

    SpeechFeatureAnalyzer transitionAnalyzer;
    transitionAnalyzer.prepare (sampleRate);
    HybridVoicePath analyzedTransitionPath;
    ok &= require (analyzedTransitionPath.prepare (1, sampleRate, 128, 0, 0),
                   "analyzed transition path prepares");
    HybridVoicePath analyzedScaledPath;
    ok &= require (analyzedScaledPath.prepare (1, sampleRate, 128, 0, 0),
                   "scaled analyzed transition path prepares");
    FixedDelayTransform analyzedIdentityTransform (0, 1);
    ScaledTransform analyzedPitchTransform { 0.4f };
    std::vector<float> scaledTransitionOutput (static_cast<std::size_t> (transitionLength), 0.0f);
    std::array<float, 128> analyzedInputBlock {}, analyzedOutputBlock {}, analyzedPitchBlock {},
                           analyzedVoicedBlock {}, analyzedAperiodicBlock {};
    std::array<float, 128> scaledAnalyzedOutputBlock {};
    std::array<const float*, 1> analyzedInPointers { analyzedInputBlock.data() };
    std::array<float*, 1> analyzedOutPointers { analyzedOutputBlock.data() };
    std::array<float*, 1> scaledAnalyzedOutPointers { scaledAnalyzedOutputBlock.data() };
    for (int offset = 0; offset < transitionLength; offset += 128)
    {
        const int count = std::min (128, transitionLength - offset);
        for (int i = 0; i < count; ++i)
        {
            const float sample = transitionInput[static_cast<std::size_t> (offset + i)];
            analyzedInputBlock[static_cast<std::size_t> (i)] = sample;
            transitionAnalyzer.processSample (sample);
            const auto& features = transitionAnalyzer.getFeatures();
            analyzedPitchBlock[static_cast<std::size_t> (i)] = features.pitchHz;
            analyzedVoicedBlock[static_cast<std::size_t> (i)] = features.voiced;
            analyzedAperiodicBlock[static_cast<std::size_t> (i)] = features.aperiodic;
        }
        analyzedTransitionPath.processBlock (analyzedInPointers.data(), analyzedOutPointers.data(), 1, count,
                                             analyzedPitchBlock.data(), analyzedVoicedBlock.data(),
                                             analyzedAperiodicBlock.data(), 100.0f,
                                             fixedDelayTransform, &analyzedIdentityTransform,
                                             65.0f, true, true);
        analyzedScaledPath.processBlock (analyzedInPointers.data(), scaledAnalyzedOutPointers.data(), 1, count,
                                         analyzedPitchBlock.data(), analyzedVoicedBlock.data(),
                                         analyzedAperiodicBlock.data(), 100.0f,
                                         scaledTransform, &analyzedPitchTransform,
                                         65.0f, true, true);
        std::copy_n (analyzedOutputBlock.data(), count,
                     transitionOutput.data() + static_cast<std::size_t> (offset));
        std::copy_n (scaledAnalyzedOutputBlock.data(), count,
                     scaledTransitionOutput.data() + static_cast<std::size_t> (offset));
    }
    const auto rangeRms = [] (const std::vector<float>& values, int start, int end)
    {
        double energy = 0.0;
        for (int i = start; i < end; ++i)
            energy += static_cast<double> (values[static_cast<std::size_t> (i)])
                    * values[static_cast<std::size_t> (i)];
        return std::sqrt (energy / static_cast<double> (end - start));
    };
    const double onsetLevelRatio = rangeRms (transitionOutput, onsetStart + 480, onsetStart + 1248)
                                 / rangeRms (transitionInput, onsetStart + 480, onsetStart + 1248);
    const double fricativeLevelRatio = rangeRms (transitionOutput, fricativeStart + 720, fricativeStart + 1680)
                                     / rangeRms (transitionInput, fricativeStart + 720, fricativeStart + 1680);
    const double tailLevelRatio = rangeRms (transitionOutput, tailStart + 1200, tailStart + 2400)
                                / rangeRms (transitionInput, tailStart + 1200, tailStart + 2400);
    const double transformedOnsetRatio = rangeRms (scaledTransitionOutput, onsetStart + 480, onsetStart + 1248)
                                       / rangeRms (transitionInput, onsetStart + 480, onsetStart + 1248);
    const double transformedTailRatio = rangeRms (scaledTransitionOutput, tailStart + 1200, tailStart + 2400)
                                      / rangeRms (transitionInput, tailStart + 1200, tailStart + 2400);
    ok &= require (onsetLevelRatio > 0.90,
                   "quiet vowel onset retains level while the analyzer fills its pitch window (ratio "
                   + std::to_string (onsetLevelRatio) + ")");
    ok &= require (fricativeLevelRatio > 0.60,
                   "unvoiced frication survives the voiced-to-unvoiced boundary (ratio "
                   + std::to_string (fricativeLevelRatio) + ")");
    ok &= require (tailLevelRatio > 0.60,
                   "a fading vowel tail remains audible as pitch confidence releases (ratio "
                   + std::to_string (tailLevelRatio) + ")");
    ok &= require (transformedOnsetRatio > 0.30 && transformedOnsetRatio < 0.80,
                   "analyzed vowel onset is neither dropped nor passed through mostly dry at 100% Voice Mix (ratio "
                   + std::to_string (transformedOnsetRatio) + ")");
    ok &= require (transformedTailRatio > 0.30 && transformedTailRatio < 0.80,
                   "analyzed vowel tail is neither dropped nor passed through mostly dry at 100% Voice Mix (ratio "
                   + std::to_string (transformedTailRatio) + ")");

    // Neutral processing should not create a gain dip at any Voice Mix setting.
    for (int mixPercent = 0; mixPercent <= 100; mixPercent += 20)
    {
        LatencyAlignedRecombiner mixSweep;
        const bool prepared = mixSweep.prepare (0, sampleRate);
        mixSweep.setLatencySamples (0);
        double mixError = 0.0;
        for (int sample = 0; sample < 3000; ++sample)
        {
            const auto mixed = mixSweep.processSample (0.20f, 0.0f, 0.20f,
                                                        static_cast<float> (mixPercent));
            if (sample >= 1000)
                mixError += static_cast<double> (mixed.output - 0.20f) * (mixed.output - 0.20f);
        }
        ok &= require (prepared && mixError < 1.0e-8,
                       "Voice Mix " + std::to_string (mixPercent)
                       + "% keeps neutral input level after smoothing settles");
    }

    // A transient remains controlled independently from the breath path.
    HybridVoicePath transientPath;
    ok &= require (transientPath.prepare (1, sampleRate, 128, 0, 0),
                   "transient residual regression path prepares");
    FixedDelayTransform transientTransform (0, 1);
    std::array<float, 128> transientInput {}, transientOutput {}, transientPitch {}, transientVoice {}, transientAperiodic {};
    transientInput[64] = 0.5f;
    std::array<const float*, 1> transientIn { transientInput.data() };
    std::array<float*, 1> transientOut { transientOutput.data() };
    ok &= require (transientPath.processBlock (transientIn.data(), transientOut.data(), 1, 128,
                    transientPitch.data(), transientVoice.data(), transientAperiodic.data(), 100.0f,
                    fixedDelayTransform, &transientTransform, 0.0f, false, true),
                   "transient residual processes at full wet mix");
    ok &= require (std::abs (transientOutput[64] - 0.5f) < 1.0e-6f,
                   "Transient Preserve retains a detected plosive while breath preservation is zero");

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
    std::vector<float> hybridAperiodic (2048, 1.0f);
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
                    fixedDelayTransform, &mockTransform, 100.0f, true, false),
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

    recombiner.reset();
    recombiner.setLatencySamples (0);
    const auto fullyWet = recombiner.processSample (0.70f, 0.0f, 0.40f, 100.0f);
    ok &= require (std::abs (fullyWet.output - 0.70f) < 1.0e-6f,
                   "100% Voice Mix routes only the transformed stem when no residual is requested");
    const auto residualWet = recombiner.processSample (0.70f, 0.10f, 0.40f, 100.0f);
    ok &= require (std::abs (residualWet.output - 0.80f) < 1.0e-6f,
                   "100% Voice Mix adds only the explicitly supplied residual, never the dry reference");

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
#else
    const auto shiftedA = renderSignalsmithTone();
    const auto shiftedB = renderSignalsmithTone();
    ok &= require (shiftedA.success && shiftedB.success
                   && shiftedA.inputLatency > 0 && shiftedA.outputLatency > 0,
                   "Signalsmith reports its two measured latency components for the live configuration");
    const bool finiteOutput = shiftedA.success
        && std::all_of (shiftedA.output.begin(), shiftedA.output.end(),
                        [] (float sample) { return std::isfinite (sample); });
    ok &= require (finiteOutput, "Signalsmith produces finite output for a deterministic harmonic voice");
    const bool deterministicOutput = shiftedA.success && shiftedB.success
        && shiftedA.output.size() == shiftedB.output.size()
        && std::equal (shiftedA.output.begin(), shiftedA.output.end(), shiftedB.output.begin(),
                       [] (float a, float b) { return std::abs (a - b) < 1.0e-7f; });
    ok &= require (deterministicOutput,
                   "Signalsmith output repeats deterministically for the same synthetic voice and settings");

    const auto liveStress = runLiveBlockStress();
    ok &= require (liveStress.prepared && liveStress.processed && liveStress.finite
                   && liveStress.successfulBlocks >= 15000,
                   "Signalsmith survives stereo 2048-sample Live blocks split into 10 ms hops, parameter changes, speech transitions, and repeated prepare cycles");

    const auto hybridTone = renderHybridVoiceTone (false);
    const float expectedHybridPitch = 132.0f * std::exp2 (7.0f / 12.0f);
    ok &= require (hybridTone.prepared && hybridTone.everyTransformSucceeded,
                   "hybrid voice path reports successful Signalsmith transforms for every 10 ms block");
    ok &= require (hybridTone.outputRms > 0.05f,
                   "hybrid voice path retains audible level at 100% Voice Mix");
    ok &= require (std::abs (hybridTone.measuredPitchHz - expectedHybridPitch) < 8.0f,
                   "100% Voice Mix preserves the +7 semitone shift through splitting and recombination (measured "
                       + std::to_string (hybridTone.measuredPitchHz) + " Hz)");

    const auto analyzedHybridTone = renderHybridVoiceTone (true);
    ok &= require (analyzedHybridTone.prepared && analyzedHybridTone.everyTransformSucceeded
                   && analyzedHybridTone.analyzedPitchHz > 122.0f && analyzedHybridTone.analyzedPitchHz < 142.0f,
                   "live pitch tracker supplies the expected 132 Hz voice to the hybrid transform (median "
                       + std::to_string (analyzedHybridTone.analyzedPitchHz) + " Hz)");
    ok &= require (analyzedHybridTone.outputRms > 0.05f
                   && std::abs (analyzedHybridTone.measuredPitchHz - expectedHybridPitch) < 12.0f,
                   "live analyzer, transform, and 100% Voice Mix produce the requested +7 semitone voice (measured "
                       + std::to_string (analyzedHybridTone.measuredPitchHz) + " Hz)");

    if (finiteOutput)
    {
        const int measureStart = static_cast<int> (sampleRate * 0.25);
        const int measureEnd = static_cast<int> (sampleRate * 0.95);
        int crossings = 0;
        float previous = shiftedA.output[static_cast<std::size_t> (measureStart - 1)];
        for (int n = measureStart; n < measureEnd; ++n)
        {
            const float current = shiftedA.output[static_cast<std::size_t> (n)];
            if (previous <= 0.0f && current > 0.0f)
                ++crossings;
            previous = current;
        }
        const float duration = static_cast<float> (measureEnd - measureStart) / static_cast<float> (sampleRate);
        const float measuredPitch = static_cast<float> (crossings) / duration;
        const float expectedPitch = 132.0f * std::exp2 (7.0f / 12.0f);
        ok &= require (std::abs (measuredPitch - expectedPitch) < 6.0f,
                       "Signalsmith shifts a 132 Hz voiced tone by +7 semitones (measured "
                       + std::to_string (measuredPitch) + " Hz)");
    }
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

    PitchContourMapper dynamicsNeutral;
    PitchContourMapper dynamicsFull;
    dynamicsNeutral.prepare (sampleRate);
    dynamicsFull.prepare (sampleRate);
    dynamicsNeutral.setDynamicsPercent (0.0f);
    dynamicsFull.setDynamicsPercent (100.0f);
    for (int frame = 0; frame < 250; ++frame)
    {
        dynamicsNeutral.processFrame (220.0f, 1.0f);
        dynamicsFull.processFrame (220.0f, 1.0f);
    }
    ok &= require (std::abs (dynamicsNeutral.getExpansionRatio() - 1.0f) < 1.0e-6f
                   && std::abs (dynamicsFull.getExpansionRatio() - 1.25f) < 1.0e-6f,
                   "Pitch Dynamics maps 0% to neutral and 100% to a bounded 1.25x contour expansion");

    PitchContourMapper onsetBaseGuard;
    onsetBaseGuard.prepare (sampleRate);
    onsetBaseGuard.setDynamicsPercent (100.0f);
    for (int frame = 0; frame < 4; ++frame)
        onsetBaseGuard.processFrame (330.0f, 0.35f);
    for (int frame = 0; frame < 6; ++frame)
        onsetBaseGuard.processFrame (165.0f, 0.90f);
    const float offsetAfterStableSeed = onsetBaseGuard.getOffsetSemitones();
    const float stableBaselineHz = onsetBaseGuard.getBaselinePitchHz();
    for (int frame = 0; frame < 20; ++frame)
        onsetBaseGuard.processFrame (196.0f, 0.90f);
    ok &= require (std::abs (offsetAfterStableSeed) < 0.02f
                   && std::abs (stableBaselineHz - 165.0f) < 0.05f
                   && onsetBaseGuard.getOffsetSemitones() > 0.30f,
                   "low-confidence startup pitch is ignored before the stable pitch and formant reference is seeded");

    for (int frame = 0; frame < 30; ++frame)
        dynamicsFull.processFrame (277.18f, 1.0f);
    ok &= require (dynamicsFull.getOffsetSemitones() > 0.50f
                   && dynamicsFull.getOffsetSemitones() < 2.0f,
                   "Pitch Dynamics follows an upward voiced F0 movement with a bounded smooth offset ("
                   + std::to_string (dynamicsFull.getOffsetSemitones()) + " semitones)");

    PitchContourMapper octaveGuard;
    octaveGuard.prepare (sampleRate);
    octaveGuard.setDynamicsPercent (100.0f);
    for (int frame = 0; frame < 250; ++frame)
        octaveGuard.processFrame (220.0f, 1.0f);
    const float offsetBeforeOutlier = octaveGuard.getOffsetSemitones();
    octaveGuard.processFrame (440.0f, 1.0f);
    ok &= require (std::abs (octaveGuard.getOffsetSemitones() - offsetBeforeOutlier) < 0.10f,
                   "Pitch Dynamics rejects a one-frame octave F0 outlier");

    PitchContourMapper contourNeutral;
    PitchContourMapper contourExpanded;
    contourNeutral.prepare (sampleRate);
    contourExpanded.prepare (sampleRate);
    contourNeutral.setDynamicsPercent (0.0f);
    contourExpanded.setDynamicsPercent (100.0f);
    for (int frame = 0; frame < 250; ++frame)
    {
        contourNeutral.processFrame (220.0f, 1.0f);
        contourExpanded.processFrame (220.0f, 1.0f);
    }
    std::vector<float> sourceContour, neutralContour, expandedContour;
    for (int frame = 0; frame < 400; ++frame)
    {
        const float inputSemitones = 1.5f * std::sin (2.0f * static_cast<float> (pi)
                                     * static_cast<float> (frame) / 120.0f);
        const float inputHz = 220.0f * std::exp2 (inputSemitones / 12.0f);
        const float neutralOffset = contourNeutral.processFrame (inputHz, 1.0f);
        const float expandedOffset = contourExpanded.processFrame (inputHz, 1.0f);
        if (frame > 80)
        {
            sourceContour.push_back (inputSemitones);
            neutralContour.push_back (inputSemitones + neutralOffset);
            expandedContour.push_back (inputSemitones + expandedOffset);
        }
    }
    auto percentileSpan = [] (std::vector<float> values)
    {
        std::sort (values.begin(), values.end());
        const auto low = static_cast<std::size_t> (0.05 * static_cast<double> (values.size() - 1));
        const auto high = static_cast<std::size_t> (0.95 * static_cast<double> (values.size() - 1));
        return values[high] - values[low];
    };
    const float sourceSpan = percentileSpan (sourceContour);
    const float neutralSpan = percentileSpan (neutralContour);
    const float expandedSpan = percentileSpan (expandedContour);
    ok &= require (std::abs (neutralSpan - sourceSpan) < 0.15f
                   && expandedSpan > sourceSpan * 1.15f,
                   "Pitch Dynamics preserves neutral contour and expands expressive F0 movement");

    PitchContourMapper unvoicedRelease;
    unvoicedRelease.prepare (sampleRate);
    unvoicedRelease.setDynamicsPercent (100.0f);
    for (int frame = 0; frame < 250; ++frame)
        unvoicedRelease.processFrame (220.0f, 1.0f);
    for (int frame = 0; frame < 30; ++frame)
        unvoicedRelease.processFrame (277.18f, 1.0f);
    const float offsetBeforeUnvoiced = unvoicedRelease.getOffsetSemitones();
    for (int frame = 0; frame < 30; ++frame)
        unvoicedRelease.processFrame (0.0f, 0.0f);
    ok &= require (offsetBeforeUnvoiced > 0.50f
                   && std::abs (unvoicedRelease.getOffsetSemitones()) < 0.20f,
                   "Pitch Dynamics smoothly releases back to neutral in unvoiced gaps");

    return ok ? 0 : 1;
}


