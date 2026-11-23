#pragma once

namespace hump
{

// The two waveshapers of the distortion section. Both apply a drive gain and
// then bend everything past a knee so that a full-scale input (|x| == 1) lands
// exactly on full scale again; the curves differ in how they get there.
namespace distortion
{
    // "Mech": hard knee at 0.75, linear (flattened) slope above it.
    constexpr float kMechKnee = 0.75f;
    constexpr float kMechMaxGain = 10.0f;

    // "Tube": knee at 0.6, parabolic shoulder above it. Inputs beyond full
    // scale fold back down, which is part of the character at high drive.
    constexpr float kTubeKnee = 0.6f;
    constexpr float kTubeMaxGain = 4.0f;
    constexpr float kTubeCurveScale = 0.8f;
    constexpr float kInvSqrt2 = 0.707107f;

    struct MechShape
    {
        explicit MechShape (float drive) noexcept
            : gain (1.0f + kMechMaxGain * drive),
              slope ((1.0f - kMechKnee) / (gain - kMechKnee))
        {
        }

        float operator() (float x) const noexcept
        {
            const float y = x * gain;
            if (y > kMechKnee)
                return (y - kMechKnee) * slope + kMechKnee;
            if (y < -kMechKnee)
                return -((-kMechKnee - y) * slope + kMechKnee);
            return y;
        }

        float gain;
        float slope;
    };

    struct TubeShape
    {
        explicit TubeShape (float drive) noexcept
            : gain (1.0f + kTubeMaxGain * drive),
              range (gain - kTubeKnee)
        {
        }

        float operator() (float x) const noexcept
        {
            const float y = x * gain;
            if (y > kTubeKnee)
                return shoulder (y);
            if (y < -kTubeKnee)
                return -shoulder (-y);
            return y;
        }

        float gain;
        float range;

    private:
        float shoulder (float y) const noexcept
        {
            const float u = (1.0f - (y - kTubeKnee) / range) * kInvSqrt2;
            return (0.5f - u * u) * kTubeCurveScale + kTubeKnee;
        }
    };

    template <typename Shape>
    void processStereo (const Shape& shape, float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = shape (left[i]);
            right[i] = shape (right[i]);
        }
    }
} // namespace distortion

} // namespace hump
