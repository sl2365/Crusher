#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Original Fresh high-frequency exciter with additional contextual controls.
// No JUCE dependency or audio-thread allocation. Only the selected Fresh
// engine (or its outgoing crossfade) is processed.
namespace crusher::fresh
{
struct Controls
{
    float frequency = 0.5f;  // 0..1: 500..13000 Hz, 50% = original 3400 Hz
    float excite = 0.5f;     // 0..1: high-band saturator strength
    float air = 0.5f;        // 0..1: amount of excited high-band added
    float saturation = 0.5f; // 0..1: final soft saturation
    float tone = 0.5f;       // 0..1: darker/brighter post-shaping
    float outputDb = 0.0f;   // -12..12 dB, after the original wet blend
};

class Processor
{
public:
    void prepare(double rate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, rate));
        const float twoPi = 6.2831853071795864769f;
        for (size_t i = 0; i < frequencyTable.size(); ++i)
        {
            const float t = static_cast<float>(i) /
                static_cast<float>(frequencyTable.size() - 1);
            const float freq = t <= 0.5f ? 500.0f + t * 5800.0f
                : 3400.0f + (t - 0.5f) * 19200.0f;
            frequencyTable[i] = std::exp(-twoPi *
                std::clamp(freq, 1.0f, fs * 0.45f) / fs);
        }
        // Fixed tone filter is inexpensive and remains stateful so an
        // automated TONE change cannot abruptly initialise its history.
        toneCoefficient = std::exp(-twoPi *
            std::clamp(4000.0f, 1.0f, fs * 0.45f) / fs);
        reset();
        controlsValid = false; // Recompute the coefficient for this sample rate.
        setControls(controls);
    }

    void reset() noexcept
    {
        highPassLowState.fill(0.0f);
        toneState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (next.frequency == controls.frequency && next.excite == controls.excite
            && next.air == controls.air && next.saturation == controls.saturation
            && next.tone == controls.tone && next.outputDb == controls.outputDb
            && controlsValid)
            return;

        if (!controlsValid || next.frequency != controls.frequency)
        {
            const float pos = std::clamp(next.frequency, 0.0f, 1.0f)
                            * static_cast<float>(frequencyTable.size() - 1);
            const auto index = static_cast<size_t>(pos);
            const auto nextIndex = std::min(index + 1, frequencyTable.size() - 1);
            const float fraction = pos - static_cast<float>(index);
            highPassCoefficient = frequencyTable[index]
                + (frequencyTable[nextIndex] - frequencyTable[index]) * fraction;
        }
        if (!controlsValid || next.outputDb != controls.outputDb)
            outputGain = std::pow(10.0f,
                std::clamp(next.outputDb, -12.0f, 12.0f) / 20.0f);
        exciteScale = 0.25f + 1.5f * std::clamp(next.excite, 0.0f, 1.0f);
        airScale = 2.0f * std::clamp(next.air, 0.0f, 1.0f);
        saturationScale = 0.35f + 1.3f * std::clamp(next.saturation, 0.0f, 1.0f);
        toneAmount = 2.0f * (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f);
        controls = next;
        controlsValid = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float a = std::clamp(amount, 0.0f, 1.0f);
        if (a <= 0.0f)
            return sanitise(sample * outputGain);

        highPassLowState[index] = (1.0f - highPassCoefficient) * sample
            + highPassCoefficient * highPassLowState[index];
        const float highBand = sample - highPassLowState[index];
        const float exciterGain = (2.0f + a * 10.0f) * exciteScale;
        const float exciterNormaliser = std::max(0.001f, std::tanh(exciterGain));
        const float excitedHigh = std::tanh(highBand * exciterGain)
            / exciterNormaliser;
        const float brightened = sample
            + excitedHigh * a * (0.32f + a * 0.40f) * airScale;
        const float outputDrive = (1.0f + a * 0.55f) * saturationScale;
        const float outputNormaliser = std::max(0.001f, std::tanh(outputDrive));
        const float output = std::tanh(brightened * outputDrive)
            / outputNormaliser / (1.0f + a * 0.12f);
        const float saturated = sanitiseWet(output);
        float result = sample + (saturated - sample) * a;
        toneState[index] = (1.0f - toneCoefficient) * result
                         + toneCoefficient * toneState[index];
        if (toneAmount != 0.0f)
            result += (result - toneState[index])
                * (toneAmount < 0.0f ? toneAmount : 0.85f * toneAmount);
        return sanitise(result * outputGain);
    }

private:
    static float sanitiseWet(float value) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, -2.0f, 2.0f) : 0.0f;
    }
    static float sanitise(float value) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, -8.0f, 8.0f) : 0.0f;
    }

    Controls controls {};
    bool controlsValid = false;
    float highPassCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    float exciteScale = 1.0f;
    float airScale = 1.0f;
    float saturationScale = 1.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    std::array<float, 129> frequencyTable {};
    std::array<float, 2> highPassLowState {};
    std::array<float, 2> toneState {};
};
} // namespace crusher::fresh
