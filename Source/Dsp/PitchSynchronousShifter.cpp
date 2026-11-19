#include "PitchSynchronousShifter.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr float minimumConfidenceForPhase = 0.18f;
    constexpr float twoPi = 6.28318530717958647692f;
}

bool PitchSynchronousShifter::prepare (double sampleRate, float minPitchHz,
                                      float maxPitchHz) noexcept
{
    if (! std::isfinite (sampleRate) || sampleRate < 8000.0 || sampleRate > 192000.0
        || ! std::isfinite (minPitchHz) || ! std::isfinite (maxPitchHz)
        || minPitchHz < 40.0f || maxPitchHz <= minPitchHz)
        return false;

    sampleRateHz = sampleRate;
    minimumPitchHz = minPitchHz;
    maximumPitchHz = maxPitchHz;
    maximumPeriodSamples = static_cast<float> (sampleRateHz) / minimumPitchHz;
    lookAheadSamples = static_cast<int> (std::ceil (2.0f * maximumPeriodSamples)) + 4;
    fixedLatencySamples = static_cast<int> (std::ceil (3.0f * maximumPeriodSamples)) + 8;

    // The ring must hold the analysis look-ahead, the complete output grain,
    // and enough margin to prevent a future grain from wrapping over unread data.
    const auto capacity = static_cast<std::size_t> (std::ceil (8.0f * maximumPeriodSamples)) + 64u;
    inputRing.assign (capacity, 0.0f);
    pitchRing.assign (capacity, 0.0f);
    confidenceRing.assign (capacity, 0.0f);
    outputAccumulation.assign (capacity, 0.0f);
    outputWeights.assign (capacity, 0.0f);
    synthesisEvents.assign (capacity, 0u);

    ratioSmoothingCoefficient = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRateHz) * 0.020f));
    pitchSmoothingCoefficient = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRateHz) * 0.020f));

    prepared = true;
    reset();
    return true;
}

void PitchSynchronousShifter::reset() noexcept
{
    std::fill (inputRing.begin(), inputRing.end(), 0.0f);
    std::fill (pitchRing.begin(), pitchRing.end(), 0.0f);
    std::fill (confidenceRing.begin(), confidenceRing.end(), 0.0f);
    std::fill (outputAccumulation.begin(), outputAccumulation.end(), 0.0f);
    std::fill (outputWeights.begin(), outputWeights.end(), 0.0f);
    std::fill (synthesisEvents.begin(), synthesisEvents.end(), 0u);
    pitchMarks.fill (-1);
    markWriteIndex = 0;
    markCount = 0;
    sampleClock = 0;
    smoothedPitchHz = 0.0f;
    smoothedPitchRatio = 1.0f;
    synthesisPhase = 0.0f;
    previousInput = 0.0f;
    hasSynthesisPhase = false;
}

void PitchSynchronousShifter::setPitchShiftSemitones (float semitones) noexcept
{
    if (! std::isfinite (semitones))
        semitones = 0.0f;
    semitones = std::clamp (semitones, -12.0f, 12.0f);
    targetPitchRatio = std::exp2 (semitones / 12.0f);
}

std::size_t PitchSynchronousShifter::ringIndex (std::int64_t absoluteIndex) const noexcept
{
    const auto size = static_cast<std::int64_t> (inputRing.size());
    auto wrapped = absoluteIndex % size;
    if (wrapped < 0)
        wrapped += size;
    return static_cast<std::size_t> (wrapped);
}

void PitchSynchronousShifter::addPitchMark (std::int64_t sampleIndex) noexcept
{
    pitchMarks[markWriteIndex] = sampleIndex;
    markWriteIndex = (markWriteIndex + 1u) % markHistorySize;
    markCount = std::min (markCount + 1u, markHistorySize);
}

bool PitchSynchronousShifter::findNearestPitchMark (std::int64_t targetIndex,
                                                     float maximumDistance,
                                                     std::int64_t& result) const noexcept
{
    float bestDistance = std::numeric_limits<float>::max();
    bool found = false;
    for (std::size_t i = 0; i < markCount; ++i)
    {
        const std::size_t index = (markWriteIndex + markHistorySize - 1u - i) % markHistorySize;
        const auto mark = pitchMarks[index];
        if (mark < 0)
            continue;
        const float distance = static_cast<float> (mark > targetIndex ? mark - targetIndex : targetIndex - mark);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            result = mark;
            found = true;
        }
    }
    return found && bestDistance <= maximumDistance;
}

void PitchSynchronousShifter::scheduleGrain (std::int64_t synthesisIndex,
                                              float grainPitchHz) noexcept
{
    if (! std::isfinite (grainPitchHz) || grainPitchHz < minimumPitchHz || grainPitchHz > maximumPitchHz)
        return;

    const float period = static_cast<float> (sampleRateHz) / grainPitchHz;
    const float maxMarkDistance = period * 0.60f;
    std::int64_t sourceMark = -1;
    if (! findNearestPitchMark (synthesisIndex, maxMarkDistance, sourceMark))
        return;

    const int radius = std::clamp (static_cast<int> (std::lround (period)), 2,
                                   static_cast<int> (std::ceil (maximumPeriodSamples)));
    const std::int64_t sourceStart = sourceMark - radius;
    if (sourceStart < 0)
        return;

    const std::int64_t outputCentre = synthesisIndex + fixedLatencySamples;
    const std::int64_t outputStart = outputCentre - radius;
    // The fixed look-ahead must guarantee that the first grain sample is not
    // scheduled in the past. If it is not, reject it rather than make a click.
    if (outputStart < sampleClock)
        return;

    for (int offset = -radius; offset <= radius; ++offset)
    {
        const float phase = static_cast<float> (offset) / static_cast<float> (radius);
        const float window = 0.5f + 0.5f * std::cos (phase * 3.14159265358979323846f);
        if (window <= 0.0f)
            continue;

        const auto sourceIndex = ringIndex (sourceMark + offset);
        const auto outputIndex = ringIndex (outputCentre + offset);
        outputAccumulation[outputIndex] += inputRing[sourceIndex] * window;
        outputWeights[outputIndex] += window;
    }
}

float PitchSynchronousShifter::processSample (float periodicInput, float pitchHz,
                                              float voicedConfidence) noexcept
{
    if (! prepared || inputRing.empty())
        return 0.0f;

    const float input = std::isfinite (periodicInput) ? periodicInput : 0.0f;
    const float confidence = std::clamp (std::isfinite (voicedConfidence) ? voicedConfidence : 0.0f,
                                         0.0f, 1.0f);
    const auto currentIndex = ringIndex (sampleClock);
    inputRing[currentIndex] = input;
    pitchRing[currentIndex] = std::isfinite (pitchHz) ? pitchHz : 0.0f;
    confidenceRing[currentIndex] = confidence;

    const bool validPitch = std::isfinite (pitchHz) && pitchHz >= minimumPitchHz && pitchHz <= maximumPitchHz;
    if (validPitch && confidence > minimumConfidenceForPhase)
        smoothedPitchHz += pitchSmoothingCoefficient * (pitchHz - smoothedPitchHz);
    else if (confidence <= minimumConfidenceForPhase)
        smoothedPitchHz *= 0.90f;

    smoothedPitchRatio += ratioSmoothingCoefficient * (targetPitchRatio - smoothedPitchRatio);

    const bool positiveCrossing = previousInput <= 0.0f && input > 0.0f;
    previousInput = input;
    if (positiveCrossing && confidence > minimumConfidenceForPhase)
        addPitchMark (sampleClock);

    if (validPitch && confidence > minimumConfidenceForPhase && smoothedPitchHz >= minimumPitchHz)
    {
        if (! hasSynthesisPhase)
        {
            if (positiveCrossing)
            {
                synthesisEvents[currentIndex] = 1u;
                synthesisPhase = 0.0f;
                hasSynthesisPhase = true;
            }
        }
        else
        {
            const float increment = smoothedPitchHz * smoothedPitchRatio / static_cast<float> (sampleRateHz);
            synthesisPhase += increment;
            if (synthesisPhase >= 1.0f)
            {
                synthesisPhase -= std::floor (synthesisPhase);
                synthesisEvents[currentIndex] = 1u;
            }
        }
    }
    else
    {
        synthesisPhase = 0.0f;
        hasSynthesisPhase = false;
    }

    const std::int64_t synthesisIndex = sampleClock - lookAheadSamples;
    if (synthesisIndex >= 0)
    {
        const auto eventIndex = ringIndex (synthesisIndex);
        if (synthesisEvents[eventIndex] != 0u)
        {
            const float eventPitch = pitchRing[eventIndex];
            const float eventConfidence = confidenceRing[eventIndex];
            if (eventConfidence > minimumConfidenceForPhase)
                scheduleGrain (synthesisIndex, eventPitch);
            synthesisEvents[eventIndex] = 0u;
        }
    }

    const float accumulatedWeight = outputWeights[currentIndex];
    const float output = accumulatedWeight > 1.0e-5f
        ? outputAccumulation[currentIndex] / accumulatedWeight : 0.0f;
    outputAccumulation[currentIndex] = 0.0f;
    outputWeights[currentIndex] = 0.0f;

    ++sampleClock;
    return std::isfinite (output) ? output : 0.0f;
}
