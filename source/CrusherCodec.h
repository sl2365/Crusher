#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Original lightweight codec-inspired degradation. This is not an MP3, AAC,
// GSM or ToneBoosters implementation. The spectral mode uses independently
// quantised adaptive frequency bands rather than a proprietary transform codec.
// No allocation, logging or expensive transcendentals occur on the audio thread.
namespace crusher::codec
{
struct Controls
{
    float mode = 0.90f;   // PCM, mu-law, A-law, predictive, spectral (five regions)
    float rate = 0.35f;   // host-rate capture to ~800 Hz
    float bits = 0.50f;   // 2 to 12 encoded bits
    float damage = 0.50f; // frame faults, noise fill and spectral masking
    float tone = 0.50f;   // dark to bright; centre flat
};

class Processor
{
public:
    void prepare(double sampleRateHz) noexcept
    {
        const float sr = static_cast<float>(std::max(1000.0, sampleRateHz));
        constexpr float pi2 = 6.2831853071795864769f;
        for (size_t i = 0; i < tableSize; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(tableSize - 1);
            const float captureHz = sr * std::pow(800.0f / sr, t);
            rateTable[i] = std::clamp(captureHz / sr, 800.0f / sr, 1.0f);
            // Three cumulative lowpasses split approximately at 350, 1800 and
            // 5200 Hz, scaled down when the capture Nyquist falls below them.
            for (size_t band = 0; band < 3; ++band)
            {
                const float desired = band == 0 ? 350.0f
                    : (band == 1 ? 1800.0f : 5200.0f);
                const float cutoff = std::min(desired, captureHz * 0.43f);
                spectralCoefficients[band][i] = 1.0f - std::exp(
                    -pi2 * cutoff / std::max(captureHz, 100.0f));
            }
            const float darkCut = 950.0f + 11000.0f * t * t;
            darkCoefficients[i] = 1.0f - std::exp(
                -pi2 * std::min(darkCut, sr * 0.45f) / sr);
        }

        // Precompute companding curves so moving BITS, RATE and CODEC during
        // playback does not call logarithms/exponentials for audio samples.
        constexpr float mu = 255.0f;
        constexpr float a = 87.6f;
        const float muLog = std::log1p(mu);
        const float aLog = 1.0f + std::log(a);
        for (size_t i = 0; i < curveSize; ++i)
        {
            const float x = static_cast<float>(i) / static_cast<float>(curveSize - 1);
            muEncode[i] = std::log1p(mu * x) / muLog;
            muDecode[i] = std::expm1(muLog * x) / mu;
            aEncode[i] = x < 1.0f / a ? a * x / aLog
                        : (1.0f + std::log(a * x)) / aLog;
            aDecode[i] = x < 1.0f / aLog ? x * aLog / a
                        : std::exp(x * aLog - 1.0f) / a;
        }
        // Follow the input rather than generating continuous idle static.
        // 8 ms peak release is sample-rate independent; calculated only in prepare().
        noiseRelease = std::exp(-1.0f / (0.008f * sr));
        previous = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
        setControls(Controls {});
        reset();
    }

    void reset() noexcept
    {
        phase.fill(1.0f);
        held.fill(0.0f);
        predictor.fill(0.0f);
        toneLow.fill(0.0f);
        inputEnvelope.fill(0.0f);
        for (auto& channel : spectralLow) channel.fill(0.0f);
        for (auto& channel : spectralEnvelope) channel.fill(0.0f);
        for (auto& channel : spectralMask) channel.fill(1.0f);
        faultCounter.fill(0);
        spectralCounter.fill(0);
        faultyFrame.fill(false);
        noiseStates = { 0x5e22d179u, 0x9fa481b3u };
    }

    void setControls(const Controls& values) noexcept
    {
        if (values.mode == previous.mode && values.rate == previous.rate
            && values.bits == previous.bits && values.damage == previous.damage
            && values.tone == previous.tone)
            return;
        mode = std::min(4, static_cast<int>(5.0f * clamp01(values.mode)));
        captureIncrement = interpolate(rateTable, clamp01(values.rate));
        for (size_t i = 0; i < 3; ++i)
            spectralCoeff[i] = interpolate(spectralCoefficients[i], clamp01(values.rate));
        bits = std::clamp(static_cast<int>(std::lround(2.0f + 10.0f
            * clamp01(values.bits))), 2, 12);
        damage = clamp01(values.damage);
        const float tone = clamp01(values.tone);
        // At 50% the tone stage is an arithmetic no-op.
        darkMix = std::max(0.0f, (0.50f - tone) * 2.0f);
        brightMix = std::max(0.0f, (tone - 0.50f) * 2.0f) * 0.85f;
        toneCoefficient = tone <= 0.5f
            ? interpolate(darkCoefficients, tone * 2.0f)
            : interpolate(darkCoefficients, 0.60f);
        previous = values;
    }

    float process(float input, float drive, int channel) noexcept
    {
        const size_t ch = static_cast<size_t>(std::clamp(channel, 0, 1));
        if (drive <= 0.0f)
        {
            inputEnvelope[ch] = 0.0f;
            return input;
        }
        const float x = std::isfinite(input) ? std::clamp(input, -8.0f, 8.0f) : 0.0f;
        // Preserve v1.43.0's DAMAGE grain at ordinary playback levels.
        // As the input decays, silence both synthetic noise sources smoothly.
        auto& envelope = inputEnvelope[ch];
        envelope = std::max(std::abs(x), envelope * noiseRelease);
        if (envelope < 0.000025f) envelope = 0.0f;
        const float noiseActivity = std::clamp((envelope - 0.000025f)
            * 5000.0f, 0.0f, 1.0f);
        phase[ch] += captureIncrement;
        if (phase[ch] >= 1.0f)
        {
            phase[ch] -= 1.0f;
            const float source = std::clamp(x, -1.0f, 1.0f);
            const float step = std::ldexp(1.0f, 1 - bits); // 2/(2^bits)
            // Deterministic frame errors and spectral noise fill; state is
            // independent for each channel and advanced only while selected.
            if (++faultCounter[ch] >= 48)
            {
                faultCounter[ch] = 0;
                faultyFrame[ch] = 0.5f * (random(ch) + 1.0f)
                    < damage * damage * 0.24f;
            }
            float decoded = source;
            switch (mode)
            {
                case 0: decoded = quantise(source, step); break;
                case 1: decoded = compand(source, step, muEncode, muDecode); break;
                case 2: decoded = compand(source, step, aEncode, aDecode); break;
                case 3:
                {
                    const float estimate = predictor[ch] * 0.86f;
                    const float residual = std::clamp(source - estimate, -1.0f, 1.0f);
                    decoded = std::clamp(estimate + quantise(residual, step), -1.0f, 1.0f);
                    predictor[ch] = decoded;
                    break;
                }
                default: decoded = spectralProcess(source, step, ch, noiseActivity); break;
            }
            if (damage > 0.0f)
            {
                // Add unstable low-level codec grain that follows bit depth.
                const float noise = random(ch) * (step * 0.18f * damage
                    + 0.018f * damage * damage) * noiseActivity;
                if (faultyFrame[ch])
                    decoded = 0.64f * held[ch] + 0.36f * decoded + noise;
                else
                    decoded += noise;
            }
            held[ch] = std::clamp(decoded, -1.5f, 1.5f);
        }
        float wet = held[ch];
        toneLow[ch] += toneCoefficient * (wet - toneLow[ch]);
        if (darkMix > 0.0f)
            wet += (toneLow[ch] - wet) * darkMix;
        if (brightMix > 0.0f)
            wet += (wet - toneLow[ch]) * brightMix;
        const float result = input + (wet - input) * clamp01(drive);
        return std::isfinite(result) ? std::clamp(result, -16.0f, 16.0f) : 0.0f;
    }

private:
    static float clamp01(float x) noexcept
    { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; }

    template <size_t N>
    static float interpolate(const std::array<float, N>& values, float value) noexcept
    {
        const float index = value * static_cast<float>(N - 1);
        const size_t a = static_cast<size_t>(index);
        const size_t b = std::min(a + 1, N - 1);
        return values[a] + (values[b] - values[a]) * (index - static_cast<float>(a));
    }

    static float quantise(float x, float step) noexcept
    { return std::clamp(std::round(x / step) * step, -1.0f, 1.0f); }

    float compand(float x, float step, const std::array<float, 2049>& encoder,
                  const std::array<float, 2049>& decoder) const noexcept
    {
        const float encoded = std::copysign(interpolate(encoder, std::abs(x)), x);
        const float clipped = quantise(encoded, step);
        return std::copysign(interpolate(decoder, std::abs(clipped)), clipped);
    }

    float spectralProcess(float source, float step, size_t ch,
                          float noiseActivity) noexcept
    {
        auto& lp = spectralLow[ch];
        lp[0] += spectralCoeff[0] * (source - lp[0]);
        lp[1] += spectralCoeff[1] * (source - lp[1]);
        lp[2] += spectralCoeff[2] * (source - lp[2]);
        const float bands[4] = { lp[0], lp[1] - lp[0], lp[2] - lp[1],
                                 source - lp[2] };
        auto& env = spectralEnvelope[ch];
        auto& mask = spectralMask[ch];
        const bool updateMask = ++spectralCounter[ch] >= 32;
        if (updateMask) spectralCounter[ch] = 0;
        float result = 0.0f;
        for (size_t band = 0; band < 4; ++band)
        {
            env[band] += 0.06f * (std::abs(bands[band]) - env[band]);
            if (updateMask)
            {
                const float threshold = damage * damage
                    * (band == 0 ? 0.018f : 0.035f + 0.021f * static_cast<float>(band));
                // 32-capture-sample frames preserve the character of
                // spectral holes and modulating noise floors.
                mask[band] = (env[band] < threshold)
                    ? (0.04f + 0.14f * (random(ch) + 1.0f)) : 1.0f;
            }
            const float bandStep = step * (band == 0 ? 0.70f
                : 1.0f + 0.55f * static_cast<float>(band));
            float quantised = quantise(bands[band], bandStep);
            if (mask[band] < 1.0f)
                quantised = quantised * mask[band]
                    + random(ch) * step * damage * (1.0f - mask[band])
                        * noiseActivity;
            result += quantised;
        }
        return std::clamp(result, -1.5f, 1.5f);
    }

    float random(size_t ch) noexcept
    {
        auto& state = noiseStates[ch];
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0x00ffffffu) / 8388607.5f - 1.0f;
    }

    static constexpr size_t tableSize = 257;
    static constexpr size_t curveSize = 2049;
    std::array<float, tableSize> rateTable {}, darkCoefficients {};
    std::array<std::array<float, tableSize>, 3> spectralCoefficients {};
    std::array<float, curveSize> muEncode {}, muDecode {}, aEncode {}, aDecode {};
    std::array<float, 3> spectralCoeff {};
    std::array<float, 2> phase {}, held {}, predictor {}, toneLow {}, inputEnvelope {};
    std::array<std::array<float, 3>, 2> spectralLow {};
    std::array<std::array<float, 4>, 2> spectralEnvelope {}, spectralMask {};
    std::array<int, 2> faultCounter {}, spectralCounter {};
    std::array<bool, 2> faultyFrame {};
    std::array<uint32_t, 2> noiseStates {};
    Controls previous { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float captureIncrement = 1.0f;
    float damage = 0.0f, darkMix = 0.0f, brightMix = 0.0f;
    float toneCoefficient = 1.0f, noiseRelease = 0.0f;
    int mode = 0, bits = 12;
};
} // namespace crusher::codec
