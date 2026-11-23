#pragma once

// Original, independently developed diode-style distortion for Crusher.
// Inspired by general diode-clipping circuit principles, NOT AIR DSP code.
// No allocations, exponentials, or coefficient generation on the audio thread.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace crusher::diodeclip
{
struct Controls
{
    float inputHP = 0.5f;
    float envelope = 0.5f;
    float diode = 0.5f;
    float outputHP = 0.5f;
    float outputLP = 0.5f;
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const double fs = std::max(8000.0, sampleRate);
        for (std::size_t i = 0; i < tableSize; ++i)
        {
            const float t = static_cast<float>(i)
                          / static_cast<float>(tableSize - 1);
            // Wide ranges intentionally extend beyond AIR's published controls.
            inputHPTable[i] = coefficient(35.0f * std::pow(1000.0f / 35.0f, t), fs);
            outputHPTable[i] = coefficient(30.0f * std::pow(4000.0f / 30.0f, t), fs);
            outputLPTable[i] = coefficient(1200.0f * std::pow(16000.0f / 1200.0f, t), fs);
            envelopeTable[i] = std::exp(-1.0f /
                (static_cast<float>(fs) * (0.0015f + 0.065f * t)));
        }
        reset();
        previous = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
        setControls(Controls{});
    }

    void reset() noexcept
    {
        for (auto& s : channels)
            s = {};
    }

    void setControls(const Controls& next) noexcept
    {
        const auto safe = [](float v) noexcept
        {
            return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
        };
        const float inHP = safe(next.inputHP);
        const float env = safe(next.envelope);
        const float diode = safe(next.diode);
        const float outHP = safe(next.outputHP);
        const float outLP = safe(next.outputLP);
        if (inHP != previous.inputHP) inputCoefficient = lookup(inputHPTable, inHP);
        if (env != previous.envelope) envelopeCoefficient = lookup(envelopeTable, env);
        if (outHP != previous.outputHP) outputHPCoefficient = lookup(outputHPTable, outHP);
        if (outLP != previous.outputLP) outputLPCoefficient = lookup(outputLPTable, outLP);
        if (diode != previous.diode)
        {
            // Unequal conduction curves produce odd and even harmonics.
            positiveThreshold = 0.32f + 0.68f * (1.0f - diode);
            negativeThreshold = 0.32f + 0.68f * diode;
        }
        previous = { inHP, env, diode, outHP, outLP };
    }

    float process(float sample, float drive, int channel) noexcept
    {
        if (! std::isfinite(sample)) sample = 0.0f;
        const std::size_t index = static_cast<std::size_t>(std::clamp(channel, 0, 1));
        auto& s = channels[index];
        const float d = std::isfinite(drive) ? std::clamp(drive, 0.0f, 1.0f) : 0.0f;
        // High-pass before saturation: its cutoff changes which frequencies clip.
        s.inputLow += (1.0f - inputCoefficient) * (sample - s.inputLow);
        const float pre = sample - s.inputLow;
        s.envelope = envelopeCoefficient * s.envelope
                   + (1.0f - envelopeCoefficient) * std::abs(pre);
        const float v = pre * (1.0f + 14.0f * d);
        const float dynamicThreshold = 0.7f + 0.55f * std::min(s.envelope, 2.0f);
        const float threshold = (v >= 0.0f ? positiveThreshold : negativeThreshold)
                              * dynamicThreshold;
        // Rational, smooth diode transfer: no per-sample transcendentals.
        const float clipped = (v * threshold) / (threshold + std::abs(v));
        s.outputHighLow += (1.0f - outputHPCoefficient)
                         * (clipped - s.outputHighLow);
        const float highPassed = clipped - s.outputHighLow;
        s.outputLow += (1.0f - outputLPCoefficient) * (highPassed - s.outputLow);
        const float wet = std::clamp(s.outputLow * 2.3f, -3.0f, 3.0f);
        return sample + (wet - sample) * d;
    }

private:
    struct State
    {
        float inputLow = 0.0f;
        float envelope = 0.0f;
        float outputHighLow = 0.0f;
        float outputLow = 0.0f;
    };
    static constexpr std::size_t tableSize = 257;
    static float coefficient(float hz, double sampleRate) noexcept
    {
        return std::exp(-6.283185307179586f * hz / static_cast<float>(sampleRate));
    }
    static float lookup(const std::array<float, tableSize>& table, float value) noexcept
    {
        const float at = value * static_cast<float>(tableSize - 1);
        const auto i = static_cast<std::size_t>(at);
        if (i >= tableSize - 1) return table.back();
        const float fraction = at - static_cast<float>(i);
        return table[i] + fraction * (table[i + 1] - table[i]);
    }

    std::array<State, 2> channels {};
    std::array<float, tableSize> inputHPTable {};
    std::array<float, tableSize> envelopeTable {};
    std::array<float, tableSize> outputHPTable {};
    std::array<float, tableSize> outputLPTable {};
    // Values must differ on the initial update so caches are initialised.
    Controls previous { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float inputCoefficient = 0.0f;
    float envelopeCoefficient = 0.0f;
    float outputHPCoefficient = 0.0f;
    float outputLPCoefficient = 0.0f;
    float positiveThreshold = 0.5f;
    float negativeThreshold = 0.5f;
};
} // namespace crusher::diodeclip
