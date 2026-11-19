#pragma once

#include <cstddef>
#include <vector>

#ifndef VOICEMORPH_HAS_WORLD
#define VOICEMORPH_HAS_WORLD 0
#endif

struct WorldAnalysisResult
{
    int sampleRate = 0;
    int fftSize = 0;
    int frameCount = 0;
    int spectralBins = 0;
    double framePeriodMs = 5.0;
    double effectiveCheapTrickFloorHz = 0.0;
    std::vector<double> temporalPositionsSeconds;
    std::vector<double> f0Hz;
    // Row-major [frameCount][spectralBins]. Values are WORLD's linear spectral
    // envelope and aperiodicity estimates, respectively.
    std::vector<double> spectralEnvelope;
    std::vector<double> aperiodicity;
};

// Windowed analysis adapter for the WORLD vocoder. This is deliberately a
// worker-thread/offline utility, NOT an audio-callback function: WORLD's DIO /
// CheapTrick / D4C calls analyse an entire supplied window and allocate scratch
// data here. A later real-time pipeline can publish the result to the audio
// thread via a lock-free snapshot/ring buffer.
class WorldSpeechAnalyzer final
{
public:
    enum class PitchTracker { Dio, Harvest };

    bool isAvailable() const noexcept;

    bool analyzeWindow (const float* monoInput,
                        int numSamples,
                        int sampleRate,
                        double minF0Hz,
                        double maxF0Hz,
                        double framePeriodMs,
                        PitchTracker tracker,
                        WorldAnalysisResult& result) const;
};
