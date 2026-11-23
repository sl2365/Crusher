#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Standalone configurable Soft Clip. Neutral controls retain the exact
// tanh / compensation arithmetic from Crusher v0.1.30.
namespace crusher::softclip
{
struct Controls
{
    float threshold = 0.5f; // clipping onset
    float knee = 0.5f;      // softer atan to sharper clipped curve
    float bias = 0.5f;      // positive/negative asymmetry
    float body = 0.5f;      // bass pre-emphasis
    float tone = 0.5f;      // post-distortion spectral tilt
    float outputDb = 0.0f;  // independent -12 to +12dB
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        constexpr float twoPi = 6.2831853071795864769f;
        bodyCoefficient = std::exp(-twoPi * std::min(260.0f, fs * 0.45f) / fs);
        toneCoefficient = std::exp(-twoPi * std::min(3600.0f, fs * 0.45f) / fs);
        for (size_t i = 0; i < outputGainTable.size(); ++i)
        {
            const float db = -12.0f + 24.0f * static_cast<float>(i)
                / static_cast<float>(outputGainTable.size() - 1);
            outputGainTable[i] = std::pow(10.0f, db * 0.05f);
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
        if (controlsValid && next.threshold == controls.threshold
            && next.knee == controls.knee && next.bias == controls.bias
            && next.body == controls.body && next.tone == controls.tone
            && next.outputDb == controls.outputDb)
            return;

        const auto neutralised = [](float value) noexcept
        {
            return std::clamp(value, 0.0f, 1.0f) - 0.5f;
        };
        const float thresholdOffset = neutralised(next.threshold);
        thresholdGain = thresholdOffset < 0.0f
            ? 1.0f + thresholdOffset * 1.6f
            : 1.0f + thresholdOffset * 3.0f;
        kneeOffset = neutralised(next.knee);
        biasOffset = neutralised(next.bias) * 1.5f;
        bodyAmount = neutralised(next.body) * 2.0f;
        toneAmount = neutralised(next.tone) * 1.7f;
        if (!controlsValid || next.outputDb != controls.outputDb)
        {
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
            else
            {
                const float pos = (std::clamp(next.outputDb, -12.0f, 12.0f)
                    + 12.0f) * static_cast<float>(outputGainTable.size() - 1) / 24.0f;
                const size_t a = static_cast<size_t>(pos);
                const size_t b = std::min(a + 1, outputGainTable.size() - 1);
                outputGain = outputGainTable[a]
                    + (outputGainTable[b] - outputGainTable[a])
                        * (pos - static_cast<float>(a));
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
            input += bodyState[index] * bodyAmount * 1.5f;
        }
        float result = input;
        if (safeAmount > 0.0f)
        {
            // Never modify this original expression at neutral settings.
            const float driveGain = 1.0f + safeAmount * (16.0f - 1.0f);
            const float outputCompensation = 1.0f / (1.0f + 3.0f * safeAmount);
            const float gain = thresholdGain == 1.0f
                ? driveGain : driveGain * thresholdGain;
            float shaped = std::tanh(input * gain);
            if (biasOffset != 0.0f)
                shaped = std::tanh(input * gain + biasOffset)
                       - std::tanh(biasOffset);
            if (kneeOffset < 0.0f)
            {
                const float softer = (2.0f / 3.14159265358979323846f)
                    * (std::atan(input * gain + biasOffset) - std::atan(biasOffset));
                shaped += (softer - shaped) * (-kneeOffset * 2.0f);
            }
            else if (kneeOffset > 0.0f)
            {
                const float hard = std::clamp(input * gain + biasOffset, -1.0f, 1.0f)
                                 - std::clamp(biasOffset, -1.0f, 1.0f);
                shaped += (hard - shaped) * (kneeOffset * 2.0f);
            }
            const float saturated = shaped * outputCompensation;
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
    float thresholdGain = 1.0f;
    float kneeOffset = 0.0f;
    float biasOffset = 0.0f;
    float bodyAmount = 0.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    float bodyCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    std::array<float, 2> bodyState {};
    std::array<float, 2> toneState {};
    std::array<float, 257> outputGainTable {};
};
} // namespace crusher::softclip
