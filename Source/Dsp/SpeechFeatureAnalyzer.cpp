#include "SpeechFeatureAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float minimumRms = 0.00020f; // approximately -74 dBFS
    constexpr float minimumPitchHz = 65.0f;
    constexpr float maximumPitchHz = 450.0f;

    float clamp01 (float value) noexcept
    {
        return std::clamp (value, 0.0f, 1.0f);
    }
}

void SpeechFeatureAnalyzer::prepare (double sampleRate) noexcept
{
    sampleRateHz = std::clamp (sampleRate, 8000.0, 192000.0);
    // Keep the pitch-analysis rate close to 12 kHz across common host rates.
    // A fixed /4 decimator would make the analysis window too short at 96/192 kHz.
    decimationFactor = std::max (1, static_cast<int> (std::lround (sampleRateHz / 12000.0)));
    decimatedRateHz = sampleRateHz / static_cast<double> (decimationFactor);
    hopSamples = std::max (1, static_cast<int> (std::lround (sampleRateHz * 0.010)));
    dcCoefficient = static_cast<float> (std::exp (-2.0 * 3.14159265358979323846 * 35.0 / sampleRateHz));
    lowpassCoefficient = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979323846 * 2600.0 / sampleRateHz));
    reset();
}

void SpeechFeatureAnalyzer::setTracking (float amountPercent) noexcept
{
    trackingAmount = std::clamp (std::isfinite (amountPercent) ? amountPercent * 0.01f : 0.70f,
                                 0.0f, 1.0f);
}

void SpeechFeatureAnalyzer::reset() noexcept
{
    ring.fill (0.0f);
    writeIndex = 0;
    samplesInRing = 0;
    decimationCount = 0;
    decimationSum = 0.0f;
    samplesSinceAnalysis = 0;
    dcPreviousInput = 0.0f;
    dcPreviousOutput = 0.0f;
    lowpassState = 0.0f;
    previousDc = 0.0f;
    frameEnergy = 0.0f;
    frameHighEnergy = 0.0f;
    framePeakDelta = 0.0f;
    smoothedVoiced = 0.0f;
    smoothedAperiodic = 0.0f;
    smoothedPitchHz = 0.0f;
    features = {};
}

void SpeechFeatureAnalyzer::pushDecimatedSample (float sample) noexcept
{
    ring[writeIndex] = sample;
    writeIndex = (writeIndex + 1) % ringSize;
    samplesInRing = std::min (samplesInRing + 1, ringSize);
}

bool SpeechFeatureAnalyzer::processSample (float monoSample) noexcept
{
    const float input = std::isfinite (monoSample) ? monoSample : 0.0f;
    const float dcBlocked = input - dcPreviousInput + dcCoefficient * dcPreviousOutput;
    dcPreviousInput = input;
    dcPreviousOutput = dcBlocked;

    lowpassState += lowpassCoefficient * (dcBlocked - lowpassState);
    const float high = dcBlocked - lowpassState;
    const float delta = std::abs (dcBlocked - previousDc);
    previousDc = dcBlocked;

    frameEnergy += dcBlocked * dcBlocked;
    frameHighEnergy += high * high;
    framePeakDelta = std::max (framePeakDelta, delta);

    decimationSum += dcBlocked;
    if (++decimationCount >= decimationFactor)
    {
        pushDecimatedSample (decimationSum / static_cast<float> (decimationFactor));
        decimationCount = 0;
        decimationSum = 0.0f;
    }

    if (++samplesSinceAnalysis < hopSamples)
        return false;

    analyseFrame();
    samplesSinceAnalysis = 0;
    frameEnergy = 0.0f;
    frameHighEnergy = 0.0f;
    framePeakDelta = 0.0f;
    return true;
}

void SpeechFeatureAnalyzer::analyseFrame() noexcept
{
    const float rms = std::sqrt (frameEnergy / static_cast<float> (std::max (1, hopSamples)));
    const float highRatio = frameEnergy > 1.0e-12f
        ? clamp01 (frameHighEnergy / frameEnergy) : 0.0f;
    const float transient = clamp01 (framePeakDelta / (rms * 5.0f + 1.0e-6f));

    float mean = 0.0f;
    if (samplesInRing >= analysisSize)
    {
        const std::size_t start = (writeIndex + ringSize - analysisSize) % ringSize;
        for (std::size_t i = 0; i < analysisSize; ++i)
            mean += ring[(start + i) % ringSize];
        mean /= static_cast<float> (analysisSize);
    }

    float windowEnergy = 0.0f;
    for (std::size_t i = 0; i < analysisSize; ++i)
    {
        const std::size_t index = (writeIndex + ringSize - analysisSize + i) % ringSize;
        const float value = ring[index] - mean;
        windowEnergy += value * value;
    }
    const float windowRms = std::sqrt (windowEnergy / static_cast<float> (analysisSize));

    float bestCorrelation = 0.0f;
    float bestLag = 0.0f;
    if (samplesInRing >= analysisSize && windowRms > minimumRms)
    {
        const int minLag = std::max (2, static_cast<int> (std::floor (decimatedRateHz / maximumPitchHz)));
        const int maxLag = std::min (static_cast<int> (analysisSize / 2),
                                     static_cast<int> (std::ceil (decimatedRateHz / minimumPitchHz)));
        std::array<float, analysisSize / 2 + 1> correlations {};
        std::array<float, analysisSize / 2 + 1> differences {};
        std::array<float, analysisSize / 2 + 1> cmndf {};
        const int maxStoredLag = std::min (maxLag, static_cast<int> (correlations.size()) - 1);

        // YIN's cumulative-mean-normalised difference avoids choosing a doubled
        // period simply because a periodic waveform also correlates at 2T, 3T...
        // A parallel normalised correlation value is retained for confidence.
        for (int lag = 1; lag <= maxStoredLag; ++lag)
        {
            double cross = 0.0;
            double energyA = 0.0;
            double energyB = 0.0;
            const std::size_t count = analysisSize - static_cast<std::size_t> (lag);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::size_t ia = (writeIndex + ringSize - analysisSize + i) % ringSize;
                const std::size_t ib = (ia + static_cast<std::size_t> (lag)) % ringSize;
                const double a = static_cast<double> (ring[ia] - mean);
                const double b = static_cast<double> (ring[ib] - mean);
                const double difference = a - b;
                cross += a * b;
                energyA += a * a;
                energyB += b * b;
                differences[static_cast<std::size_t> (lag)] += static_cast<float> (difference * difference);
            }
            const double denominator = std::sqrt (energyA * energyB) + 1.0e-18;
            correlations[static_cast<std::size_t> (lag)] = static_cast<float> (cross / denominator);
        }

        double cumulativeDifference = 0.0;
        for (int lag = 1; lag <= maxStoredLag; ++lag)
        {
            const float difference = differences[static_cast<std::size_t> (lag)];
            cumulativeDifference += difference;
            if (lag >= minLag && cumulativeDifference > 1.0e-18)
                cmndf[static_cast<std::size_t> (lag)] = static_cast<float> (difference * static_cast<double> (lag) / cumulativeDifference);
        }

        int selectedLag = 0;
        constexpr float yinThreshold = 0.18f;
        for (int lag = minLag; lag <= maxStoredLag; ++lag)
        {
            if (cmndf[static_cast<std::size_t> (lag)] < yinThreshold)
            {
                selectedLag = lag;
                while (selectedLag < maxStoredLag
                       && cmndf[static_cast<std::size_t> (selectedLag + 1)]
                              < cmndf[static_cast<std::size_t> (selectedLag)])
                    ++selectedLag;
                break;
            }
        }

        if (selectedLag == 0)
        {
            float lowestDifference = 1.0f;
            for (int lag = minLag; lag <= maxStoredLag; ++lag)
            {
                if (cmndf[static_cast<std::size_t> (lag)] < lowestDifference)
                {
                    lowestDifference = cmndf[static_cast<std::size_t> (lag)];
                    selectedLag = lag;
                }
            }
        }

        if (selectedLag > 0)
        {
            bestLag = static_cast<float> (selectedLag);
            const float left = cmndf[static_cast<std::size_t> (std::max (minLag, selectedLag - 1))];
            const float centre = cmndf[static_cast<std::size_t> (selectedLag)];
            const float right = cmndf[static_cast<std::size_t> (std::min (maxStoredLag, selectedLag + 1))];
            const float denominator = left - 2.0f * centre + right;
            if (std::abs (denominator) > 1.0e-5f)
                bestLag += std::clamp (0.5f * (left - right) / denominator, -0.5f, 0.5f);
            bestCorrelation = correlations[static_cast<std::size_t> (selectedLag)];
        }
    }

    const float levelConfidence = clamp01 ((20.0f * std::log10 (windowRms + 1.0e-9f) + 72.0f) / 26.0f);
    const float periodicityConfidence = clamp01 ((bestCorrelation - 0.28f) / 0.57f);
    const float targetVoiced = periodicityConfidence * levelConfidence;
    const float targetAperiodic = clamp01 (highRatio * (1.0f - 0.55f * bestCorrelation) * 1.65f) * levelConfidence;

    // Short attack / moderate release prevents frame-to-frame chattering while
    // preserving the edges of consonants. These are analysis values only for now.
    // Tracking controls how quickly the detector follows pitch/voicing changes.
    // Low values favour stability; high values follow phrase transitions faster.
    const float voicedAttack = 0.18f + 0.42f * trackingAmount;
    const float voicedRelease = 0.08f + 0.18f * trackingAmount;
    const float aperiodicAttack = 0.24f + 0.38f * trackingAmount;
    const float aperiodicRelease = 0.10f + 0.20f * trackingAmount;
    const float voicedCoeff = targetVoiced > smoothedVoiced ? voicedAttack : voicedRelease;
    const float aperiodicCoeff = targetAperiodic > smoothedAperiodic ? aperiodicAttack : aperiodicRelease;
    smoothedVoiced += (targetVoiced - smoothedVoiced) * voicedCoeff;
    smoothedAperiodic += (targetAperiodic - smoothedAperiodic) * aperiodicCoeff;

    const float candidatePitch = bestLag > 0.0f
        ? static_cast<float> (decimatedRateHz) / bestLag : 0.0f;
    const bool pitchIsValid = bestCorrelation >= 0.35f && candidatePitch >= minimumPitchHz
                           && candidatePitch <= maximumPitchHz && levelConfidence > 0.15f;
    if (pitchIsValid)
        smoothedPitchHz = smoothedPitchHz > 0.0f
            ? smoothedPitchHz + (candidatePitch - smoothedPitchHz) * (0.12f + 0.43f * trackingAmount)
            : candidatePitch;
    else if (smoothedVoiced < 0.10f)
        smoothedPitchHz *= 0.72f;

    // Never publish a stale sub-range F0 during silence or aperiodic sounds.
    // The previous implementation exponentially decayed toward zero but kept
    // exposing tiny positive values (e.g. 1–2 Hz), which could be mistaken for
    // a real tracking result by downstream controls and diagnostics.
    if (smoothedVoiced < 0.08f || smoothedPitchHz < minimumPitchHz)
        smoothedPitchHz = 0.0f;

    features.rms = rms;
    features.pitchHz = smoothedPitchHz;
    features.periodicity = clamp01 (bestCorrelation);
    features.voiced = smoothedVoiced;
    features.aperiodic = smoothedAperiodic;
    features.highBandRatio = highRatio;
    features.transient = transient;
}
