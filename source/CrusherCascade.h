#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Original multi-stage saturation engine. All 16 stages are available but
// only the required stages are evaluated. Stereo states remain independent.
// No noise source, allocation, locking or transcendental function in process().
namespace crusher::cascade
{
struct Controls
{
    float stages = 0.50f;    // continuously interpolate 1..16 stages
    float variation = 0.50f; // soft -> rounded -> sharp transfer families
    float emphasis = 0.50f;  // pre-distortion spectral tilt
    float bias = 0.50f;      // asymmetric transfer curves; centre neutral
    float punch = 0.50f;     // transient-preserving dry path
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float rate = static_cast<float>(std::clamp(sampleRate, 8000.0, 384000.0));
        constexpr float twoPi = 6.283185307179586f;
        preCoefficient = 1.0f - std::exp(-twoPi * std::min(1250.0f, rate * 0.40f) / rate);
        postCoefficient = 1.0f - std::exp(-twoPi * std::min(10000.0f, rate * 0.42f) / rate);
        fastCoefficient = std::exp(-1.0f / (0.0014f * rate));
        slowCoefficient = std::exp(-1.0f / (0.040f * rate));
        dcCoefficient = std::exp(-twoPi * 12.0f / rate);
        setControls(Controls {});
        reset();
    }

    void reset() noexcept
    {
        preLow.fill(0.0f);
        postLow.fill(0.0f);
        fastEnv.fill(0.0f);
        slowEnv.fill(0.0f);
        dcInput.fill(0.0f);
        dcOutput.fill(0.0f);
    }

    void setControls(const Controls& values) noexcept
    {
        // Only scalar control updates on the audio thread. No array rebuilds.
        stagePosition = 1.0f + 15.0f * clamp01(values.stages);
        const float newVariation = clamp01(values.variation);
        const float newBias = 2.0f * clamp01(values.bias) - 1.0f;
        // On a static preset, precompute each stage's curve/offset once.
        // During automation this is refreshed only for changed VARIATION/BIAS.
        if (newVariation != variation || newBias != bias)
        {
            variation = newVariation;
            bias = newBias;
            for (int i = 0; i < 16; ++i)
            {
                const size_t index = static_cast<size_t>(i);
                stageBias[index] = bias * (0.55f + 0.12f * static_cast<float>(i % 3))
                    * ((i % 2) == 0 ? 1.0f : -0.32f);
                stageCurve[index] = std::clamp(variation * 2.0f
                    + (static_cast<float>(i % 5) - 2.0f) * 0.14f, 0.0f, 2.0f);
                biasReference[index] = transfer(stageBias[index], stageCurve[index]);
            }
        }
        emphasis = 2.0f * clamp01(values.emphasis) - 1.0f;
        punch = clamp01(values.punch);
    }

    float process(float input, float drive, int channel) noexcept
    {
        const float wetAmount = clamp01(drive);
        if (wetAmount == 0.0f) return input; // exact zero-DRIVE bypass
        const size_t ch = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float x = std::isfinite(input) ? std::clamp(input, -8.0f, 8.0f) : 0.0f;

        // Bipolar spectral emphasis before the nonlinear chain, centred at a
        // true arithmetic no-op. Positive values emphasise attack/edge.
        const float low = advance(preLow[ch], preCoefficient, x);
        float signal = std::clamp(x + (x - low) * emphasis * 1.35f, -8.0f, 8.0f);

        const int fullStages = std::clamp(static_cast<int>(stagePosition), 1, 16);
        const int finalStage = std::min(fullStages + 1, 16);
        const float fractionalStage = finalStage == fullStages
            ? 0.0f : stagePosition - static_cast<float>(fullStages);
        const float stageGain = 1.0f + 2.15f * wetAmount;
        const float inverseGain = (1.0f + 0.060f * wetAmount) / stageGain;
        float fullOutput = signal;

        // Short, deterministic cascade with three interpolated curve families.
        // Per-stage offsets and curves differ without randomized state.
        for (int i = 0; i < finalStage; ++i)
        {
            const size_t index = static_cast<size_t>(i);
            const float driven = std::clamp(signal * stageGain, -12.0f, 12.0f);
            const float shaped = transfer(driven + stageBias[index], stageCurve[index])
                - biasReference[index];
            signal = std::clamp(shaped * inverseGain, -8.0f, 8.0f);
            if (i + 1 == fullStages) fullOutput = signal;
        }
        signal = fullOutput + (signal - fullOutput) * fractionalStage;

        // Light output smoothing helps manage excessive top-end build-up.
        const float post = advance(postLow[ch], postCoefficient, signal);
        signal = signal * 0.75f + post * 0.25f;

        // Remove DC introduced by asymmetric saturation without creating
        // a signal or random noise when the input is silent.
        const float highPassed = signal - dcInput[ch] + dcCoefficient * dcOutput[ch];
        dcInput[ch] = signal;
        dcOutput[ch] = std::abs(highPassed) < 1.0e-16f ? 0.0f : highPassed;
        signal = dcOutput[ch];

        // Fast/slow envelope difference identifies transients. PUNCH blends
        // original attacks back into the shaped sound, including snare hits.
        const float level = std::abs(x);
        fastEnv[ch] = level + fastCoefficient * (fastEnv[ch] - level);
        slowEnv[ch] = level + slowCoefficient * (slowEnv[ch] - level);
        if (fastEnv[ch] < 1.0e-16f) fastEnv[ch] = 0.0f;
        if (slowEnv[ch] < 1.0e-16f) slowEnv[ch] = 0.0f;
        const float rise = std::max(0.0f, fastEnv[ch] - slowEnv[ch]);
        const float transient = std::clamp(rise / (0.04f + slowEnv[ch]) * 1.35f, 0.0f, 1.0f);
        signal += (x - signal) * (punch * 0.92f * transient);
        const float result = x + (signal - x) * wetAmount;
        return std::isfinite(result) ? std::clamp(result, -16.0f, 16.0f) : 0.0f;
    }

private:
    static float clamp01(float v) noexcept
    {
        return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
    }

    static float advance(float& state, float coefficient, float x) noexcept
    {
        state += coefficient * (x - state);
        if (std::abs(state) < 1.0e-16f) state = 0.0f;
        return state;
    }

    static float transfer(float x, float curve) noexcept
    {
        // Three original stateless curves; each has a unity small-signal
        // slope, so increasing STAGES primarily increases harmonic density.
        const float soft = x / (1.0f + 0.72f * std::abs(x));
        const float clipped = std::clamp(x, -0.85f, 0.85f);
        const float bounded = std::clamp(x, -3.0f, 3.0f);
        const float square = bounded * bounded;
        const float rounded = bounded * (27.0f + square) / (27.0f + 9.0f * square);
        return curve < 1.0f
            ? soft + (rounded - soft) * curve
            : rounded + (clipped - rounded) * (curve - 1.0f);
    }

    std::array<float, 2> preLow {}, postLow {}, fastEnv {}, slowEnv {};
    std::array<float, 2> dcInput {}, dcOutput {};
    std::array<float, 16> stageBias {}, stageCurve {}, biasReference {};
    float preCoefficient = 0.10f;
    float postCoefficient = 0.50f;
    float fastCoefficient = 0.99f;
    float slowCoefficient = 0.999f;
    float dcCoefficient = 0.998f;
    float stagePosition = 8.5f;
    float variation = -1.0f;
    float emphasis = 0.0f;
    float bias = 2.0f;
    float punch = 0.50f;
};
} // namespace crusher::cascade
