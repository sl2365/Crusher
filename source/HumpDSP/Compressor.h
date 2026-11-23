#pragma once

#include "EnvelopeFollower.h"

#include <algorithm>
#include <cmath>

namespace hump
{

// Stereo-linked "crush" compressor. The amount control is really an input
// gain (up to +40 dB) feeding a soft-knee limiter with instant attack and a
// very short release, so it squashes and saturates at the same time.
// In Phat mode the detector reads raw peaks; otherwise a slew-limited
// envelope smooths the detector, which sounds rounder.
class Compressor
{
public:
    static constexpr float kMaxAmount = 0.99f;

    void setSampleRate (double newSampleRate) noexcept
    {
        sampleRate = static_cast<float> (newSampleRate);
        detector.setSampleRate (newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        gain = 1.0f;
        releaseStep = 0.0f;
        detector.reset();
    }

    void setPhatMode (bool shouldBePhat) noexcept { phatMode = shouldBePhat; }

    void process (float* left, float* right, int numSamples, float targetAmount) noexcept
    {
        const float makeup = 1.0f / (1.0f - currentAmount);
        const float kneeRange = makeup - kKnee;
        const float releaseSamples = kReleaseSeconds * sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = std::max (std::abs (left[i]), std::abs (right[i]));
            if (! phatMode)
                peak = detector.process (peak);

            float l = left[i] * makeup;
            float r = right[i] * makeup;
            const float level = peak * makeup;

            if (level > kKnee)
            {
                const float u = (1.0f - (level - kKnee) / kneeRange) * kInvSqrt2;
                const float target = ((0.5f - u * u) * kCurveScale + kKnee) / level;
                if (target < gain)
                {
                    gain = target;
                    releaseStep = (1.0f - gain) / releaseSamples;
                }
            }

            if (gain < 1.0f)
            {
                l *= gain;
                r *= gain;
                gain += releaseStep;
            }

            left[i] = l;
            right[i] = r;
        }

        currentAmount = std::min (targetAmount, kMaxAmount);
    }

private:
    static constexpr float kKnee = 0.31f;
    static constexpr float kCurveScale = 1.38f;
    static constexpr float kInvSqrt2 = 0.707107f;
    static constexpr float kReleaseSeconds = 0.00025f;
    static constexpr float kDetectorAttackSeconds = 0.03f;
    static constexpr float kDetectorReleaseSeconds = 0.08f;

    EnvelopeFollower detector { kDetectorAttackSeconds, kDetectorReleaseSeconds };
    float sampleRate = 44100.0f;
    float currentAmount = 0.0f;
    float gain = 1.0f;
    float releaseStep = 0.0f;
    bool phatMode = false;
};

} // namespace hump
