#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Bumpy distortion DSP, independent of JUCE so its neutral identity and
// new waveshaping controls can be regression-tested outside the VST3.
// Only called by the active Bumpy model (or during its outgoing crossfade).
namespace crusher::bumpy
{
struct Controls
{
    float ripple = 0.5f;    // 0..1; 0.5 reproduces original ripple level
    float rate = 0.5f;      // 0..1; ripple harmonics, neutral at 0.5
    float contour = 0.5f;   // 0..1; pre-emphasis, neutral at 0.5
    float bias = 0.5f;      // 0..1; positive/negative symmetry, neutral 0.5
    float tone = 0.5f;      // 0..1; dark/bright, neutral at 0.5
    float outputDb = 0.0f;  // -12..+12 dB output trim
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        constexpr float twoPi = 6.2831853071795864769f;
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        dcCoefficient = std::exp(-twoPi * 22.0f / fs);
        preCoefficient = std::exp(-twoPi * 1450.0f / fs);
        postCoefficient = std::exp(-twoPi * 2600.0f / fs);
        reset();
    }

    void reset() noexcept
    {
        preState.fill(0.0f);
        previousInput.fill(0.0f);
        previousOutput.fill(0.0f);
        postState.fill(0.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (next.ripple == controls.ripple && next.rate == controls.rate
            && next.contour == controls.contour && next.bias == controls.bias
            && next.tone == controls.tone && next.outputDb == controls.outputDb)
            return;
        if (next.outputDb != controls.outputDb)
            outputGain = std::pow(10.0f,
                std::clamp(next.outputDb, -12.0f, 12.0f) / 20.0f);
        controls = next;
        rippleScale = 2.0f * std::clamp(next.ripple, 0.0f, 1.0f);
        rateScale = 0.4f + 1.2f * std::clamp(next.rate, 0.0f, 1.0f);
        contourScale = 2.0f * std::clamp(next.contour, 0.0f, 1.0f);
        biasAmount = 2.0f * (std::clamp(next.bias, 0.0f, 1.0f) - 0.5f);
        toneAmount = 2.0f * (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f);
    }

    float process(float sample, float amount, int channel) noexcept
    {
        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float a = std::clamp(amount, 0.0f, 1.0f);
        // At zero Drive, only the Output trim remains operational.
        if (a <= 0.0f)
            return sample * outputGain;

        preState[index] = (1.0f - preCoefficient) * sample
                        + preCoefficient * preState[index];
        const float mid = sample - preState[index];
        const float emphasised = sample + a * contourScale
            * (0.72f * mid - 0.10f * preState[index]);
        const float driven = emphasised * (1.0f + a * 12.0f);
        const float base = std::tanh(driven);
        const float ripple = std::sin(driven
            * (2.7f + a * 2.3f) * rateScale
            + 0.35f * std::sin(driven * 1.7f));
        const float envelope = a * 0.16f * rippleScale
            * (1.0f - std::exp(-std::abs(driven) * 0.80f));
        const float asymmetric = base + ripple * envelope
            + a * 0.055f * base * base
            + a * biasAmount * 0.28f * (base * base);
        const float dcBlocked = asymmetric - previousInput[index]
            + dcCoefficient * previousOutput[index];
        previousInput[index] = asymmetric;
        previousOutput[index] = std::isfinite(dcBlocked)
            ? std::clamp(dcBlocked, -4.0f, 4.0f) : 0.0f;
        const float compensation = 0.92f / (1.0f + a * 1.05f);
        float shaped = previousOutput[index] * compensation;
        // Cheap first-order post filter; at centre, the audio is not modified.
        postState[index] = (1.0f - postCoefficient) * shaped
                         + postCoefficient * postState[index];
        if (toneAmount < 0.0f)
            shaped += (shaped - postState[index]) * toneAmount;
        else if (toneAmount > 0.0f)
            shaped += (shaped - postState[index]) * (0.85f * toneAmount);
        const float result = (sample + (shaped - sample) * a) * outputGain;
        return std::isfinite(result) ? std::clamp(result, -8.0f, 8.0f) : 0.0f;
    }

private:
    Controls controls {};
    float rippleScale = 1.0f;
    float rateScale = 1.0f;
    float contourScale = 1.0f;
    float biasAmount = 0.0f;
    float toneAmount = 0.0f;
    float outputGain = 1.0f;
    float dcCoefficient = 0.0f;
    float preCoefficient = 0.0f;
    float postCoefficient = 0.0f;
    std::array<float, 2> preState {};
    std::array<float, 2> previousInput {};
    std::array<float, 2> previousOutput {};
    std::array<float, 2> postState {};
};
}
