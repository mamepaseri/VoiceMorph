#pragma once

// Expands expressive F0 movement around a slowly-following phrase baseline.
// This controller produces a smoothed semitone offset; it does not synthesize
// audio or replace the pitch tracker. Call once per ~10 ms analyzer frame.
class PitchContourMapper final
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setDynamicsPercent (float amountPercent) noexcept;
    float processFrame (float pitchHz, float voicedConfidence) noexcept;
    float getOffsetSemitones() const noexcept { return smoothedOffsetSemitones; }
    float getExpansionRatio() const noexcept;

private:
    float dynamicsPercent = 75.0f;
    float baselineLog2Hz = 0.0f;
    float lastAcceptedLog2Hz = 0.0f;
    float pendingLog2Hz = 0.0f;
    float smoothedOffsetSemitones = 0.0f;
    int pendingJumpFrames = 0;
    bool baselineReady = false;
    bool previousPitchReady = false;
};
