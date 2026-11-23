#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Original Crusher Asym DSP. No JUCE dependency: the model can be regression
// tested independently of the plugin/editor. All controls are per-Asym only.
namespace crusher::asym
{
struct Controls
{
    float bias = 0.0f;      // -1 .. 1, 0 = original Asym bias
    float skew = 0.0f;      // -1 .. 1, 0 = original positive/negative balance
    float shape = 0.5f;     // 0 soft (atan), .5 original (tanh), 1 hard clip
    float tone = 0.5f;      // 0 dark, .5 untouched, 1 presence
    float outputDb = 0.0f;  // -12 .. +12 dB
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        constexpr double twoPi = 6.2831853071795864769;
        const double fs = std::max(1.0, sampleRate);
        dcCoefficient = static_cast<float>(std::exp(-twoPi * 22.0 / fs));
        toneCoefficient = static_cast<float>(std::exp(-twoPi * 3200.0 / fs));
        reset();
    }

    void reset() noexcept
    {
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
        toneState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (controls.bias == next.bias && controls.skew == next.skew
            && controls.shape == next.shape && controls.tone == next.tone
            && controls.outputDb == next.outputDb)
            return;
        controls = next;
        const float skew = std::clamp(controls.skew, -1.0f, 1.0f);
        positiveSkewGain = std::exp(skew * 1.10f);
        negativeSkewGain = std::exp(-skew * 1.10f);
        outputTrim = std::pow(10.0f,
            std::clamp(controls.outputDb, -12.0f, 12.0f) / 20.0f);
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float a = std::clamp(amount, 0.0f, 1.0f);
        // Keep the original processing at neutral values, including DC
        // removal, drive blending, gain normalisation and output compensation.
        const float gain = 1.0f + a * 11.0f;
        const float offset = 0.035f + a * 0.20f
                           + std::clamp(controls.bias, -1.0f, 1.0f) * 0.52f;
        const float zero = std::tanh(offset);
        const float posGain = gain * (1.0f + a * 0.55f) * positiveSkewGain;
        const float negGain = gain * (0.72f + a * 0.18f) * negativeSkewGain;
        const float selectedGain = sample >= 0.0f ? posGain : negGain;
        const float shape = std::clamp(controls.shape, 0.0f, 1.0f);
        const float tanhZero = zero;
        const float tanhNorm = sample >= 0.0f
            ? std::max(0.001f, std::tanh(posGain + offset) - tanhZero)
            : std::max(0.001f, tanhZero - std::tanh(-negGain + offset));
        const float tanhShaped = (std::tanh(sample * selectedGain + offset)
                                - tanhZero) / tanhNorm;
        float shaped = tanhShaped;
        if (shape < 0.5f)
        {
            const float atanZero = std::atan(offset);
            const float atanNorm = sample >= 0.0f
                ? std::max(0.001f, std::atan(posGain + offset) - atanZero)
                : std::max(0.001f, atanZero - std::atan(-negGain + offset));
            const float soft = (std::atan(sample * selectedGain + offset)
                               - atanZero) / atanNorm;
            shaped = soft + (tanhShaped - soft) * (shape * 2.0f);
        }
        else if (shape > 0.5f)
        {
            // Piecewise hard knee; all branches are zero-centred so the
            // existing DC blocker handles severe bias/skew settings safely.
            const float clipZero = std::clamp(offset, -1.0f, 1.0f);
            const float clipNorm = sample >= 0.0f
                ? std::max(0.001f, 1.0f - clipZero)
                : std::max(0.001f, clipZero + 1.0f);
            const float hard = (std::clamp(sample * selectedGain + offset,
                                          -1.0f, 1.0f) - clipZero) / clipNorm;
            shaped += (hard - tanhShaped) * ((shape - 0.5f) * 2.0f);
        }

        const float dcBlocked = shaped - previousInput[index]
                              + dcCoefficient * previousOutput[index];
        previousInput[index] = shaped;
        previousOutput[index] = std::clamp(
            std::isfinite(dcBlocked) ? dcBlocked : 0.0f, -4.0f, 4.0f);
        const float clean = previousOutput[index];
        toneState[index] = (1.0f - toneCoefficient) * clean
                         + toneCoefficient * toneState[index];
        const float tone = std::clamp(controls.tone, 0.0f, 1.0f);
        float colored = clean;
        if (tone < 0.5f)
            colored += (toneState[index] - clean) * (1.0f - 2.0f * tone);
        else if (tone > 0.5f)
            colored += (clean - toneState[index]) * ((tone - 0.5f) * 2.6f);

        const float compensation = 0.94f / (1.0f + a * 0.68f);
        const float effected = colored * compensation * outputTrim;
        const float output = sample + (effected - sample) * a;
        return std::isfinite(output) ? std::clamp(output, -16.0f, 16.0f) : 0.0f;
    }

private:
    Controls controls;
    float positiveSkewGain = 1.0f;
    float negativeSkewGain = 1.0f;
    float outputTrim = 1.0f;
    float dcCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
    std::array<float, 2> toneState {};
};
}
