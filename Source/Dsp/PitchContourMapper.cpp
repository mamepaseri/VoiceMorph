#include "PitchContourMapper.h"

#include <algorithm>
#include <cmath>

void PitchContourMapper::prepare (double sampleRate) noexcept
{
    // The analyzer publishes one frame every 10 ms, regardless of host rate.
    (void) std::clamp (std::isfinite (sampleRate) ? sampleRate : 48000.0, 8000.0, 192000.0);
    reset();
}

void PitchContourMapper::reset() noexcept
{
    baselineLog2Hz = 0.0f;
    lastAcceptedLog2Hz = 0.0f;
    pendingLog2Hz = 0.0f;
    smoothedOffsetSemitones = 0.0f;
    pendingJumpFrames = 0;
    baselineSeedLog2Hz.fill (0.0f);
    baselineSeedCount = 0;
    baselineReady = false;
    previousPitchReady = false;
}

void PitchContourMapper::setDynamicsPercent (float amountPercent) noexcept
{
    dynamicsPercent = std::clamp (std::isfinite (amountPercent) ? amountPercent : 0.0f,
                                  0.0f, 100.0f);
}

float PitchContourMapper::getExpansionRatio() const noexcept
{
    // 0% leaves the contour unchanged; 100% expands phrase-relative movement
    // by at most 1.25x to keep the optional effect subtle.
    return 1.0f + 0.25f * dynamicsPercent * 0.01f;
}

float PitchContourMapper::getBaselinePitchHz() const noexcept
{
    return baselineReady ? std::exp2 (baselineLog2Hz) : 0.0f;
}

float PitchContourMapper::processFrame (float pitchHz, float voicedConfidence) noexcept
{
    constexpr float frameSeconds = 0.010f;
    const float confidence = std::clamp (std::isfinite (voicedConfidence) ? voicedConfidence : 0.0f,
                                         0.0f, 1.0f);
    const bool valid = std::isfinite (pitchHz) && pitchHz >= 65.0f && pitchHz <= 450.0f
                    && confidence >= 0.25f;
    float targetOffset = 0.0f;
    bool rejectOutlier = false;

    if (! baselineReady)
    {
        // Do not let a low-confidence or isolated startup estimate establish
        // the phrase reference. Require six consecutive, confident F0 frames
        // whose adjacent values stay within about 2.4 semitones.
        constexpr float seedConfidence = 0.60f;
        constexpr float maximumSeedStepLog2 = 0.20f;
        if (valid && confidence >= seedConfidence)
        {
            const float measuredLog2 = std::log2 (pitchHz);
            if (baselineSeedCount > 0
                && std::abs (measuredLog2 - baselineSeedLog2Hz[baselineSeedCount - 1])
                       > maximumSeedStepLog2)
                baselineSeedCount = 0;

            baselineSeedLog2Hz[baselineSeedCount++] = measuredLog2;
            if (baselineSeedCount == baselineSeedFrameCount)
            {
                auto sortedSeed = baselineSeedLog2Hz;
                std::sort (sortedSeed.begin(), sortedSeed.end());
                baselineLog2Hz = 0.5f * (sortedSeed[2] + sortedSeed[3]);
                lastAcceptedLog2Hz = baselineLog2Hz;
                baselineReady = true;
                previousPitchReady = true;
                baselineSeedCount = 0;
            }
        }
        else
            baselineSeedCount = 0;
    }
    else if (valid)
    {
        const float measuredLog2 = std::log2 (pitchHz);
        // Isolated jumps larger than about 6.6 semitones are held for one
        // frame; a consistent second estimate confirms a true pitch move.
        if (previousPitchReady && std::abs (measuredLog2 - lastAcceptedLog2Hz) > 0.55f)
        {
            if (pendingJumpFrames == 0 || std::abs (measuredLog2 - pendingLog2Hz) > 0.12f)
            {
                pendingLog2Hz = measuredLog2;
                pendingJumpFrames = 1;
                rejectOutlier = true;
            }
            else
            {
                ++pendingJumpFrames;
                if (pendingJumpFrames < 2)
                    rejectOutlier = true;
                else
                {
                    pendingJumpFrames = 0;
                    lastAcceptedLog2Hz = measuredLog2;
                }
            }
        }
        else
        {
            pendingJumpFrames = 0;
            lastAcceptedLog2Hz = measuredLog2;
        }

        if (! rejectOutlier)
        {
            // Follow the phrase's long-term register slowly; short rises,
            // falls and vibrato remain visible as local deviations.
            constexpr float baselineSeconds = 1.20f;
            const float baselineAlpha = 1.0f - std::exp (-frameSeconds / baselineSeconds);
            baselineLog2Hz += (measuredLog2 - baselineLog2Hz) * baselineAlpha;
        }

        if (rejectOutlier)
        {
            targetOffset = smoothedOffsetSemitones;
        }
        else
        {
            float deviationSemitones = 12.0f * (measuredLog2 - baselineLog2Hz);
            const float magnitude = std::abs (deviationSemitones);
            if (magnitude < 0.08f)
                deviationSemitones = 0.0f;
            else
                deviationSemitones = std::copysign (magnitude - 0.08f, deviationSemitones);

            const float expansion = getExpansionRatio() - 1.0f;
            const float confidenceWeight = std::clamp ((confidence - 0.25f) / 0.55f, 0.0f, 1.0f);
            targetOffset = std::clamp (deviationSemitones * expansion * confidenceWeight, -4.0f, 4.0f);
        }
    }

    // A 45 ms attack follows expressive movement; the 75 ms release smooths
    // transitions back toward neutral / unvoiced material.
    const float timeConstant = std::abs (targetOffset) > std::abs (smoothedOffsetSemitones)
        ? 0.045f : 0.075f;
    const float coefficient = 1.0f - std::exp (-frameSeconds / timeConstant);
    smoothedOffsetSemitones += (targetOffset - smoothedOffsetSemitones) * coefficient;
    if (! valid && std::abs (smoothedOffsetSemitones) < 0.015f)
        smoothedOffsetSemitones = 0.0f;
    return smoothedOffsetSemitones;
}
