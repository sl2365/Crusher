#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Expanded Crusher Fuzz. Neutral controls reproduce the original high-gain,
// asymmetric hard clip, 5200 Hz low-pass, 18 Hz DC blocker and wet DRIVE mix.
// Only the selected model (or the outgoing model during crossfade) is processed.
namespace crusher::fuzz
{
struct Controls
{
    float gate = 0.5f;    // 0..1, centre = original un-gated fuzz
    float bias = 0.5f;    // 0..1, centre = original asymmetric offset
    float clip = 0.5f;    // 0..1, centre = original positive/negative rails
    float body = 0.5f;    // 0..1, centre = unchanged pre-distortion spectrum
    float tone = 0.5f;    // 0..1, centre = original 5200 Hz post-filter
    float outputDb = 0.0f; // -12..12 dB, after wet DRIVE blend
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        constexpr float twoPi = 6.2831853071795864769f;
        dcCoefficient = std::exp(-twoPi * std::clamp(18.0f, 1.0f, fs * 0.45f) / fs);
        bodyCoefficient = std::exp(-twoPi * std::clamp(500.0f, 1.0f, fs * 0.45f) / fs);
        for (size_t i = 0; i < toneTable.size(); ++i)
        {
            const float t = static_cast<float>(i) /
                static_cast<float>(toneTable.size() - 1);
            // 50% is exactly 5200 Hz, matching the existing Fuzz algorithm.
            const float hz = t <= 0.5f ? 800.0f + t * 8800.0f
                : 5200.0f + (t - 0.5f) * 15600.0f;
            toneTable[i] = std::exp(-twoPi * std::clamp(hz, 1.0f, fs * 0.45f) / fs);
        }
        for (size_t i = 0; i < outputTable.size(); ++i)
        {
            const float t = static_cast<float>(i)
                / static_cast<float>(outputTable.size() - 1);
            outputTable[i] = std::pow(10.0f, (-12.0f + 24.0f * t) / 20.0f);
        }
        reset();
        controlsValid = false;
        setControls(controls);
    }

    void reset() noexcept
    {
        bodyState.fill(0.0f);
        toneState.fill(0.0f);
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (controlsValid && next.gate == controls.gate
            && next.bias == controls.bias && next.clip == controls.clip
            && next.body == controls.body && next.tone == controls.tone
            && next.outputDb == controls.outputDb)
            return;
        if (!controlsValid || next.tone != controls.tone)
        {
            const float pos = std::clamp(next.tone, 0.0f, 1.0f)
                * static_cast<float>(toneTable.size() - 1);
            const size_t first = static_cast<size_t>(pos);
            const size_t second = std::min(first + 1, toneTable.size() - 1);
            toneCoefficient = toneTable[first]
                + (toneTable[second] - toneTable[first])
                    * (pos - static_cast<float>(first));
        }
        if (!controlsValid || next.outputDb != controls.outputDb)
        {
            const float pos = (std::clamp(next.outputDb, -12.0f, 12.0f)
                + 12.0f) * static_cast<float>(outputTable.size() - 1) / 24.0f;
            const size_t first = static_cast<size_t>(pos);
            const size_t second = std::min(first + 1, outputTable.size() - 1);
            outputGain = outputTable[first]
                + (outputTable[second] - outputTable[first])
                    * (pos - static_cast<float>(first));
        }

        const float gatePosition = std::clamp(next.gate, 0.0f, 1.0f) - 0.5f;
        gateThreshold = gatePosition < 0.0f ? -gatePosition * 0.16f
                                              : gatePosition * 0.30f;
        gateTransition = gatePosition <= 0.0f ? 0.045f : 0.012f;
        biasOffset = (std::clamp(next.bias, 0.0f, 1.0f) - 0.5f) * 0.30f;
        clipScale = 1.0f + (std::clamp(next.clip, 0.0f, 1.0f) - 0.5f) * 1.30f;
        clipDrive = 1.0f + (std::clamp(next.clip, 0.0f, 1.0f) - 0.5f) * 1.60f;
        bodyScale = (std::clamp(next.body, 0.0f, 1.0f) - 0.5f) * 2.0f;
        controls = next;
        controlsValid = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const size_t index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float a = std::clamp(amount, 0.0f, 1.0f);
        if (a <= 0.0f)
            return sanitise(sample * outputGain);

        bodyState[index] = (1.0f - bodyCoefficient) * sample
                         + bodyCoefficient * bodyState[index];
        float pre = sample;
        if (bodyScale != 0.0f)
            pre += bodyState[index] * bodyScale;
        if (gateThreshold > 0.0f)
        {
            const float level = std::abs(pre);
            const float open = std::clamp(
                (level - gateThreshold) / gateTransition + 0.5f, 0.0f, 1.0f);
            pre *= open * open * (3.0f - 2.0f * open);
        }

        // Keep the original arithmetic/order for the neutral sound.
        const float inputGain = 3.0f + a * 47.0f;
        const float bias = (0.015f + a * 0.12f) + biasOffset;
        const float positiveRail = (0.70f - a * 0.18f) * clipScale;
        const float negativeRail = -(0.50f - a * 0.10f) * clipScale;
        const float driven = pre * (inputGain * clipDrive) + bias;
        const float clipped = std::clamp(driven, negativeRail, positiveRail);
        const float zeroOffset = std::clamp(bias, negativeRail, positiveRail);
        const float asymmetric = (clipped - zeroOffset)
            / std::max(0.001f, std::max(positiveRail, -negativeRail));

        toneState[index] = (1.0f - toneCoefficient) * asymmetric
                         + toneCoefficient * toneState[index];
        const float dcBlocked = toneState[index] - previousInput[index]
                              + dcCoefficient * previousOutput[index];
        previousInput[index] = toneState[index];
        previousOutput[index] = sanitise(dcBlocked, 4.0f);

        const float outputCompensation = 0.78f / (1.0f + a * 0.30f);
        const float shaped = previousOutput[index] * outputCompensation;
        const float result = sample + (shaped - sample) * a;
        return sanitise(result * outputGain);
    }

private:
    static float sanitise(float value, float limit = 8.0f) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, -limit, limit) : 0.0f;
    }
    Controls controls {};
    bool controlsValid = false;
    float dcCoefficient = 0.0f;
    float bodyCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    float gateThreshold = 0.0f;
    float gateTransition = 0.045f;
    float biasOffset = 0.0f;
    float clipScale = 1.0f;
    float clipDrive = 1.0f;
    float bodyScale = 0.0f;
    float outputGain = 1.0f;
    std::array<float, 129> toneTable {};
    std::array<float, 129> outputTable {};
    std::array<float, 2> bodyState {};
    std::array<float, 2> toneState {};
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
};
} // namespace crusher::fuzz
