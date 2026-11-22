// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cstddef>
#include <vector>

struct RecombinedSample
{
    float hybrid = 0.0f;
    float output = 0.0f;
};

// Delays the preserved residual and the dry reference by the transform engine's
// measured latency, then combines the already-delayed transformed voiced stem.
// The transformed voiced sample must correspond to the same input-time sample
// as the delayed residual returned here. This class does not add latency to the
// transformed stem and does not guess the latency of any third-party engine.
//
// All memory is allocated in prepare(); processSample() is allocation-free.
class LatencyAlignedRecombiner final
{
public:
    bool prepare (int maximumLatencySamples, double sampleRate) noexcept;
    void reset() noexcept;
    void setLatencySamples (int latencySamples) noexcept;

    RecombinedSample processSample (float transformedVoiced,
                                    float preservedResidual,
                                    float originalInput,
                                    float voiceMixPercent,
                                    bool forceDryFallback = false) noexcept;

    int getLatencySamples() const noexcept { return latencySamples; }

private:
    float readDelayed (const std::vector<float>& buffer) const noexcept;

    std::vector<float> residualDelay;
    std::vector<float> dryDelay;
    std::size_t writeIndex = 0;
    std::size_t samplesWritten = 0;
    int latencySamples = 0;
    float mixSmoothingCoefficient = 0.0f;
    float smoothedVoiceMix = 1.0f;
    bool prepared = false;
};
