#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Original Rectify: abs(tanh(input * drive)), 18 Hz DC block and the
// original 0.78 / (1 + 0.65 * DRIVE) gain compensation. With the new
// controls neutral, no additional arithmetic affects this signal path.
namespace crusher::rectify
{
struct Controls
{
    float wave = 0.5f;       // 0 positive half-wave, 0.5 full-wave, 1 negative half-wave
    float bias = 0.5f;       // input offset/asymmetric rectification
    float curve = 0.5f;      // softer/harder rectified shape
    float fold = 0.0f;       // additional post-rectifier waveform folds
    float tone = 0.5f;       // post-rectifier dark/bright tone
    float outputDb = 0.0f;  // -12 to +12 dB
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const auto fs = static_cast<float>(std::max(1.0, sampleRate));
        constexpr float twoPi = 6.2831853071795864769f;
        dcCoefficient = std::exp(-twoPi * std::clamp(18.0f, 1.0f, fs * 0.45f) / fs);
        toneCoefficient = std::exp(-twoPi * std::clamp(2800.0f, 1.0f, fs * 0.45f) / fs);
        for (size_t i = 0; i < outputGainTable.size(); ++i)
        {
            const float dB = -12.0f + 24.0f * static_cast<float>(i)
                / static_cast<float>(outputGainTable.size() - 1);
            outputGainTable[i] = std::pow(10.0f, dB * 0.05f);
        }
        reset();
        valid = false;
        setControls(controls);
    }

    void reset() noexcept
    {
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
        toneState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (valid && next.wave == controls.wave && next.bias == controls.bias
            && next.curve == controls.curve && next.fold == controls.fold
            && next.tone == controls.tone && next.outputDb == controls.outputDb)
            return;
        if (!valid || next.outputDb != controls.outputDb)
        {
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
            else
            {
                const float position = (std::clamp(next.outputDb, -12.0f, 12.0f)
                    + 12.0f) * static_cast<float>(outputGainTable.size() - 1) / 24.0f;
                const auto index = static_cast<size_t>(position);
                const auto nextIndex = std::min(index + 1, outputGainTable.size() - 1);
                outputGain = outputGainTable[index]
                    + (outputGainTable[nextIndex] - outputGainTable[index])
                        * (position - static_cast<float>(index));
            }
        }
        if (!valid || next.bias != controls.bias)
        {
            biasOffset = (std::clamp(next.bias, 0.0f, 1.0f) - 0.5f) * 1.6f;
            biasReference = biasOffset == 0.0f ? 0.0f : std::tanh(biasOffset);
        }
        wavePosition = std::clamp(next.wave, 0.0f, 1.0f);
        curvePosition = std::clamp(next.curve, 0.0f, 1.0f);
        foldAmount = std::clamp(next.fold, 0.0f, 1.0f);
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.8f;
        controls = next;
        valid = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float safeAmount = std::clamp(amount, 0.0f, 1.0f);
        const float driveGain = 1.0f + safeAmount * 11.0f;
        float driven = std::tanh(sample * driveGain + biasOffset);
        if (biasOffset != 0.0f)
            driven -= biasReference;
        float rectified = std::abs(driven);

        if (wavePosition != 0.5f)
        {
            const float halfWave = wavePosition < 0.5f
                ? std::max(driven, 0.0f) : std::max(-driven, 0.0f);
            const float blend = std::abs(wavePosition - 0.5f) * 2.0f;
            rectified += (halfWave - rectified) * blend;
        }
        if (curvePosition < 0.5f)
        {
            const float harder = std::copysign(rectified * rectified, rectified);
            rectified += (harder - rectified) * (1.0f - 2.0f * curvePosition);
        }
        else if (curvePosition > 0.5f)
        {
            const float softer = std::copysign(std::sqrt(std::abs(rectified)), rectified);
            rectified += (softer - rectified) * (2.0f * curvePosition - 1.0f);
        }
        if (foldAmount > 0.0f)
        {
            const float scaled = rectified * (1.0f + 6.0f * foldAmount);
            const float phase = scaled - 4.0f * std::floor(scaled * 0.25f);
            const float folded = phase < 1.0f ? phase
                : phase < 3.0f ? 2.0f - phase : phase - 4.0f;
            rectified += (folded - rectified) * foldAmount;
        }

        const float dcBlocked = rectified - previousInput[index]
                              + dcCoefficient * previousOutput[index];
        previousInput[index] = rectified;
        previousOutput[index] = std::isfinite(dcBlocked)
            ? std::clamp(dcBlocked, -4.0f, 4.0f) : 0.0f;
        const float outputCompensation = 0.78f / (1.0f + 0.65f * safeAmount);
        const float shaped = previousOutput[index] * outputCompensation;
        float result = sample + (shaped - sample) * safeAmount;
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
    bool valid = false;
    float dcCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    float biasOffset = 0.0f;
    float biasReference = 0.0f;
    float wavePosition = 0.5f;
    float curvePosition = 0.5f;
    float foldAmount = 0.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    std::array<float, 257> outputGainTable {};
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
    std::array<float, 2> toneState {};
};
} // namespace crusher::rectify
