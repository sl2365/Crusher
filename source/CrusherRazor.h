#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Razor's original high-frequency pre-emphasis, tanh/sine wave shaper and
// DC blocker are reproduced verbatim when these six new controls are neutral.
// This single processor replaces the previous fixed Razor implementation.
namespace crusher::razor
{
struct Controls
{
    float edge = 0.5f;   // 0..1: pre-drive high-frequency emphasis
    float freq = 0.5f;   // 0..1: emphasis cutoff, centre = original 2.1 kHz
    float ring = 0.5f;   // 0..1: sine/ringing harmonics, centre = original
    float bite = 0.5f;   // 0..1: pre-shaper gain, centre = original
    float tone = 0.5f;   // 0..1: post distortion dark/bright tilt
    float outputDb = 0.0f; // -12..12 dB, centre = unity
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const auto fs = static_cast<float>(std::max(1.0, sampleRate));
        const auto coefficientFor = [fs](float hz) noexcept
        {
            constexpr float twoPi = 6.2831853071795864769f;
            const float cutoff = std::clamp(hz, 1.0f, fs * 0.45f);
            return std::exp(-twoPi * cutoff / fs);
        };
        const auto frequencyAt = [](float p) noexcept
        {
            return p <= 0.5f ? 350.0f + 3500.0f * p
                             : 2100.0f + (p - 0.5f) * 16800.0f;
        };
        for (size_t i = 0; i < frequencyTable.size(); ++i)
            frequencyTable[i] = coefficientFor(frequencyAt(
                static_cast<float>(i) / static_cast<float>(frequencyTable.size() - 1)));
        // Exact original coefficient, calculated with the same floating-point
        // operations as the old onePoleCoefficient(2100, sampleRate) call.
        originalToneCoefficient = coefficientFor(2100.0f);
        dcCoefficient = coefficientFor(26.0f);
        postToneCoefficient = coefficientFor(3000.0f);
        for (size_t i = 0; i < gainTable.size(); ++i)
        {
            const float db = -12.0f + 24.0f * static_cast<float>(i)
                / static_cast<float>(gainTable.size() - 1);
            gainTable[i] = std::pow(10.0f, db * 0.05f);
        }
        reset();
        valid = false;
        setControls(controls);
    }

    void reset() noexcept
    {
        emphasisState.fill(0.0f);
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
        postToneState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (valid && next.edge == controls.edge && next.freq == controls.freq
            && next.ring == controls.ring && next.bite == controls.bite
            && next.tone == controls.tone && next.outputDb == controls.outputDb)
            return;
        if (!valid || next.freq != controls.freq)
        {
            const float f = std::clamp(next.freq, 0.0f, 1.0f);
            if (f == 0.5f)
                emphasisCoefficient = originalToneCoefficient;
            else
            {
                const float position = f * static_cast<float>(frequencyTable.size() - 1);
                const auto first = static_cast<size_t>(position);
                const auto second = std::min(first + 1, frequencyTable.size() - 1);
                emphasisCoefficient = frequencyTable[first]
                    + (frequencyTable[second] - frequencyTable[first])
                        * (position - static_cast<float>(first));
            }
        }
        if (!valid || next.outputDb != controls.outputDb)
        {
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
            else
            {
                const float position = (std::clamp(next.outputDb, -12.0f, 12.0f)
                    + 12.0f) * static_cast<float>(gainTable.size() - 1) / 24.0f;
                const auto first = static_cast<size_t>(position);
                const auto second = std::min(first + 1, gainTable.size() - 1);
                outputGain = gainTable[first] + (gainTable[second] - gainTable[first])
                    * (position - static_cast<float>(first));
            }
        }
        const float e = std::clamp(next.edge, 0.0f, 1.0f);
        edgeGain = 0.1f + 1.8f * e; // exactly one at 50%
        const float b = std::clamp(next.bite, 0.0f, 1.0f);
        biteGain = b <= 0.5f ? 0.25f + 1.5f * b : 1.0f + 3.0f * (b - 0.5f);
        const float h = std::clamp(next.ring, 0.0f, 1.0f);
        ringGain = h <= 0.5f ? 2.0f * h : 1.0f + 3.0f * (h - 0.5f);
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.8f;
        controls = next;
        valid = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const float safeAmount = std::clamp(amount, 0.0f, 1.0f);
        // The original processor is skipped at DRIVE=0 by the outer dispatcher.
        // OUTPUT is still available at DRIVE=0 without running the shaper.
        if (safeAmount <= 0.0f)
            return outputGain == 1.0f ? sample : sample * outputGain;

        const size_t index = static_cast<size_t>(std::clamp(channel, 0, 1));
        emphasisState[index] = (1.0f - emphasisCoefficient) * sample
                              + emphasisCoefficient * emphasisState[index];
        const float edge = sample - emphasisState[index];
        const float preEmphasised = sample
            + edge * safeAmount * (1.2f + safeAmount * 2.4f) * edgeGain;
        const float driveGain = (1.0f + safeAmount * 19.0f) * biteGain;
        const float driven = preEmphasised * driveGain;
        const float sharp = std::tanh(driven)
            + std::sin(driven * (2.6f + safeAmount * 1.8f))
                * safeAmount * 0.15f * ringGain;
        const float dcBlocked = sharp - previousInput[index]
                              + dcCoefficient * previousOutput[index];
        previousInput[index] = sharp;
        previousOutput[index] = std::isfinite(dcBlocked)
            ? std::clamp(dcBlocked, -4.0f, 4.0f) : 0.0f;
        const float outputCompensation = 0.88f / (1.0f + safeAmount * 1.10f);
        const float output = previousOutput[index] * outputCompensation;
        float result = sample + (output - sample) * safeAmount;
        if (toneAmount != 0.0f)
        {
            postToneState[index] = (1.0f - postToneCoefficient) * result
                                  + postToneCoefficient * postToneState[index];
            result += (result - postToneState[index]) * toneAmount;
        }
        if (outputGain != 1.0f)
            result *= outputGain;
        return std::isfinite(result) ? std::clamp(result, -32.0f, 32.0f) : 0.0f;
    }

private:
    Controls controls {};
    bool valid = false;
    float originalToneCoefficient = 0.0f;
    float emphasisCoefficient = 0.0f;
    float dcCoefficient = 0.0f;
    float postToneCoefficient = 0.0f;
    float edgeGain = 1.0f, ringGain = 1.0f, biteGain = 1.0f;
    float toneAmount = 0.0f, outputGain = 1.0f;
    std::array<float, 257> frequencyTable {};
    std::array<float, 257> gainTable {};
    std::array<float, 2> emphasisState {};
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
    std::array<float, 2> postToneState {};
};
} // namespace crusher::razor
