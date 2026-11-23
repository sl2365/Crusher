#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Configurable Stressed waveshaper. All percentage controls at 50%, with
// OUTPUT at 0 dB, retain the original tanh/sine/DC-block arithmetic exactly.
namespace crusher::stressed
{
struct Controls
{
    float press = 0.5f;    // tanh compression pressure
    float fold = 0.5f;     // sine fold contribution
    float tension = 0.5f;  // folded waveform frequency
    float bias = 0.5f;     // input symmetry
    float tone = 0.5f;     // post-shaper tilt
    float outputDb = 0.0f; // independent -12..+12 dB
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        constexpr float twoPi = 6.2831853071795864769f;
        dcCoefficient = std::exp(-twoPi * std::min(26.0f, fs * 0.45f) / fs);
        toneCoefficient = std::exp(-twoPi * std::min(3400.0f, fs * 0.45f) / fs);
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
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
        toneState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (controlsValid && next.press == controls.press
            && next.fold == controls.fold && next.tension == controls.tension
            && next.bias == controls.bias && next.tone == controls.tone
            && next.outputDb == controls.outputDb)
            return;

        const float p = std::clamp(next.press, 0.0f, 1.0f);
        pressFactor = p <= 0.5f ? 0.3f + p * 1.4f : 1.0f + (p - 0.5f) * 5.0f;
        foldFactor = std::clamp(next.fold, 0.0f, 1.0f) * 2.0f;
        const float t = std::clamp(next.tension, 0.0f, 1.0f);
        tensionFactor = t <= 0.5f ? 0.3f + t * 1.4f : 1.0f + (t - 0.5f) * 3.5f;
        biasOffset = (std::clamp(next.bias, 0.0f, 1.0f) - 0.5f) * 2.4f;
        biasZero = biasOffset == 0.0f ? 0.0f : std::tanh(biasOffset);
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.75f;
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
        if (safeAmount <= 0.0f)
            return outputGain == 1.0f ? sample : sample * outputGain;

        const size_t index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float driveGain = 1.0f + safeAmount * 21.0f;
        const float driven = sample * driveGain;
        // This is the original operation order at the neutral defaults.
        const float shapeDrive = biasOffset == 0.0f ? driven : driven + biasOffset;
        float compressed = std::tanh(pressFactor == 1.0f
            ? shapeDrive : shapeDrive * pressFactor);
        if (biasOffset != 0.0f)
            compressed -= biasZero;
        const float foldPhase = (1.25f + safeAmount * 0.75f);
        const float folded = std::sin(tensionFactor == 1.0f && biasOffset == 0.0f
            ? driven * foldPhase
            : shapeDrive * (foldPhase * tensionFactor));
        const float originalFold = folded * safeAmount * 0.30f;
        const float foldedTerm = foldFactor == 1.0f
            ? originalFold : originalFold * foldFactor;
        const float stressed = compressed * (1.0f - safeAmount * 0.30f)
                             + foldedTerm
                             + compressed * compressed * safeAmount * 0.045f;
        const float dcBlocked = stressed - previousInput[index]
                              + dcCoefficient * previousOutput[index];
        previousInput[index] = stressed;
        previousOutput[index] = std::isfinite(dcBlocked)
            ? std::clamp(dcBlocked, -4.0f, 4.0f) : 0.0f;

        const float outputCompensation = 0.90f / (1.0f + safeAmount * 1.12f);
        const float output = previousOutput[index] * outputCompensation;
        float result = sample + (output - sample) * safeAmount;
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
    float dcCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    float pressFactor = 1.0f;
    float foldFactor = 1.0f;
    float tensionFactor = 1.0f;
    float biasOffset = 0.0f;
    float biasZero = 0.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    std::array<float, 257> outputGainTable {};
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
    std::array<float, 2> toneState {};
};
} // namespace crusher::stressed
