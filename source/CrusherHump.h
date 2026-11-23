#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Optional tone-shaping extensions around the *unchanged* HumpCrusher
// Mech -> Filter -> Tube path. At the neutral settings both processing
// functions return their input with no intervening arithmetic.
namespace crusher::humpmodel
{
struct Controls
{
    float body = 0.5f;     // 0..1: pre-Mech bass emphasis/cut
    float bias = 0.5f;     // 0..1: pre-Mech asymmetry
    float fold = 0.0f;     // 0..1: optional post-Tube waveform folding
    float tone = 0.5f;     // 0..1: post-Tube tonal tilt
    float outputDb = 0.0f; // -12..+12 dB, before global EQ post/COMP
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        const float twoPi = 6.2831853071795864769f;
        bodyCoefficient = 1.0f - std::exp(-twoPi * std::min(360.0f, fs * 0.45f) / fs);
        toneCoefficient = 1.0f - std::exp(-twoPi * std::min(2300.0f, fs * 0.45f) / fs);
        dcCoefficient = std::exp(-twoPi * std::min(18.0f, fs * 0.45f) / fs);
        for (size_t i = 0; i < gainTable.size(); ++i)
        {
            const float db = -12.0f + 24.0f * static_cast<float>(i)
                / static_cast<float>(gainTable.size() - 1);
            gainTable[i] = std::pow(10.0f, db * 0.05f);
        }
        reset();
        controlsValid = false;
        setControls(Controls {});
    }

    void reset() noexcept
    {
        bodyState.fill(0.0f);
        toneState.fill(0.0f);
        dcInput.fill(0.0f);
        dcOutput.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (controlsValid && next.body == controls.body
            && next.bias == controls.bias && next.fold == controls.fold
            && next.tone == controls.tone && next.outputDb == controls.outputDb)
            return;
        bodyAmount = (std::clamp(next.body, 0.0f, 1.0f) - 0.5f) * 1.6f;
        biasAmount = (std::clamp(next.bias, 0.0f, 1.0f) - 0.5f) * 1.8f;
        foldAmount = std::clamp(next.fold, 0.0f, 1.0f);
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.6f;
        if (!controlsValid || next.outputDb != controls.outputDb)
        {
            const float position = (std::clamp(next.outputDb, -12.0f, 12.0f) + 12.0f)
                * static_cast<float>(gainTable.size() - 1) / 24.0f;
            const size_t first = static_cast<size_t>(position);
            const size_t second = std::min(first + 1, gainTable.size() - 1);
            outputGain = gainTable[first] + (gainTable[second] - gainTable[first])
                * (position - static_cast<float>(first));
            // Exact bypass gain for the zero dB position.
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
        }
        controls = next;
        controlsValid = true;
    }

    float processPre(float x, int channel) noexcept
    {
        if (bodyAmount == 0.0f && biasAmount == 0.0f)
            return x; // preserves the original Mech input exactly
        const auto c = static_cast<size_t>(std::clamp(channel, 0, 1));
        if (bodyAmount != 0.0f)
        {
            bodyState[c] += bodyCoefficient * (x - bodyState[c]);
            x += bodyAmount * bodyState[c];
        }
        if (biasAmount != 0.0f)
        {
            // Different gain for the positive and negative half cycles;
            // the slow DC remover keeps large offsets out of the filter.
            const float shaped = x * (1.0f + biasAmount * (x >= 0.0f ? 0.65f : -0.65f));
            const float dcFree = shaped - dcInput[c] + dcCoefficient * dcOutput[c];
            dcInput[c] = shaped;
            dcOutput[c] = dcFree;
            x = dcFree;
        }
        return std::isfinite(x) ? std::clamp(x, -24.0f, 24.0f) : 0.0f;
    }

    float processPost(float x, int channel) noexcept
    {
        if (foldAmount == 0.0f && toneAmount == 0.0f && outputGain == 1.0f)
            return x; // preserves the original Tube output exactly
        if (foldAmount > 0.0f)
        {
            // Optional folding; the original Hump shoulder remains intact.
            const float threshold = 1.0f - 0.62f * foldAmount;
            const float excursion = std::max(0.0f, std::abs(x) - threshold);
            if (excursion > 0.0f)
                x -= std::copysign(std::min(excursion * foldAmount * 2.4f, 2.0f), x);
        }
        if (toneAmount != 0.0f)
        {
            const auto c = static_cast<size_t>(std::clamp(channel, 0, 1));
            toneState[c] += toneCoefficient * (x - toneState[c]);
            x += (x - toneState[c]) * toneAmount;
        }
        if (outputGain != 1.0f)
            x *= outputGain;
        return std::isfinite(x) ? std::clamp(x, -24.0f, 24.0f) : 0.0f;
    }

private:
    Controls controls {};
    bool controlsValid = false;
    float bodyAmount = 0.0f;
    float biasAmount = 0.0f;
    float foldAmount = 0.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    float bodyCoefficient = 0.0f;
    float toneCoefficient = 0.0f;
    float dcCoefficient = 0.0f;
    std::array<float, 2> bodyState {};
    std::array<float, 2> toneState {};
    std::array<float, 2> dcInput {};
    std::array<float, 2> dcOutput {};
    std::array<float, 257> gainTable {};
};
} // namespace crusher::humpmodel
