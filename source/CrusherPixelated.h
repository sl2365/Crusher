#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Sample-hold and quantisation Pixelated model. All 50% settings and
// OUTPUT = 0 dB retain the exact original bitcrushing / noise arithmetic.
// Run only while this model is selected or outgoing in the model crossfade.
namespace crusher::pixelated
{
struct Controls
{
    float rate = 0.5f;       // Sample-hold rate relative to original DRIVE curve
    float bits = 0.5f;       // Coarser below 50%, smoother/cleaner above; original at 50%
    float dither = 0.5f;     // Original noise amount at 50%
    float jitter = 0.5f;     // Optional sample-hold timing instability
    float tone = 0.5f;       // Optional post-crush tone tilt
    float outputDb = 0.0f;   // -12 .. +12 dB
};

class Processor
{
public:
    void prepare(double sampleRate) noexcept
    {
        const float fs = static_cast<float>(std::max(1.0, sampleRate));
        constexpr float twoPi = 6.2831853071795864769f;
        toneCoefficient = 1.0f - std::exp(-twoPi * std::min(2000.0f, fs * 0.45f) / fs);
        // DRIVE response is calculated once per sample rate, not with
        // square roots on the real-time audio thread.
        for (size_t i = 0; i < driveCurve.size(); ++i)
        {
            const float fraction = static_cast<float>(i)
                / static_cast<float>(driveCurve.size() - 1);
            driveCurve[i] = std::sqrt(std::sqrt(fraction));
        }
        for (size_t i = 0; i < gainTable.size(); ++i)
        {
            const float db = -12.0f + static_cast<float>(i) * 24.0f
                / static_cast<float>(gainTable.size() - 1);
            gainTable[i] = std::pow(10.0f, db / 20.0f);
        }
        validControls = false;
        setControls(Controls {});
        reset();
    }

    void reset() noexcept
    {
        heldSample.fill(0.0f);
        phase.fill(1.0f); // Original reset starts with an immediate capture.
        toneState.fill(0.0f);
        noiseState = { 0x94d049bbu, 0x369dea0fu };
        jitterState = { 0xb5297a4du, 0x68e31da4u };
        jitterHoldScale.fill(1.0f);
    }

    void setControls(const Controls& next) noexcept
    {
        if (validControls && next.rate == controls.rate
            && next.bits == controls.bits && next.dither == controls.dither
            && next.jitter == controls.jitter && next.tone == controls.tone
            && next.outputDb == controls.outputDb)
            return;

        // Centre positions preserve the original DSP response exactly.
        // BITS moves continuously between quantisation strengths instead of
        // adding an integer offset that can mute quiet inputs at 2 bits.
        rateScale = 0.35f + 1.30f * std::clamp(next.rate, 0.0f, 1.0f);
        bitsPosition = std::clamp(next.bits, 0.0f, 1.0f);
        ditherScale = 0.25f + 1.5f * std::clamp(next.dither, 0.0f, 1.0f);
        // Midpoint is exactly the original sample-and-hold timing. Away from
        // centre, vary whole capture intervals rather than adding tiny
        // independent rate fluctuations on every sample.
        jitterAmount = (std::clamp(next.jitter, 0.0f, 1.0f) - 0.5f) * 2.0f;
        jitterDepth = std::abs(jitterAmount);
        if (jitterAmount == 0.0f)
            jitterHoldScale.fill(1.0f);
        toneAmount = (std::clamp(next.tone, 0.0f, 1.0f) - 0.5f) * 1.6f;
        if (! validControls || next.outputDb != controls.outputDb)
        {
            const float pos = (std::clamp(next.outputDb, -12.0f, 12.0f) + 12.0f)
                * static_cast<float>(gainTable.size() - 1) / 24.0f;
            const size_t a = static_cast<size_t>(pos);
            const size_t b = std::min(a + 1, gainTable.size() - 1);
            outputGain = gainTable[a] + (gainTable[b] - gainTable[a])
                * (pos - static_cast<float>(a));
            if (next.outputDb == 0.0f)
                outputGain = 1.0f;
        }
        controls = next;
        validControls = true;
    }

    float process(float sample, float amount, int channel) noexcept
    {
        if (amount <= 0.0f)
            return outputGain == 1.0f ? sample : sample * outputGain;
        const auto index = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float safeAmount = std::clamp(amount, 0.0f, 1.0f);
        // The former quadratic hold-rate and linear bit-depth response made
        // DRIVE barely audible until roughly its last third. Curve the
        // distortion intensity, while retaining exact 0% and 100% endpoints.
        const float curvePosition = safeAmount
            * static_cast<float>(driveCurve.size() - 1);
        const size_t curveIndex = static_cast<size_t>(curvePosition);
        const size_t nextCurveIndex = std::min(curveIndex + 1, driveCurve.size() - 1);
        const float shapedAmount = driveCurve[curveIndex]
            + (driveCurve[nextCurveIndex] - driveCurve[curveIndex])
                * (curvePosition - static_cast<float>(curveIndex));
        const float originalRate = 1.0f - 0.94f * shapedAmount * shapedAmount;
        float holdRate = originalRate;
        if (rateScale != 1.0f)
            holdRate = std::clamp(originalRate * rateScale, 0.002f, 1.0f);
        if (jitterAmount != 0.0f)
            holdRate = std::clamp(holdRate * jitterHoldScale[index], 0.002f, 1.0f);
        phase[index] += holdRate;
        if (phase[index] >= 1.0f)
        {
            phase[index] -= 1.0f;
            heldSample[index] = sample;
            if (jitterAmount != 0.0f)
            {
                // Draw once per new hold interval, not once per sample.
                // Keep this RNG separate from quantisation dither so 50%
                // JITTER still produces exactly the original sound.
                const float random = nextNoise(jitterState[index]);
                if (jitterAmount > 0.0f)
                {
                    // Right of centre: pronounced irregular flutter with
                    // both fast captures and much longer sample holds.
                    const float spread = random * random * jitterDepth;
                    jitterHoldScale[index] = random < 0.0f
                        ? 1.0f / (1.0f + 20.0f * spread)
                        : 1.0f + 4.0f * spread;
                }
                else
                {
                    // Left of centre: predominantly extended holds for
                    // pronounced stuttering without muting the signal.
                    const float slow = 0.5f * (1.0f - random);
                    jitterHoldScale[index] = 1.0f
                        / (1.0f + 20.0f * jitterDepth * slow * slow);
                }
            }
        }

        const int originalBits = std::clamp(
            static_cast<int>(std::lround(16.0f - shapedAmount * 12.0f)), 4, 16);
        // Draw noise once per sample, just as the original quantiser does.
        // This also keeps the noise sequence identical at BITS = 50%.
        const float randomValue = nextNoise(noiseState[index]);
        const float held = heldSample[index];
        float quantised = held;
        if (bitsPosition < 0.5f)
        {
            // Smoothly morph between a practical three-bit crush and the
            // original DRIVE-dependent word length. Interpolating adjacent
            // quantisers eliminates wide stretches of identical BITS values.
            const float travel = bitsPosition * 2.0f;
            const float effectiveBits = 3.0f
                + static_cast<float>(originalBits - 3) * travel;
            const int lowerBits = std::clamp(static_cast<int>(effectiveBits), 3, originalBits);
            const int upperBits = std::min(lowerBits + 1, originalBits);
            const float fraction = effectiveBits - static_cast<float>(lowerBits);
            const auto quantise = [held, randomValue, safeAmount, this](int depth) noexcept
            {
                const float steps = static_cast<float>(
                    (1u << static_cast<unsigned int>(depth - 1)) - 1u);
                const float noise = randomValue * safeAmount * 0.35f / steps * ditherScale;
                return std::round((held + noise) * steps) / steps;
            };
            const float coarse = quantise(lowerBits);
            const float fine = lowerBits == upperBits ? coarse : quantise(upperBits);
            quantised = coarse + (fine - coarse) * fraction;

            // Keep a small amount of the held waveform at very low BITS.
            // Sparse/quiet material must not disappear into quantiser zero.
            const float signalProtection = 0.35f * (1.0f - travel);
            quantised += (held - quantised) * signalProtection;
        }
        else
        {
            // The centre is the original expression, bit-for-bit. Above
            // centre, progressively remove quantisation error rather than
            // raising bit depth beyond audibility.
            const float levels = static_cast<float>(
                (1u << static_cast<unsigned int>(originalBits - 1)) - 1u);
            const float dither = randomValue
                * safeAmount * 0.35f / levels * ditherScale;
            quantised = std::round((held + dither) * levels) / levels;
            if (bitsPosition > 0.5f)
                quantised += (held - quantised) * ((bitsPosition - 0.5f) * 2.0f);
        }
        const float output = sanitise(quantised / (1.0f + safeAmount * 0.08f), 2.0f);
        // Bring the crushed signal into the blend earlier, without changing
        // the dry 0% or full-wet 100% DRIVE endpoints.
        const float wetAmount = safeAmount * (1.65f - 0.65f * safeAmount);
        float result = sample + (output - sample) * wetAmount;
        if (toneAmount != 0.0f)
        {
            toneState[index] += toneCoefficient * (result - toneState[index]);
            result += (result - toneState[index]) * toneAmount;
        }
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
    static float sanitise(float sample, float limit) noexcept
    {
        return std::isfinite(sample) ? std::clamp(sample, -limit, limit) : 0.0f;
    }

    Controls controls {};
    bool validControls = false;
    float rateScale = 1.0f;
    float bitsPosition = 0.5f;
    float ditherScale = 1.0f;
    float jitterAmount = 0.0f, jitterDepth = 0.0f;
    float toneAmount = 0.0f, outputGain = 1.0f;
    std::array<float, 2> jitterHoldScale { 1.0f, 1.0f };
    float toneCoefficient = 0.0f;
    std::array<float, 1025> driveCurve {};
    std::array<float, 2> heldSample {};
    std::array<float, 2> phase {};
    std::array<float, 2> toneState {};
    std::array<uint32_t, 2> noiseState { 0x94d049bbu, 0x369dea0fu };
    std::array<uint32_t, 2> jitterState { 0xb5297a4du, 0x68e31da4u };
    std::array<float, 257> gainTable {};
};
} // namespace crusher::pixelated
