#pragma once

#include <array>
#include <cstddef>

// Optionally expands expressive F0 movement around a slowly-following phrase
// baseline. The initial baseline is seeded only after six consistent,
// high-confidence frames so an ambiguous startup estimate cannot define it.
// Call once per ~10 ms analyzer frame.
class PitchContourMapper final
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setDynamicsPercent (float amountPercent) noexcept;
    float processFrame (float pitchHz, float voicedConfidence) noexcept;
    float getOffsetSemitones() const noexcept { return smoothedOffsetSemitones; }
    float getBaselinePitchHz() const noexcept;
    float getExpansionRatio() const noexcept;

private:
    static constexpr std::size_t baselineSeedFrameCount = 6;
    std::array<float, baselineSeedFrameCount> baselineSeedLog2Hz {};
    std::size_t baselineSeedCount = 0;
    float dynamicsPercent = 0.0f;
    float baselineLog2Hz = 0.0f;
    float lastAcceptedLog2Hz = 0.0f;
    float pendingLog2Hz = 0.0f;
    float smoothedOffsetSemitones = 0.0f;
    int pendingJumpFrames = 0;
    bool baselineReady = false;
    bool previousPitchReady = false;
};
