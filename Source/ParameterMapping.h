#pragma once

#include <algorithm>
#include <cmath>

namespace voice_morph::parameter_mapping
{
    // AudioParameterChoice values are discrete indices, not normalized 0..1 values.
    inline int choiceValueToIndex (double value, int choiceCount) noexcept
    {
        if (choiceCount <= 0 || ! std::isfinite (value))
            return 0;
        return std::clamp (static_cast<int> (std::lround (value)), 0, choiceCount - 1);
    }

    inline double choiceIndexToValue (int index, int choiceCount) noexcept
    {
        if (choiceCount <= 0)
            return 0.0;
        return static_cast<double> (std::clamp (index, 0, choiceCount - 1));
    }

    inline double rotaryAngleFromProportion (double proportion, double startAngle, double endAngle) noexcept
    {
        if (! std::isfinite (proportion) || ! std::isfinite (startAngle) || ! std::isfinite (endAngle))
            return startAngle;
        return startAngle + std::clamp (proportion, 0.0, 1.0) * (endAngle - startAngle);
    }
    // Pitch targets are defined in semitone space, so the plotted transformed F0
    // follows the same ratio as the pitch-shift parameter.
    inline double shiftedFrequencyFromSemitones (double frequencyHz, double semitones) noexcept
    {
        if (! std::isfinite (frequencyHz) || ! std::isfinite (semitones) || frequencyHz <= 0.0)
            return 0.0;
        const double shifted = frequencyHz * std::exp2 (semitones / 12.0);
        return std::isfinite (shifted) ? shifted : 0.0;
    }
}

