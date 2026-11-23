#pragma once

#include "MathUtils.h"

#include <algorithm>

namespace hump
{

// Linear parameter glide: whenever the target changes, ramp from the current
// value to the new target over a fixed time (10 ms by default).
class ParamRamp
{
public:
    explicit ParamRamp (float initialValue = 0.0f, float rampSecondsIn = kDefaultRampSeconds) noexcept
        : rampSeconds (rampSecondsIn), start (initialValue), current (initialValue), target (initialValue)
    {
    }

    void setSampleRate (double sampleRate) noexcept
    {
        rampSamples = std::max (1, static_cast<int> (rampSeconds * static_cast<float> (sampleRate)));
    }

    void setTarget (float newTarget) noexcept
    {
        if (exactlyEqual (newTarget, target))
            return;
        start = current;
        target = newTarget;
        elapsed = 0;
    }

    void snapToTarget() noexcept
    {
        start = current = target;
        elapsed = 0;
    }

    // Moves time forward by numSamples and returns the value reached.
    float advance (int numSamples) noexcept
    {
        if (exactlyEqual (current, target))
            return current;

        elapsed += numSamples;
        const float t = std::min (1.0f, static_cast<float> (elapsed) / static_cast<float> (rampSamples));
        current = start + (target - start) * t;
        if (t >= 1.0f)
            current = target;
        return current;
    }

    float getTarget() const noexcept { return target; }

private:
    static constexpr float kDefaultRampSeconds = 0.01f;

    float rampSeconds;
    int rampSamples = 441;
    int elapsed = 0;
    float start;
    float current;
    float target;
};

} // namespace hump
