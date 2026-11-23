#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Original Samurai high-pass emphasis, dead-zone, tanh/sine blades and DC
// blocking are arithmetically unchanged when all controls are neutral.
namespace crusher::samurai
{
struct Controls
{
    float edge = 0.5f;       // pre-shaper high-frequency emphasis
    float gate = 0.5f;       // dead-zone threshold
    float blade = 0.5f;      // nonlinear saturation hardness
    float ring = 0.5f;       // metallic sine harmonic contribution
    float tone = 0.5f;       // post-shaper tilt
    float outputDb = 0.0f;  // -12 to +12 dB
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        const auto coefficient = [fs](float frequency) noexcept
        {
            constexpr float twoPiLocal = 6.2831853071795864769f;
            return std::exp(-twoPiLocal * std::clamp(frequency, 1.0f, fs * 0.45f) / fs);
        };
        emphasisCoefficient = coefficient(2700.0f);
        dcCoefficient = coefficient(28.0f);
        postToneCoefficient = coefficient(3800.0f);
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
        emphasisState.fill(0.0f);
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
        toneState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (controlsValid && next.edge == controls.edge && next.gate == controls.gate
            && next.blade == controls.blade && next.ring == controls.ring
            && next.tone == controls.tone && next.outputDb == controls.outputDb)
            return;
        const float e = std::clamp(next.edge, 0.0f, 1.0f);
        const float g = std::clamp(next.gate, 0.0f, 1.0f);
        const float b = std::clamp(next.blade, 0.0f, 1.0f);
        const float r = std::clamp(next.ring, 0.0f, 1.0f);
        edgeFactor = e <= 0.5f ? 0.25f + 1.5f * e : 1.0f + 2.0f * (e - 0.5f);
        gateFactor = g <= 0.5f ? 0.25f + 1.5f * g : 1.0f + 4.0f * (g - 0.5f);
        bladeFactor = b <= 0.5f ? 0.3f + 1.4f * b : 1.0f + 3.0f * (b - 0.5f);
        ringFactor = r <= 0.5f ? r * 2.0f : 1.0f + (r - 0.5f) * 4.0f;
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.8f;
        if (!controlsValid || next.outputDb != controls.outputDb)
        {
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
            else
            {
                const float position = (std::clamp(next.outputDb, -12.0f, 12.0f) + 12.0f)
                    * static_cast<float>(outputGainTable.size() - 1) / 24.0f;
                const auto i = static_cast<size_t>(position);
                const auto j = std::min(i + 1, outputGainTable.size() - 1);
                outputGain = outputGainTable[i] + (outputGainTable[j] - outputGainTable[i])
                    * (position - static_cast<float>(i));
            }
        }
        controls = next;
        controlsValid = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float safeAmount = std::clamp(amount, 0.0f, 1.0f);
        emphasisState[index] = (1.0f - emphasisCoefficient) * sample
                             + emphasisCoefficient * emphasisState[index];
        const float edge = sample - emphasisState[index];
        // The neutral branches match the original calculation order exactly.
        const float emphasized = edgeFactor == 1.0f
            ? sample + edge * safeAmount * 2.8f
            : sample + edge * safeAmount * 2.8f * edgeFactor;
        const float driven = emphasized * (1.0f + safeAmount * 15.0f);
        const float originalDeadZone = 0.035f + safeAmount * 0.10f;
        const float deadZone = gateFactor == 1.0f
            ? originalDeadZone : originalDeadZone * gateFactor;
        const float slicedMagnitude = std::max(0.0f, std::abs(driven) - deadZone);
        const float sliced = std::copysign(slicedMagnitude, driven);
        const float originalHardness = 2.8f + safeAmount * 5.5f;
        const float hardness = bladeFactor == 1.0f
            ? originalHardness : originalHardness * bladeFactor;
        const float ringContribution = ringFactor == 1.0f
            ? std::sin(driven * 4.6f) * safeAmount * 0.10f
            : std::sin(driven * 4.6f) * safeAmount * 0.10f * ringFactor;
        const float blade = std::tanh(sliced * hardness) + ringContribution;
        const float dcBlocked = blade - previousInput[index]
                              + dcCoefficient * previousOutput[index];
        previousInput[index] = blade;
        const float safeDc = std::isfinite(dcBlocked) ? dcBlocked : 0.0f;
        previousOutput[index] = std::clamp(safeDc, -4.0f, 4.0f);
        const float outputCompensation = 0.87f / (1.0f + safeAmount * 0.78f);
        const float output = previousOutput[index] * outputCompensation;
        float result = sample + (output - sample) * safeAmount;
        if (toneAmount != 0.0f)
        {
            toneState[index] = (1.0f - postToneCoefficient) * result
                             + postToneCoefficient * toneState[index];
            result += (result - toneState[index]) * toneAmount;
        }
        if (outputGain != 1.0f)
            result *= outputGain;
        return std::isfinite(result) ? std::clamp(result, -32.0f, 32.0f) : 0.0f;
    }

private:
    Controls controls {};
    bool controlsValid = false;
    float emphasisCoefficient = 0.0f;
    float dcCoefficient = 0.0f;
    float postToneCoefficient = 0.0f;
    float edgeFactor = 1.0f;
    float gateFactor = 1.0f;
    float bladeFactor = 1.0f;
    float ringFactor = 1.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    std::array<float, 257> outputGainTable {};
    std::array<float, 2> emphasisState {};
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
    std::array<float, 2> toneState {};
};
} // namespace crusher::samurai
