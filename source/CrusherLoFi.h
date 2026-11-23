#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Original multi-stage digital degradation model. It does not share state
// with Pixelated or use any third-party Lo-Fi DSP. All settings are real-time
// safe; coefficient lookup tables are generated in prepare(), never in process().
namespace crusher::lofi
{
struct Controls
{
    float rate = 0.5f;      // 0 = host rate; 1 = approximately 180 Hz capture
    float bits = 0.5f;      // continuously morph between 1 and 16 bits
    float clip = 0.0f;      // independent hard-clip threshold / makeup
    float rectify = 0.0f;   // dry polarity -> full-wave rectification
    float noise = 0.0f;     // deterministic pre-crush noise
    float preAA = 0.0f;     // input-to-decimator anti-alias filtering
    float postAA = 0.0f;    // post-hold reconstruction filtering
};

class Processor
{
public:
    void prepare(double rateHz) noexcept
    {
        const float sampleRate = static_cast<float>(std::max(1000.0, rateHz));
        constexpr float tau = 6.2831853071795864769f;
        for (size_t i = 0; i < tableLength; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(tableLength - 1);
            const float captureHz = sampleRate * std::pow(180.0f / sampleRate, t);
            captureRate[i] = std::clamp(captureHz / sampleRate, 180.0f / sampleRate, 1.0f);
            const float preCut = std::min(sampleRate * 0.45f, captureHz * 0.42f);
            const float postCut = std::min(sampleRate * 0.45f, captureHz * 0.48f);
            preCoefficients[i] = 1.0f - std::exp(-tau * preCut / sampleRate);
            postCoefficients[i] = 1.0f - std::exp(-tau * postCut / sampleRate);
        }
        previous = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
        setControls(Controls {});
        reset();
    }

    void reset() noexcept
    {
        held.fill(0.0f);
        phase.fill(1.0f);
        preOne.fill(0.0f);
        preTwo.fill(0.0f);
        postOne.fill(0.0f);
        postTwo.fill(0.0f);
        noiseStates = { 0x71a36245u, 0x9e3779b9u };
    }

    void setControls(const Controls& next) noexcept
    {
        if (next.rate == previous.rate && next.bits == previous.bits
            && next.clip == previous.clip && next.rectify == previous.rectify
            && next.noise == previous.noise && next.preAA == previous.preAA
            && next.postAA == previous.postAA)
            return;

        const float rateIndex = std::clamp(next.rate, 0.0f, 1.0f)
            * static_cast<float>(tableLength - 1);
        const auto a = static_cast<size_t>(rateIndex);
        const auto b = std::min(a + 1, tableLength - 1);
        const float frac = rateIndex - static_cast<float>(a);
        const auto interpolated = [a, b, frac](const auto& table) noexcept
        { return table[a] + (table[b] - table[a]) * frac; };
        captureIncrement = interpolated(captureRate);
        preCoeff = interpolated(preCoefficients);
        postCoeff = interpolated(postCoefficients);
        bitPosition = 1.0f + 15.0f * std::clamp(next.bits, 0.0f, 1.0f);
        clipThreshold = 1.0f - 0.90f * std::clamp(next.clip, 0.0f, 1.0f);
        clipActive = next.clip > 0.0f;
        rectifyBlend = std::clamp(next.rectify, 0.0f, 1.0f);
        const float noise = std::clamp(next.noise, 0.0f, 1.0f);
        noiseGain = 0.22f * noise * noise;
        preBlend = std::clamp(next.preAA, 0.0f, 1.0f);
        postBlend = std::clamp(next.postAA, 0.0f, 1.0f);
        previous = next;
    }

    float process(float sample, float drive, int channel) noexcept
    {
        if (drive <= 0.0f) return sample;
        const size_t ch = static_cast<size_t>(std::clamp(channel, 0, 1));
        float x = std::isfinite(sample) ? std::clamp(sample, -8.0f, 8.0f) : 0.0f;
        if (clipActive)
            x = std::clamp(x, -clipThreshold, clipThreshold) / clipThreshold;
        if (rectifyBlend > 0.0f)
            x += (std::abs(x) - x) * rectifyBlend;
        if (noiseGain > 0.0f)
            x += nextNoise(ch) * noiseGain;

        // The optional AA pair is tied to the actual capture frequency.
        // Place the pre-filter after nonlinearity so it can suppress newly
        // generated high harmonics before they fold under sample reduction.
        if (preBlend > 0.0f)
        {
            preOne[ch] += preCoeff * (x - preOne[ch]);
            preTwo[ch] += preCoeff * (preOne[ch] - preTwo[ch]);
            x += (preTwo[ch] - x) * preBlend;
        }
        else
        {
            preOne[ch] = x;
            preTwo[ch] = x;
        }

        phase[ch] += captureIncrement;
        if (phase[ch] >= 1.0f)
        {
            phase[ch] -= 1.0f;
            const float bounded = std::clamp(x, -1.0f, 1.0f);
            const int lowBits = std::clamp(static_cast<int>(bitPosition), 1, 16);
            const int highBits = std::min(lowBits + 1, 16);
            const float blend = bitPosition - static_cast<float>(lowBits);
            const float coarse = quantise(bounded, lowBits);
            const float fine = lowBits == highBits ? coarse : quantise(bounded, highBits);
            held[ch] = coarse + (fine - coarse) * blend;
        }
        float wet = held[ch];
        if (postBlend > 0.0f)
        {
            postOne[ch] += postCoeff * (wet - postOne[ch]);
            postTwo[ch] += postCoeff * (postOne[ch] - postTwo[ch]);
            wet += (postTwo[ch] - wet) * postBlend;
        }
        else
        {
            postOne[ch] = wet;
            postTwo[ch] = wet;
        }
        const float mix = std::clamp(drive, 0.0f, 1.0f);
        const float result = sample + (wet - sample) * mix;
        return std::isfinite(result) ? std::clamp(result, -16.0f, 16.0f) : 0.0f;
    }

private:
    static float quantise(float x, int bits) noexcept
    {
        if (bits == 1) return x > 0.0f ? 1.0f : (x < 0.0f ? -1.0f : 0.0f);
        const float levels = static_cast<float>((1u << static_cast<unsigned int>(bits - 1)) - 1u);
        return std::round(x * levels) / levels;
    }

    float nextNoise(size_t ch) noexcept
    {
        auto& state = noiseStates[ch];
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0x00ffffffu) / 8388607.5f - 1.0f;
    }

    static constexpr size_t tableLength = 1025;
    std::array<float, tableLength> captureRate {}, preCoefficients {}, postCoefficients {};
    std::array<float, 2> held {}, phase {}, preOne {}, preTwo {}, postOne {}, postTwo {};
    std::array<uint32_t, 2> noiseStates {};
    Controls previous { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float captureIncrement = 1.0f, preCoeff = 1.0f, postCoeff = 1.0f;
    float bitPosition = 16.0f, clipThreshold = 1.0f, rectifyBlend = 0.0f;
    float noiseGain = 0.0f, preBlend = 0.0f, postBlend = 0.0f;
    bool clipActive = false;
};
} // namespace crusher::lofi
