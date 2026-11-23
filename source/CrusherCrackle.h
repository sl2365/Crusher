#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Crackle-specific stochastic distortion. No allocations or JUCE dependency.
// Invoked only when Crackle is selected (or briefly outgoing during crossfade).
// Neutral controls reproduce the original Crackle algorithm.
namespace crusher::crackle
{
struct Controls
{
    float density = 0.5f;  // 0..1: event frequency, 50% = original
    float burst = 0.5f;    // 0..1: event amplitude, 50% = original
    float decay = 0.5f;    // 0..1: event length, 50% = original
    float grit = 0.5f;     // 0..1: saturation strength, 50% = original
    float tone = 0.5f;     // 0..1: dark/bright post-shaping, 50% neutral
    float outputDb = 0.0f; // -12..12 dB trim
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        sampleRateHz = fs;
        dcCoefficient = std::exp(-6.2831853071795864769f * 22.0f / fs);
        toneCoefficient = std::exp(-6.2831853071795864769f * 2800.0f / fs);
        reset();
    }

    void reset() noexcept
    {
        burstState.fill(0.0f);
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
        toneState.fill(0.0f);
        noiseState = { 0xa511e9b3u, 0x63d83595u };
    }

    void setControls(const Controls& next) noexcept
    {
        if (next.density == controls.density && next.burst == controls.burst
            && next.decay == controls.decay && next.grit == controls.grit
            && next.tone == controls.tone && next.outputDb == controls.outputDb)
            return;
        if (next.outputDb != controls.outputDb)
            outputGain = std::pow(10.0f,
                std::clamp(next.outputDb, -12.0f, 12.0f) / 20.0f);
        controls = next;
        // Each centred control multiplies the original behaviour by 1.0.
        densityScale = 0.20f + 1.60f * std::clamp(next.density, 0.0f, 1.0f);
        burstScale = 2.0f * std::clamp(next.burst, 0.0f, 1.0f);
        decayScale = 0.25f + 1.5f * std::clamp(next.decay, 0.0f, 1.0f);
        gritScale = 0.35f + 1.30f * std::clamp(next.grit, 0.0f, 1.0f);
        toneAmount = 2.0f * (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f);
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float a = std::clamp(amount, 0.0f, 1.0f);
        if (a <= 0.0f)
            return sample * outputGain;

        const float activity = std::clamp(std::abs(sample) * 2.5f, 0.0f, 1.0f);
        const float eventRateHz = 4.0f + a * a * 360.0f;
        const float eventProbability = (eventRateHz / sampleRateHz) * densityScale;
        const float trigger = nextNoise(noiseState[index]) * 0.5f + 0.5f;
        if (trigger < eventProbability * (0.20f + activity * 0.80f))
        {
            const float polarity = nextNoise(noiseState[index]);
            burstState[index] = polarity * (0.025f + a * 0.32f)
                * (0.30f + activity * 0.70f) * burstScale;
        }
        const float duration = (0.0035f + a * (0.00065f - 0.0035f))
            * decayScale;
        const float decay = std::clamp(1.0f - 1.0f / (duration * sampleRateHz),
                                        0.0f, 0.999999f);
        const float burst = burstState[index];
        burstState[index] *= decay;

        const float driveGain = (1.0f + a * 5.0f) * gritScale;
        const float normaliser = std::max(0.001f, std::tanh(driveGain));
        const float rough = std::tanh(sample * driveGain) / normaliser + burst;
        const float dcBlocked = rough - previousInput[index]
            + dcCoefficient * previousOutput[index];
        previousInput[index] = rough;
        previousOutput[index] = sanitise(dcBlocked, 4.0f);
        const float outputCompensation = 1.0f / (1.0f + a * 0.55f);
        float shaped = previousOutput[index] * outputCompensation;

        toneState[index] = (1.0f - toneCoefficient) * shaped
            + toneCoefficient * toneState[index];
        if (toneAmount != 0.0f)
            shaped += (shaped - toneState[index]) * (toneAmount < 0.0f
                ? toneAmount : 0.9f * toneAmount);
        const float out = (sample + (shaped - sample) * a) * outputGain;
        return std::isfinite(out) ? std::clamp(out, -8.0f, 8.0f) : 0.0f;
    }

private:
    static float nextNoise(uint32_t& state) noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(static_cast<int32_t>(state)) / 2147483648.0f;
    }
    static float sanitise(float value, float limit) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, -limit, limit) : 0.0f;
    }

    Controls controls {};
    float sampleRateHz = 44100.0f;
    float dcCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    float densityScale = 1.0f;
    float burstScale = 1.0f;
    float decayScale = 1.0f;
    float gritScale = 1.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    std::array<uint32_t, 2> noiseState { 0xa511e9b3u, 0x63d83595u };
    std::array<float, 2> burstState {};
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
    std::array<float, 2> toneState {};
};
}
