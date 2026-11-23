#pragma once

#include <algorithm>

namespace hump
{

// Slew-rate limited peak follower. Each sample the tracked value may fall by at
// most `releaseStep` and then rise by at most `attackStep`, which gives a
// linear (not exponential) attack/release ramp of the configured duration.
class EnvelopeFollower
{
public:
    EnvelopeFollower (float attackSecondsIn, float releaseSecondsIn) noexcept
        : attackSeconds (attackSecondsIn), releaseSeconds (releaseSecondsIn)
    {
        setSampleRate (kDefaultSampleRate);
    }

    void setSampleRate (double sampleRate) noexcept
    {
        const auto sr = static_cast<float> (sampleRate);
        attackStep = attackSeconds > 0.0f ? 1.0f / (attackSeconds * sr) : 0.0f;
        releaseStep = releaseSeconds > 0.0f ? 1.0f / (releaseSeconds * sr) : 0.0f;
        value = 0.0f;
    }

    void reset() noexcept { value = 0.0f; }

    float process (float input) noexcept
    {
        const float floor = value - releaseStep;
        float delta = 0.0f;

        if (input > floor)
            delta = input - floor;
        else if (input < -floor)
            delta = -input - floor;

        value = std::max (0.0f, floor + std::min (delta, attackStep));
        return value;
    }

    float getValue() const noexcept { return value; }

private:
    static constexpr double kDefaultSampleRate = 44100.0;

    float attackSeconds;
    float releaseSeconds;
    float attackStep = 0.0f;
    float releaseStep = 0.0f;
    float value = 0.0f;
};

} // namespace hump
