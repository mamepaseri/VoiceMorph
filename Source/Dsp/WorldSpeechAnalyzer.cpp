// SPDX-License-Identifier: AGPL-3.0-only
#include "WorldSpeechAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#if VOICEMORPH_HAS_WORLD
#include <world/cheaptrick.h>
#include <world/d4c.h>
#include <world/dio.h>
#include <world/harvest.h>
#include <world/stonemask.h>
#endif

bool WorldSpeechAnalyzer::isAvailable() const noexcept
{
#if VOICEMORPH_HAS_WORLD
    return true;
#else
    return false;
#endif
}

bool WorldSpeechAnalyzer::analyzeWindow (const float* monoInput,
                                         int numSamples,
                                         int sampleRate,
                                         double minF0Hz,
                                         double maxF0Hz,
                                         double framePeriodMs,
                                         PitchTracker tracker,
                                         WorldAnalysisResult& result) const
{
#if VOICEMORPH_HAS_WORLD
    if (monoInput == nullptr || numSamples < sampleRate / 20
        || sampleRate < 8000 || minF0Hz < 40.0 || maxF0Hz <= minF0Hz
        || framePeriodMs < 2.5 || framePeriodMs > 20.0)
        return false;

    std::vector<double> x (static_cast<size_t> (numSamples));
    for (int i = 0; i < numSamples; ++i)
        x[static_cast<size_t> (i)] = static_cast<double> (monoInput[i]);

    int frames = 0;
    std::vector<double> temporal;
    std::vector<double> f0;
    if (tracker == PitchTracker::Harvest)
    {
        HarvestOption option {};
        InitializeHarvestOption (&option);
        option.f0_floor = minF0Hz;
        option.f0_ceil = maxF0Hz;
        option.frame_period = framePeriodMs;
        frames = GetSamplesForHarvest (sampleRate, numSamples, framePeriodMs);
        if (frames <= 0)
            return false;
        temporal.resize (static_cast<size_t> (frames));
        f0.resize (static_cast<size_t> (frames));
        Harvest (x.data(), numSamples, sampleRate, &option, temporal.data(), f0.data());
    }
    else
    {
        DioOption option {};
        InitializeDioOption (&option);
        option.f0_floor = minF0Hz;
        option.f0_ceil = maxF0Hz;
        option.frame_period = framePeriodMs;
        frames = GetSamplesForDIO (sampleRate, numSamples, framePeriodMs);
        if (frames <= 0)
            return false;
        temporal.resize (static_cast<size_t> (frames));
        f0.resize (static_cast<size_t> (frames));
        Dio (x.data(), numSamples, sampleRate, &option, temporal.data(), f0.data());

        std::vector<double> refined (static_cast<size_t> (frames));
        StoneMask (x.data(), numSamples, sampleRate, temporal.data(), f0.data(), frames, refined.data());
        f0.swap (refined);
    }

    CheapTrickOption cheapOption {};
    InitializeCheapTrickOption (sampleRate, &cheapOption);
    cheapOption.f0_floor = minF0Hz;
    cheapOption.fft_size = GetFFTSizeForCheapTrick (sampleRate, &cheapOption);
    if (cheapOption.fft_size <= 0)
        return false;

    const int bins = cheapOption.fft_size / 2 + 1;
    std::vector<double> spectral (static_cast<size_t> (frames) * static_cast<size_t> (bins));
    std::vector<double> ap (static_cast<size_t> (frames) * static_cast<size_t> (bins));
    std::vector<double*> spectralRows (static_cast<size_t> (frames));
    std::vector<double*> apRows (static_cast<size_t> (frames));
    for (int frame = 0; frame < frames; ++frame)
    {
        spectralRows[static_cast<size_t> (frame)] = spectral.data()
            + static_cast<size_t> (frame) * static_cast<size_t> (bins);
        apRows[static_cast<size_t> (frame)] = ap.data()
            + static_cast<size_t> (frame) * static_cast<size_t> (bins);
    }

    CheapTrick (x.data(), numSamples, sampleRate, temporal.data(), f0.data(), frames,
                &cheapOption, spectralRows.data());

    D4COption d4cOption {};
    InitializeD4COption (&d4cOption);
    D4C (x.data(), numSamples, sampleRate, temporal.data(), f0.data(), frames,
         cheapOption.fft_size, &d4cOption, apRows.data());

    result.sampleRate = sampleRate;
    result.fftSize = cheapOption.fft_size;
    result.frameCount = frames;
    result.spectralBins = bins;
    result.framePeriodMs = framePeriodMs;
    result.effectiveCheapTrickFloorHz = GetF0FloorForCheapTrick (sampleRate, cheapOption.fft_size);
    result.temporalPositionsSeconds = std::move (temporal);
    result.f0Hz = std::move (f0);
    result.spectralEnvelope = std::move (spectral);
    result.aperiodicity = std::move (ap);
    return true;
#else
    (void) monoInput;
    (void) numSamples;
    (void) sampleRate;
    (void) minF0Hz;
    (void) maxF0Hz;
    (void) framePeriodMs;
    (void) tracker;
    (void) result;
    return false;
#endif
}
