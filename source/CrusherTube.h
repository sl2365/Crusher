#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Original Tube saturation at neutral settings, with optional pre/post
// shaping. This is the standalone Tube model, not HumpCrusher's TUBE stage.
namespace crusher::tube
{
struct Controls
{
    float bias = 0.5f;   // extra positive/negative asymmetry
    float heat = 0.5f;   // input saturation intensity
    float sag = 0.5f;    // envelope-dependent drive response
    float body = 0.5f;   // pre-drive low-frequency tilt
    float tone = 0.5f;   // post-drive brightness tilt
    float outputDb = 0.0f;
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        constexpr float twoPi = 6.2831853071795864769f;
        bodyCoefficient = std::exp(-twoPi * std::min(240.0f, fs * 0.45f) / fs);
        toneCoefficient = std::exp(-twoPi * std::min(3000.0f, fs * 0.45f) / fs);
        sagAttackCoefficient = std::exp(-twoPi * std::min(35.0f, fs * 0.45f) / fs);
        sagReleaseCoefficient = std::exp(-twoPi * std::min(5.0f, fs * 0.45f) / fs);
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
        sagEnvelope.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (controlsValid && next.bias == controls.bias
            && next.heat == controls.heat && next.sag == controls.sag
            && next.body == controls.body && next.tone == controls.tone
            && next.outputDb == controls.outputDb)
            return;

        biasOffset = (std::clamp(next.bias, 0.0f, 1.0f) - 0.5f) * 1.6f;
        const float heat = std::clamp(next.heat, 0.0f, 1.0f);
        heatMultiplier = heat <= 0.5f ? 0.25f + heat * 1.5f
                                       : 1.0f + (heat - 0.5f) * 3.0f;
        sagAmount = (std::clamp(next.sag, 0.0f, 1.0f) - 0.5f) * 1.8f;
        bodyAmount = (std::clamp(next.body, 0.0f, 1.0f) - 0.5f) * 1.6f;
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.7f;
        if (!controlsValid || next.outputDb != controls.outputDb)
        {
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
            else
            {
                const float position = (std::clamp(next.outputDb, -12.0f, 12.0f)
                    + 12.0f) * static_cast<float>(outputGainTable.size() - 1) / 24.0f;
                const auto i = static_cast<size_t>(position);
                const auto j = std::min(i + 1, outputGainTable.size() - 1);
                outputGain = outputGainTable[i]
                    + (outputGainTable[j] - outputGainTable[i])
                    * (position - static_cast<float>(i));
            }
        }
        controls = next;
        controlsValid = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const float safeAmount = std::clamp(amount, 0.0f, 1.0f);
        if (safeAmount <= 0.0f && bodyAmount == 0.0f
            && toneAmount == 0.0f)
            return outputGain == 1.0f ? sample : sample * outputGain;

        const size_t index = static_cast<size_t>(std::clamp(channel, 0, 1));
        float input = sample;
        if (bodyAmount != 0.0f)
        {
            bodyState[index] = (1.0f - bodyCoefficient) * input
                             + bodyCoefficient * bodyState[index];
            input += bodyState[index] * bodyAmount * 1.5f;
        }
        if (safeAmount > 0.0f && sagAmount != 0.0f)
        {
            const float magnitude = std::min(std::abs(input), 3.0f);
            const float coefficient = magnitude > sagEnvelope[index]
                ? sagAttackCoefficient : sagReleaseCoefficient;
            sagEnvelope[index] = (1.0f - coefficient) * magnitude
                               + coefficient * sagEnvelope[index];
            const float envelope = sagEnvelope[index] / (0.2f + sagEnvelope[index]);
            input *= std::clamp(1.0f - sagAmount * envelope * 0.8f, 0.15f, 1.8f);
        }

        float result = input;
        if (safeAmount > 0.0f)
        {
            // Keep the original arithmetic and operation order untouched
            // when the six new controls are at their neutral defaults.
            const float originalGain = 1.0f + safeAmount * 9.0f;
            const float driveGain = heatMultiplier == 1.0f
                ? originalGain : originalGain * heatMultiplier;
            const float originalBias = 0.05f + safeAmount * 0.25f;
            const float bias = biasOffset == 0.0f
                ? originalBias : originalBias + biasOffset;
            const float zeroOffset = std::tanh(bias);
            const float reference = std::tanh(0.80f * driveGain + bias)
                                  - zeroOffset;
            const float normaliser = std::max(0.001f, reference);
            const float outputCompensation = 1.0f / (1.0f + 0.9f * safeAmount);
            const float saturated = (std::tanh(input * driveGain + bias) - zeroOffset)
                                  / normaliser * outputCompensation;
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
    float sagAttackCoefficient = 0.0f;
    float sagReleaseCoefficient = 0.0f;
    float biasOffset = 0.0f;
    float heatMultiplier = 1.0f;
    float sagAmount = 0.0f;
    float bodyAmount = 0.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    std::array<float, 257> outputGainTable {};
    std::array<float, 2> bodyState {};
    std::array<float, 2> toneState {};
    std::array<float, 2> sagEnvelope {};
};
} // namespace crusher::tube
