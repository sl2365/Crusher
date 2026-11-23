#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>

// Independently configurable Lightning model. The neutral path performs the
// original hard clipping -> signal-reactive noise -> DC blocker operations in
// the same arithmetic order, preserving the deterministic noise sequence.
namespace crusher::lightning
{
struct Controls
{
    float spark = 0.5f;      // Strength of the signal-reactive spark noise
    float react = 0.5f;      // Response of spark noise to incoming level
    float clip = 0.5f;       // Peak clipping ceiling
    float edge = 0.5f;       // Optional extra non-linear high-frequency edge
    float tone = 0.5f;       // Optional output tilt
    float outputDb = 0.0f;   // -12..+12 dB
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        const float twoPi = 6.2831853071795864769f;
        dcCoefficient = std::exp(-twoPi * std::min(28.0f, fs * 0.45f) / fs);
        toneCoefficient = 1.0f - std::exp(-twoPi * std::min(1850.0f, fs * 0.45f) / fs);
        for (size_t i = 0; i < outputGains.size(); ++i)
        {
            const float db = -12.0f + 24.0f * static_cast<float>(i)
                / static_cast<float>(outputGains.size() - 1);
            outputGains[i] = std::pow(10.0f, db / 20.0f);
        }
        reset();
        validControls = false;
        setControls(Controls {});
    }

    void reset() noexcept
    {
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
        toneState.fill(0.0f);
        noiseState = { 0x27d4eb2du, 0x165667b1u };
    }

    void setControls(const Controls& next) noexcept
    {
        if (validControls && next.spark == controls.spark
            && next.react == controls.react && next.clip == controls.clip
            && next.edge == controls.edge && next.tone == controls.tone
            && next.outputDb == controls.outputDb)
            return;

        sparkGain = 0.25f + 1.5f * std::clamp(next.spark, 0.0f, 1.0f);
        reactGain = 0.25f + 1.5f * std::clamp(next.react, 0.0f, 1.0f);
        clipCeiling = 0.4f + 1.2f * std::clamp(next.clip, 0.0f, 1.0f);
        edgeAmount = (std::clamp(next.edge, 0.0f, 1.0f) - 0.5f) * 1.4f;
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.7f;
        if (!validControls || next.outputDb != controls.outputDb)
        {
            const float position = (std::clamp(next.outputDb, -12.0f, 12.0f)
                + 12.0f) * (static_cast<float>(outputGains.size() - 1) / 24.0f);
            const size_t low = static_cast<size_t>(position);
            const size_t high = std::min(low + 1, outputGains.size() - 1);
            outputGain = outputGains[low] + (outputGains[high] - outputGains[low])
                * (position - static_cast<float>(low));
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
        }
        controls = next;
        validControls = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        // At DRIVE=0 only output trim is useful. No RNG, DC or waveshaping
        // work is performed and noise state does not advance.
        if (amount <= 0.0f)
            return outputGain == 1.0f ? sample : sample * outputGain;

        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float safeAmount = std::clamp(amount, 0.0f, 1.0f);
        const float activity = std::clamp(std::abs(sample) * 3.0f * reactGain, 0.0f, 1.0f);
        const float whiteNoise = nextNoise(noiseState[index])
                               * safeAmount * safeAmount * 0.075f
                               * (0.20f + activity * 0.80f) * sparkGain;
        const float driveGain = 1.0f + safeAmount * 18.0f;
        const float driven = sample * driveGain;
        const float hardClipped = std::clamp(driven, -clipCeiling, clipCeiling);
        float sharp = std::clamp(hardClipped + whiteNoise, -1.05f, 1.05f);
        if (edgeAmount != 0.0f)
            sharp = std::clamp(sharp + std::sin(sharp * 3.7f) * edgeAmount * 0.2f,
                               -1.8f, 1.8f);
        const float dcBlocked = sharp - previousInput[index]
                              + dcCoefficient * previousOutput[index];
        previousInput[index] = sharp;
        previousOutput[index] = sanitise(dcBlocked, 4.0f);
        const float outputCompensation = 0.92f / (1.0f + safeAmount * 1.35f);
        float shaped = previousOutput[index] * outputCompensation;
        if (toneAmount != 0.0f)
        {
            toneState[index] += toneCoefficient * (shaped - toneState[index]);
            shaped += (shaped - toneState[index]) * toneAmount;
        }
        float result = sample + (shaped - sample) * safeAmount;
        if (outputGain != 1.0f)
            result *= outputGain;
        return std::isfinite(result) ? result : 0.0f;
    }

private:
    static float nextNoise(uint32_t& state) noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(static_cast<int32_t>(state)) / 2147483648.0f;
    }

    static float sanitise(float x, float maxAbs) noexcept
    {
        return std::isfinite(x) ? std::clamp(x, -maxAbs, maxAbs) : 0.0f;
    }

    Controls controls {};
    bool validControls = false;
    float sparkGain = 1.0f, reactGain = 1.0f, clipCeiling = 1.0f;
    float edgeAmount = 0.0f, toneAmount = 0.0f, outputGain = 1.0f;
    float dcCoefficient = 0.0f, toneCoefficient = 0.0f;
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
    std::array<float, 2> toneState {};
    std::array<uint32_t, 2> noiseState { 0x27d4eb2du, 0x165667b1u };
    std::array<float, 257> outputGains {};
};
} // namespace crusher::lightning
