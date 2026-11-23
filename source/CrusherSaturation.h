#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Saturation expands the original open arctangent shaper without changing
// its arithmetic when the new controls are at neutral defaults.
namespace crusher::saturation
{
struct Controls
{
    float curve = 0.5f;     // soft atan to denser tanh shaping
    float bias = 0.5f;      // asymmetric offset into waveshaper
    float warm = 0.5f;      // harmonic density via input gain
    float body = 0.5f;      // pre-saturation low-frequency emphasis
    float tone = 0.5f;      // post-saturation tilt
    float outputDb = 0.0f;  // -12 to +12 dB
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        const auto coefficient = [fs](float hz) noexcept
        {
            constexpr float tp = 6.2831853071795864769f;
            return std::exp(-tp * std::min(hz, fs * 0.45f) / fs);
        };
        bodyCoefficient = coefficient(310.0f);
        toneCoefficient = coefficient(3400.0f);
        for (size_t i = 0; i < outputGainTable.size(); ++i)
        {
            const float dB = -12.0f + 24.0f * static_cast<float>(i)
                / static_cast<float>(outputGainTable.size() - 1);
            outputGainTable[i] = std::pow(10.0f, dB * 0.05f);
        }
        reset();
        controlsValid = false;
        setControls(controls);
    }

    void reset() noexcept
    {
        bodyState.fill(0.0f);
        toneState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (controlsValid && next.curve == controls.curve
            && next.bias == controls.bias && next.warm == controls.warm
            && next.body == controls.body && next.tone == controls.tone
            && next.outputDb == controls.outputDb)
            return;

        curveOffset = std::clamp(next.curve, 0.0f, 1.0f) - 0.5f;
        biasOffset = (std::clamp(next.bias, 0.0f, 1.0f) - 0.5f) * 1.6f;
        const float warmPosition = std::clamp(next.warm, 0.0f, 1.0f);
        warmGain = warmPosition <= 0.5f
            ? 0.35f + warmPosition * 1.3f
            : 1.0f + (warmPosition - 0.5f) * 4.0f;
        bodyAmount = (std::clamp(next.body, 0.0f, 1.0f) - 0.5f) * 2.0f;
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.7f;
        if (!controlsValid || next.outputDb != controls.outputDb)
        {
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
            else
            {
                const float position = (std::clamp(next.outputDb, -12.0f, 12.0f)
                    + 12.0f) * static_cast<float>(outputGainTable.size() - 1) / 24.0f;
                const size_t a = static_cast<size_t>(position);
                const size_t b = std::min(a + 1, outputGainTable.size() - 1);
                outputGain = outputGainTable[a]
                    + (outputGainTable[b] - outputGainTable[a])
                    * (position - static_cast<float>(a));
            }
        }
        controls = next;
        controlsValid = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const float safeAmount = std::clamp(amount, 0.0f, 1.0f);
        const size_t index = static_cast<size_t>(std::clamp(channel, 0, 1));
        float input = sample;
        if (bodyAmount != 0.0f)
        {
            bodyState[index] = (1.0f - bodyCoefficient) * input
                             + bodyCoefficient * bodyState[index];
            input += bodyState[index] * bodyAmount * 1.4f;
        }

        // Preserve the original saturation calculation and operation order
        // exactly at the neutral settings, including zero DRIVE.
        float result = input;
        if (safeAmount > 0.0f)
        {
            const float originalGain = 1.0f + safeAmount * 7.0f;
            const float gain = warmGain == 1.0f
                ? originalGain : originalGain * warmGain;
            const float normaliser = std::max(0.001f, std::atan(gain));
            const float outputCompensation = 1.0f / (1.0f + 1.2f * safeAmount);
            float wave = std::atan(input * gain);
            if (biasOffset != 0.0f)
            {
                const float zero = std::atan(biasOffset);
                wave = std::atan(input * gain + biasOffset) - zero;
            }
            float saturated = wave / normaliser * outputCompensation;
            if (curveOffset < 0.0f)
            {
                const float softGain = gain * 0.36f;
                const float softNorm = std::max(0.001f, std::atan(softGain));
                const float soft = std::atan(input * softGain + biasOffset)
                    - std::atan(biasOffset);
                const float alternative = soft / softNorm * outputCompensation;
                saturated += (alternative - saturated) * (-curveOffset * 2.0f);
            }
            else if (curveOffset > 0.0f)
            {
                const float zero = std::tanh(biasOffset);
                const float hard = std::tanh(input * gain + biasOffset) - zero;
                const float norm = std::max(0.001f, std::tanh(gain));
                const float alternative = hard / norm * outputCompensation;
                saturated += (alternative - saturated) * (curveOffset * 2.0f);
            }
            result = input + (saturated - input) * safeAmount;
        }
        if (toneAmount != 0.0f)
        {
            toneState[index] = (1.0f - toneCoefficient) * result
                             + toneCoefficient * toneState[index];
            result += (result - toneState[index]) * toneAmount;
        }
        if (outputGain != 1.0f)
            result *= outputGain;
        return std::isfinite(result) ? std::clamp(result, -32.0f, 32.0f) : 0.0f;
    }

private:
    Controls controls {};
    bool controlsValid = false;
    float bodyCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    float curveOffset = 0.0f;
    float biasOffset = 0.0f;
    float warmGain = 1.0f;
    float bodyAmount = 0.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    std::array<float, 257> outputGainTable {};
    std::array<float, 2> bodyState {};
    std::array<float, 2> toneState {};
};
} // namespace crusher::saturation
