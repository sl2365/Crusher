#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Original frequency-selective distortion aimed at vocals, percussion and drums.
// The BODY path preserves bass energy, the mid/presence bands are saturated,
// and SMOOTH dynamically attenuates the harsh upper band. No generated noise,
// look-ahead, delay allocation or host-thread memory allocation.
namespace crusher::vocal
{
struct Controls
{
    float character = 0.50f; // warm -> gritty -> clipped
    float body = 0.50f;      // low-frequency weight
    float presence = 0.50f;  // intelligibility / attack band
    float bite = 0.50f;      // distorted upper-band edge
    float smooth = 0.50f;    // adaptive high-frequency control
};

class Processor
{
public:
    void prepare(double sampleRateHz) noexcept
    {
        const float sr = static_cast<float>(std::max(8000.0, sampleRateHz));
        const auto coefficient = [sr](float hz) noexcept
        {
            constexpr float twopi = 6.2831853071795864769f;
            return 1.0f - std::exp(-twopi * std::min(hz, sr * 0.43f) / sr);
        };
        midCoeff = coefficient(1450.0f);
        // Lookup tables keep audio-rate automation inexpensive. At the 50%
        // defaults, every crossover remains at its v1.43.3 value.
        for (size_t i = 0; i < bassCoeffTable.size(); ++i)
        {
            const float position = static_cast<float>(i) / static_cast<float>(bassCoeffTable.size() - 1);
            bassCoeffTable[i] = coefficient(220.0f + 380.0f * std::abs(2.0f * position - 1.0f));
            const float biteHz = position <= 0.5f
                ? 8500.0f + (5100.0f - 8500.0f) * (position * 2.0f)
                : 5100.0f + (2900.0f - 5100.0f) * ((position - 0.5f) * 2.0f);
            presenceCoeffTable[i] = coefficient(biteHz);
            const float smoothHz = position <= 0.5f
                ? 18000.0f + (14500.0f - 18000.0f) * (position * 2.0f)
                : 14500.0f + (2700.0f - 14500.0f) * ((position - 0.5f) * 2.0f);
            postCoeffTable[i] = coefficient(smoothHz);
        }
        bassCoeff = bassCoeffTable[128];
        presenceCoeff = presenceCoeffTable[128];
        postCoeff = postCoeffTable[128];
        deEssAttack = std::exp(-1.0f / (0.002f * sr));
        deEssRelease = std::exp(-1.0f / (0.055f * sr));
        // Wider BODY range; 50% remains exactly unity gain.
        // No pow(), exp() or memory allocation is needed in process().
        for (size_t i = 0; i < bodyGainTable.size(); ++i)
        {
            const float position = static_cast<float>(i) / static_cast<float>(bodyGainTable.size() - 1);
            bodyGainTable[i] = std::pow(10.0f, (position - 0.5f) * 30.0f / 20.0f);
        }
        // The post filter removes a little ultrasonic/near-Nyquist energy
        // rather than imposing a resonant or delayed signal path.
        previous = Controls { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
        setControls(Controls {});
        reset();
    }

    void reset() noexcept
    {
        low.fill(0.0f);
        mid.fill(0.0f);
        highMid.fill(0.0f);
        postLow.fill(0.0f);
        harshEnvelope.fill(0.0f);
    }

    void setControls(const Controls& values) noexcept
    {
        if (values.character == previous.character && values.body == previous.body
            && values.presence == previous.presence && values.bite == previous.bite
            && values.smooth == previous.smooth)
            return;
        character = clamp01(values.character);
        // BODY: -15..+15 dB with a wider low-mid crossover away from 50%.
        const float tableIndex = clamp01(values.body)
            * static_cast<float>(bodyGainTable.size() - 1);
        const size_t floorIndex = static_cast<size_t>(tableIndex);
        const size_t upperIndex = std::min(floorIndex + 1, bodyGainTable.size() - 1);
        bodyGain = bodyGainTable[floorIndex] + (bodyGainTable[upperIndex]
                   - bodyGainTable[floorIndex])
                   * (tableIndex - static_cast<float>(floorIndex));
        bassCoeff = interpolate(bassCoeffTable, clamp01(values.body));
        bias = 0.035f + 0.13f * character;
        biasReference = shape(bias, character);
        presenceGain = 0.40f + 2.25f * clamp01(values.presence);
        // A post-saturation presence shelf stays effective even when the
        // nonlinear midrange transfer function is already saturated.
        presenceTilt = (clamp01(values.presence) - 0.5f) * 2.8f;
        const float bite = clamp01(values.bite);
        biteGain = 0.25f + 3.25f * bite;
        presenceCoeff = interpolate(presenceCoeffTable, bite);
        // At 50% this is the original 70% clean / 30% shaped upper band.
        biteEdgeBlend = 0.30f + (bite - 0.5f) * (bite < 0.5f ? 0.50f : 1.30f);
        biteCleanBlend = 0.70f - (bite - 0.5f) * (bite < 0.5f ? 0.50f : 1.30f);
        smoothing = clamp01(values.smooth);
        postCoeff = interpolate(postCoeffTable, smoothing);
        if (smoothing <= 0.5f)
            postBlend = 0.08f + 0.24f * smoothing * 2.0f;
        else
            postBlend = 0.32f + 0.60f * (smoothing - 0.5f) * 2.0f;
        dryPostBlend = 1.0f - postBlend;
        previous = values;
    }

    float process(float input, float drive, int channel) noexcept
    {
        const float wetAmount = clamp01(drive);
        if (wetAmount <= 0.0f) return input;
        const size_t ch = static_cast<size_t>(std::clamp(channel, 0, 1));
        const float x = std::isfinite(input) ? std::clamp(input, -8.0f, 8.0f) : 0.0f;

        // A set of cumulative one-pole splits: sum of the bands equals x,
        // including at transients. Each stereo channel keeps separate state.
        const float bass = advance(low[ch], bassCoeff, x);
        const float belowMid = advance(mid[ch], midCoeff, x);
        const float belowHigh = advance(highMid[ch], presenceCoeff, x);
        const float presenceBand = belowHigh - belowMid;
        const float core = (belowMid - bass) + presenceBand * presenceGain;
        const float air = x - belowHigh;

        const float driveGain = 1.0f + 13.0f * wetAmount;
        // The asymmetrical bias yields even harmonics but is subtracted at
        // zero input so there is no DC or sound generated during silence.
        const float midShaped = (shape(core * driveGain + bias, character)
                                - biasReference)
                               / (1.0f + 1.15f * wetAmount);

        // Preserve a portion of the unprocessed attack band. A second,
        // deliberately smaller nonlinear path supplies the adjustable BITE.
        const float edge = shape(air * biteGain * (1.0f + 5.0f * wetAmount),
                                 std::min(1.0f, 0.45f + character * 0.55f));
        float& env = harshEnvelope[ch];
        const float target = std::abs(air) + 0.35f * std::abs(belowHigh - belowMid);
        const float envelopeCoefficient = target > env ? deEssAttack : deEssRelease;
        env = target + envelopeCoefficient * (env - target);
        // For high SMOOTH settings, a signal-relative detector works on quiet
        // vocals as well as loud drum transients. At 50% the old behaviour is
        // unchanged; below 50% the sound opens up instead of adding treble.
        const float baseHarshness = std::clamp(env * 3.5f, 0.0f, 1.0f);
        float harshness = baseHarshness;
        if (smoothing > 0.5f)
        {
            const float dynamicHarshness = env / (env + 0.018f);
            const float weight = (smoothing - 0.5f) * 2.0f;
            harshness += (dynamicHarshness - harshness) * weight;
        }
        const float deEssGain = 1.0f / (1.0f + smoothing * 4.5f * harshness);
        const float top = (air * biteCleanBlend + edge * biteEdgeBlend) * deEssGain;
        const float shaped = bass * bodyGain + midShaped + top + presenceBand * presenceTilt;
        const float filtered = advance(postLow[ch], postCoeff, shaped);
        const float wet = filtered * postBlend + shaped * dryPostBlend;
        const float result = x + (wet - x) * wetAmount;
        return std::isfinite(result) ? std::clamp(result, -16.0f, 16.0f) : 0.0f;
    }

private:
    static float interpolate(const std::array<float, 257>& table, float value) noexcept
    {
        const float pos = clamp01(value) * static_cast<float>(table.size() - 1);
        const size_t lo = static_cast<size_t>(pos);
        const size_t hi = std::min(lo + 1, table.size() - 1);
        return table[lo] + (table[hi] - table[lo]) * (pos - static_cast<float>(lo));
    }

    static float clamp01(float x) noexcept
    { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; }

    static float advance(float& state, float coeff, float value) noexcept
    {
        state += coeff * (value - state);
        if (std::abs(state) < 1.0e-15f) state = 0.0f;
        return state;
    }

    static float tanhApprox(float x) noexcept
    {
        x = std::clamp(x, -3.0f, 3.0f);
        const float square = x * x;
        return x * (27.0f + square) / (27.0f + 9.0f * square);
    }

    static float shape(float x, float character) noexcept
    {
        // Continuously interpolate three genuinely different transfer curves.
        // Warm is soft and open; grit adds dense harmonics; the final range
        // has a hard, clipped edge useful on drums and aggressive vocals.
        const float warm = x / (1.0f + 0.72f * std::abs(x));
        if (character < 0.5f)
        {
            const float grit = tanhApprox(x * 1.35f);
            return warm + (grit - warm) * (character * 2.0f);
        }
        const float grit = tanhApprox(x * 1.35f);
        const float clip = std::clamp(x * 1.5f, -0.85f, 0.85f);
        return grit + (clip - grit) * ((character - 0.5f) * 2.0f);
    }

    Controls previous {};
    float character = 0.50f;
    float bodyGain = 1.0f;
    float bias = 0.10f;
    float biasReference = 0.0f;
    std::array<float, 257> bodyGainTable {};
    std::array<float, 257> bassCoeffTable {};
    std::array<float, 257> presenceCoeffTable {};
    std::array<float, 257> postCoeffTable {};
    float presenceGain = 1.525f;
    float presenceTilt = 0.0f;
    float biteGain = 1.875f;
    float biteEdgeBlend = 0.30f;
    float biteCleanBlend = 0.70f;
    float smoothing = 0.50f;
    float postBlend = 0.32f;
    float dryPostBlend = 0.68f;
    float bassCoeff = 0.03f;
    float midCoeff = 0.18f;
    float presenceCoeff = 0.45f;
    float postCoeff = 0.70f;
    float deEssAttack = 0.99f;
    float deEssRelease = 0.999f;
    std::array<float, 2> low {};
    std::array<float, 2> mid {};
    std::array<float, 2> highMid {};
    std::array<float, 2> postLow {};
    std::array<float, 2> harshEnvelope {};
};
}
