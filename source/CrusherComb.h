#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Standalone comb distortion DSP. Only the selected Comb model (and a brief
// outgoing type-change crossfade) processes this delay line. Fixed storage,
// no heap allocation or coefficient calculations in the per-sample DSP path.
namespace crusher::comb
{
struct Controls
{
    float time = 0.5f;      // 50% retains the original drive-dependent delay
    float feedback = 0.5f;  // 50% retains the original feedback amount
    float damp = 0.5f;      // 50% retains original 7600 Hz feedback damping
    float spread = 0.5f;    // 50% retains original three-sample stereo offset
    float tone = 0.5f;      // 50% leaves the original output unchanged
    float outputDb = 0.0f;  // -12..+12 dB model output trim
};

class Processor
{
public:
    static constexpr size_t bufferSize = 2048;

    void prepare(double sampleRate) noexcept
    {
        constexpr float twoPi = 6.2831853071795864769f;
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        originalDamping = std::exp(-twoPi * std::min(7600.0f, fs * 0.45f) / fs);
        lowDamping = std::exp(-twoPi * std::min(700.0f, fs * 0.45f) / fs);
        highDamping = std::exp(-twoPi * std::min(16000.0f, fs * 0.45f) / fs);
        toneCoefficient = std::exp(-twoPi * std::min(2900.0f, fs * 0.45f) / fs);
        const auto cachedControls = controls;
        controls = {};
        controls.time = -1.0f; // Force cached coefficients after sample-rate changes.
        setControls(cachedControls);
        reset();
    }

    void reset() noexcept
    {
        for (auto& line : delay) line.fill(0.0f);
        writePositions.fill(0u);
        dampingState.fill(0.0f);
        toneState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (next.time == controls.time && next.feedback == controls.feedback
            && next.damp == controls.damp && next.spread == controls.spread
            && next.tone == controls.tone && next.outputDb == controls.outputDb)
            return;
        if (next.outputDb != controls.outputDb)
            outputGain = std::pow(10.0f,
                std::clamp(next.outputDb, -12.0f, 12.0f) / 20.0f);
        controls = next;
        const float t = std::clamp(next.time, 0.0f, 1.0f);
        if (t <= 0.5f)
            timeFactor = 0.20f + t * 1.60f;
        else
        {
            const float x = (t - 0.5f) * 2.0f;
            timeFactor = 1.0f + 16.0f * x * x;
        }
        feedbackControl = std::clamp(next.feedback, 0.0f, 1.0f);
        spreadSamples = std::clamp(next.spread, 0.0f, 1.0f) * 6.0f;
        const float damp = std::clamp(next.damp, 0.0f, 1.0f);
        dampingCoefficient = damp <= 0.5f
            ? lowDamping + (originalDamping - lowDamping) * (damp * 2.0f)
            : originalDamping + (highDamping - originalDamping) * ((damp - 0.5f) * 2.0f);
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 2.0f;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float a = std::clamp(amount, 0.0f, 1.0f);
        if (a <= 0.0f)
            return sample * outputGain;

        // The legacy Comb curve is deliberately the *default* sound. Its
        // delay used 47 - 36*DRIVE samples plus three for the right channel.
        const float baseLength = static_cast<float>(std::lround(47.0f - a * 36.0f));
        const float addedStereoDelay = index == 0 ? 0.0f : spreadSamples;
        const float delaySamples = std::clamp(
            baseLength * timeFactor + addedStereoDelay,
            2.0f, static_cast<float>(bufferSize - 1));
        const auto whole = static_cast<size_t>(delaySamples);
        const float fraction = delaySamples - static_cast<float>(whole);
        const size_t write = writePositions[index];
        const size_t read = (write + bufferSize - whole) % bufferSize;
        float resonantSample = delay[index][read];
        // Fractional interpolation avoids a step in delay time when TIME or
        // SPREAD is modulated. Neutral settings use an integer delay and
        // therefore exactly reproduce the original comb tap.
        if (fraction > 0.0f && whole < bufferSize - 1)
        {
            const size_t older = (read + bufferSize - 1) % bufferSize;
            resonantSample += fraction * (delay[index][older] - resonantSample);
        }
        dampingState[index] = (1.0f - dampingCoefficient) * resonantSample
                            + dampingCoefficient * dampingState[index];
        const float originalFeedback = 0.15f + a * 0.70f;
        const float feedback = feedbackControl <= 0.5f
            ? originalFeedback * (feedbackControl * 2.0f)
            : originalFeedback + (0.94f - originalFeedback)
                 * ((feedbackControl - 0.5f) * 2.0f);
        delay[index][write] = std::tanh(sample + dampingState[index] * feedback);
        writePositions[index] = (write + 1u) % bufferSize;

        const float driveGain = 1.0f + a * 2.5f;
        const float resonant = sample + dampingState[index]
            * (0.30f + a * 0.60f);
        const float normaliser = std::max(0.001f, std::tanh(driveGain));
        float shaped = std::tanh(resonant * driveGain) / normaliser;
        shaped /= 1.0f + a * 0.38f;
        toneState[index] = (1.0f - toneCoefficient) * shaped
                         + toneCoefficient * toneState[index];
        if (toneAmount < 0.0f)
            shaped += (shaped - toneState[index]) * toneAmount;
        else if (toneAmount > 0.0f)
            shaped += (shaped - toneState[index]) * (0.80f * toneAmount);
        const float result = (sample + (shaped - sample) * a) * outputGain;
        return std::isfinite(result) ? std::clamp(result, -8.0f, 8.0f) : 0.0f;
    }

private:
    Controls controls {};
    float timeFactor = 1.0f;
    float feedbackControl = 0.5f;
    float spreadSamples = 3.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    float originalDamping = 0.0f;
    float lowDamping = 0.0f;
    float highDamping = 0.0f;
    float dampingCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    std::array<std::array<float, bufferSize>, 2> delay {};
    std::array<size_t, 2> writePositions {};
    std::array<float, 2> dampingState {};
    std::array<float, 2> toneState {};
};
}
