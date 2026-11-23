#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Configurable hard clip: at the neutral settings its arithmetic reproduces
// the original Crusher Hard Clip model. Only the selected/outgoing model runs.
namespace crusher::hardclip
{
struct Controls
{
    float threshold = 0.5f;  // 0..1; centre = original +/-1 clipping rail
    float bias = 0.5f;       // 0..1; centre = original symmetric clipping
    float knee = 0.5f;       // 0..1; centre = original sharp, flat-top clip
    float fold = 0.0f;       // 0..1; zero = original, no folding
    float tone = 0.5f;       // 0..1; centre = unchanged spectral response
    float outputDb = 0.0f;   // -12..+12 dB after the DRIVE blend
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        constexpr float twoPi = 6.2831853071795864769f;
        toneCoefficient = std::exp(-twoPi * std::min(3200.0f, fs * 0.45f) / fs);
        for (size_t i = 0; i < gainTable.size(); ++i)
        {
            const float db = -12.0f + 24.0f * static_cast<float>(i)
                / static_cast<float>(gainTable.size() - 1);
            gainTable[i] = std::pow(10.0f, db * 0.05f);
        }
        reset();
        controlsValid = false;
        setControls(controls);
    }

    void reset() noexcept { toneState.fill(0.0f); }

    void setControls(const Controls& next) noexcept
    {
        if (controlsValid && next.threshold == controls.threshold
            && next.bias == controls.bias && next.knee == controls.knee
            && next.fold == controls.fold && next.tone == controls.tone
            && next.outputDb == controls.outputDb)
            return;

        // All parameter-dependent work is cached; only controls belonging to
        // an active Hard Clip engine arrive here at audio-sample rate.
        if (!controlsValid || next.outputDb != controls.outputDb)
        {
            const float position = (std::clamp(next.outputDb, -12.0f, 12.0f)
                + 12.0f) * static_cast<float>(gainTable.size() - 1) / 24.0f;
            const size_t first = static_cast<size_t>(position);
            const size_t second = std::min(first + 1, gainTable.size() - 1);
            outputGain = gainTable[first] + (gainTable[second] - gainTable[first])
                       * (position - static_cast<float>(first));
        }
        const float th = std::clamp(next.threshold, 0.0f, 1.0f);
        threshold = th <= 0.5f ? 0.30f + 1.40f * th
                               : 1.0f + (th - 0.5f) * 1.60f;
        biasOffset = (std::clamp(next.bias, 0.0f, 1.0f) - 0.5f) * 0.80f;
        const float kneePosition = std::clamp(next.knee, 0.0f, 1.0f) - 0.5f;
        smoothKnee = std::max(0.0f, -2.0f * kneePosition);
        squareKnee = std::max(0.0f, 2.0f * kneePosition);
        foldAmount = std::clamp(next.fold, 0.0f, 1.0f);
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 2.0f;
        controls = next;
        controlsValid = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const float a = std::clamp(amount, 0.0f, 1.0f);
        if (a <= 0.0f)
            return std::isfinite(sample) ? sample * outputGain : 0.0f;

        const float driven = sample * (1.0f + a * 19.0f);
        float clipped = std::clamp(driven + biasOffset, -threshold, threshold)
                      - std::clamp(biasOffset, -threshold, threshold);

        if (foldAmount > 0.0f)
        {
            const float excursion = std::max(0.0f, std::abs(driven + biasOffset)
                                                 - threshold);
            const float folding = std::min(2.0f * threshold,
                                          excursion * foldAmount * 0.50f);
            clipped -= std::copysign(folding, driven + biasOffset);
        }
        if (smoothKnee > 0.0f)
        {
            // Soft shoulder preserves the exact hard-clip branch at 50%.
            const float softened = threshold * std::tanh((driven + biasOffset)
                / std::max(0.01f, threshold));
            const float softenedZero = threshold * std::tanh(biasOffset
                / std::max(0.01f, threshold));
            clipped += (softened - softenedZero - clipped) * smoothKnee;
        }
        if (squareKnee > 0.0f)
        {
            const float limited = std::clamp(clipped / threshold, -1.0f, 1.0f);
            const float squared = std::copysign(std::sqrt(std::abs(limited)), limited)
                                * threshold;
            clipped += (squared - clipped) * squareKnee;
        }

        const float outputCompensation = 1.0f / (1.0f + a * 0.80f);
        const float shaped = clipped * outputCompensation;
        float result = sample + (shaped - sample) * a;
        if (toneAmount != 0.0f)
        {
            const size_t index = static_cast<size_t>(std::clamp(channel, 0, 1));
            toneState[index] = (1.0f - toneCoefficient) * result
                             + toneCoefficient * toneState[index];
            result += (result - toneState[index]) *
                (toneAmount < 0.0f ? toneAmount : toneAmount * 0.85f);
        }
        result *= outputGain;
        return std::isfinite(result) ? std::clamp(result, -32.0f, 32.0f) : 0.0f;
    }

private:
    Controls controls {};
    bool controlsValid = false;
    float threshold = 1.0f;
    float biasOffset = 0.0f;
    float smoothKnee = 0.0f;
    float squareKnee = 0.0f;
    float foldAmount = 0.0f;
    float toneAmount = 0.0f;
    float toneCoefficient = 0.0f;
    float outputGain = 1.0f;
    std::array<float, 257> gainTable {};
    std::array<float, 2> toneState {};
};
} // namespace crusher::hardclip
