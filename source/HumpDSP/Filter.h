#pragma once

#include "EnvelopeFollower.h"
#include "MathUtils.h"

#include <algorithm>
#include <cmath>

namespace hump
{

// Resonant 12 dB/oct low-pass (RBJ biquad, direct form I).
class LowpassBiquad
{
public:
    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

    void setCoefficients (double frequency, double q, double sampleRate) noexcept
    {
        const double w = kTwoPi * frequency / sampleRate;
        const double cosW = std::cos (w);
        const double alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        b0 = static_cast<float> ((1.0 - cosW) * 0.5 / a0);
        b1 = static_cast<float> ((1.0 - cosW) / a0);
        b2 = b0;
        a1 = static_cast<float> (-2.0 * cosW / a0);
        a2 = static_cast<float> ((1.0 - alpha) / a0);
    }

    void process (float* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = samples[i];
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            samples[i] = y;
        }

        // Recover from denormals or a runaway state rather than ringing forever.
        if (! isSane (y1) || ! isSane (y2))
            y1 = y2 = 0.0f;
    }

private:
    static constexpr double kTwoPi = 6.283185307179586;
    static constexpr float kStateLimit = 1000.0f;

    static bool isSane (float v) noexcept
    {
        return std::isnormal (v) ? std::abs (v) <= kStateLimit : v == 0.0f;
    }

    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};

// The filter section: stereo low-pass with cutoff glides, followed by a slow
// stereo-linked limiter that tames resonance peaks (the "fat" stage).
class Filter
{
public:
    static constexpr float kMinCutoffHz = 80.0f;
    static constexpr float kMaxCutoffHz = 16000.0f;

    // Maps the 0..1 cutoff control onto 80 Hz..16 kHz with a log-shaped taper.
    static float cutoffToHz (float control) noexcept
    {
        if (control >= 1.0f)
            return kMaxCutoffHz;
        if (control <= 0.0f)
            return kMinCutoffHz;

        const double shaped = -0.25 * std::log10 (1.0 - control);
        if (shaped > 1.0)
            return kMaxCutoffHz;
        if (shaped < kMinShaped)
            return kMinCutoffHz;
        return static_cast<float> (shaped * (kMaxCutoffHz - kMinCutoffHz) + kMinCutoffHz);
    }

    static float resonanceToQ (float control) noexcept { return 1.0f + kResonanceQRange * control; }

    void setSampleRate (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        // Stay a hair (0.01 semitone) under Nyquist.
        nyquistLimitHz = 0.5 * newSampleRate * std::pow (2.0, -kNyquistMarginSemitones / 12.0);
        limiterDetector.setSampleRate (newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        left.reset();
        right.reset();
        limiterDetector.reset();
        limiterGain = 1.0f;
        limiterReleaseStep = 0.0f;
        hasState = false;
    }

    void process (float* l, float* r, int numSamples, float cutoff, float resonance) noexcept
    {
        if (! hasState)
        {
            currentCutoff = cutoff;
            currentResonance = resonance;
            hasState = true;
            updateCoefficients();
        }

        if (! exactlyEqual (resonance, currentResonance))
        {
            currentResonance = resonance;
            updateCoefficients();
        }

        const float glidePerSample = (cutoff - currentCutoff) / static_cast<float> (numSamples);

        for (int start = 0; start < numSamples; start += kGlideChunk)
        {
            const int count = std::min (kGlideChunk, numSamples - start);
            left.process (l + start, count);
            right.process (r + start, count);

            if (! exactlyEqual (glidePerSample, 0.0f))
            {
                currentCutoff += glidePerSample * static_cast<float> (count);
                updateCoefficients();
            }
        }

        if (! exactlyEqual (currentCutoff, cutoff))
        {
            currentCutoff = cutoff;
            updateCoefficients();
        }

        applyLimiter (l, r, numSamples);
    }

private:
    static constexpr double kMinShaped = 1.0e-7;
    static constexpr float kResonanceQRange = 6.0f;
    static constexpr double kNyquistMarginSemitones = 0.01;
    static constexpr int kGlideChunk = 16;
    static constexpr float kLimiterAttackSeconds = 0.0075f;
    static constexpr float kLimiterReleaseSeconds = 0.05f;
    static constexpr float kLimiterRecoverySeconds = 7.5f;

    void updateCoefficients() noexcept
    {
        const double frequency = std::min (static_cast<double> (cutoffToHz (currentCutoff)), nyquistLimitHz);
        const double q = resonanceToQ (currentResonance);
        left.setCoefficients (frequency, q, sampleRate);
        right.setCoefficients (frequency, q, sampleRate);
    }

    void applyLimiter (float* l, float* r, int numSamples) noexcept
    {
        const auto recoverySamples = static_cast<float> (kLimiterRecoverySeconds * sampleRate);

        for (int i = 0; i < numSamples; ++i)
        {
            const float level = limiterDetector.process (std::max (std::abs (l[i]), std::abs (r[i])));
            if (level > 1.0f)
            {
                const float target = 1.0f / level;
                if (target < limiterGain)
                {
                    limiterGain = target;
                    limiterReleaseStep = (1.0f - target) / recoverySamples;
                }
            }

            if (limiterGain < 1.0f)
            {
                l[i] *= limiterGain;
                r[i] *= limiterGain;
                limiterGain += limiterReleaseStep;
            }
        }
    }

    LowpassBiquad left, right;
    EnvelopeFollower limiterDetector { kLimiterAttackSeconds, kLimiterReleaseSeconds };
    double sampleRate = 44100.0;
    double nyquistLimitHz = 22050.0;
    float currentCutoff = 1.0f;
    float currentResonance = 0.0f;
    float limiterGain = 1.0f;
    float limiterReleaseStep = 0.0f;
    bool hasState = false;
};

} // namespace hump
