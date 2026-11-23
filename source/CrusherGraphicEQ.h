#pragma once

// Original Crusher seven-band EQ: independent DSP, no Voxengo source or IRs.
// No JUCE dependency or runtime allocations. EQ is off by default.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace crusher::eq
{
constexpr int bandCount = 7;
constexpr float minimumHz = 20.0f, maximumHz = 20000.0f;
constexpr float minimumDb = -18.0f, maximumDb = 18.0f;
constexpr float minimumQ = 0.35f, maximumQ = 8.0f;
inline constexpr std::array<float, bandCount> defaultFrequencies
    {{ 55.0f, 140.0f, 350.0f, 1000.0f, 2700.0f, 7000.0f, 15500.0f }};

enum Shape { bell = 0, lowShelf, highShelf, notch, lowCut, highCut };
enum Mode { pre = 0, emphasis, post };

struct Band
{
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    int shape = bell;
};

struct Coefficients
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float blend = 1.0f;

    double magnitude(double frequency, double sampleRate) const noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        const double angle = -2.0 * pi * frequency / sampleRate;
        const std::complex<double> z1(std::cos(angle), std::sin(angle));
        const auto z2 = z1 * z1;
        const auto numerator = static_cast<double>(b0) + static_cast<double>(b1) * z1
                             + static_cast<double>(b2) * z2;
        const auto denominator = 1.0 + static_cast<double>(a1) * z1
                               + static_cast<double>(a2) * z2;
        const double filtered = std::abs(numerator / denominator);
        return std::max(0.00001, 1.0 - static_cast<double>(blend)
            + static_cast<double>(blend) * filtered);
    }
};

inline Coefficients makeCoefficients(const Band& band, double sampleRate,
                                     bool correction = false) noexcept
{
    constexpr double pi = 3.14159265358979323846;
    const double rate = std::max(8000.0, sampleRate);
    const double frequency = std::clamp(static_cast<double>(band.frequency),
        20.0, std::min(20000.0, rate * 0.45));
    const double gain = std::clamp(static_cast<double>(band.gainDb), -18.0, 18.0);
    const double q = std::clamp(static_cast<double>(band.q), 0.35, 8.0);
    if (std::abs(gain) < 0.00001)
        return {};
    const double w = 2.0 * pi * frequency / rate;
    const double cs = std::cos(w), sn = std::sin(w);
    const double a = std::pow(10.0, (correction ? -gain : gain) / 40.0);
    const double alpha = sn / (2.0 * q);
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;
    double blend = 1.0;
    switch (band.shape)
    {
        case lowShelf:
        case highShelf:
        {
            const double beta = 2.0 * std::sqrt(a) * alpha;
            if (band.shape == lowShelf)
            {
                b0 = a * ((a + 1.0) - (a - 1.0) * cs + beta);
                b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cs);
                b2 = a * ((a + 1.0) - (a - 1.0) * cs - beta);
                a0 = (a + 1.0) + (a - 1.0) * cs + beta;
                a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cs);
                a2 = (a + 1.0) + (a - 1.0) * cs - beta;
            }
            else
            {
                b0 = a * ((a + 1.0) + (a - 1.0) * cs + beta);
                b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cs);
                b2 = a * ((a + 1.0) + (a - 1.0) * cs - beta);
                a0 = (a + 1.0) - (a - 1.0) * cs + beta;
                a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cs);
                a2 = (a + 1.0) - (a - 1.0) * cs - beta;
            }
            break;
        }
        case notch:
        case lowCut:
        case highCut:
        {
            if (correction)
            {
                // Notches and cuts are not mathematically invertible at
                // nulls: use a LIMITED broad bell for stable compensation.
                Band corrective = band;
                corrective.shape = bell;
                corrective.gainDb = -std::copysign(
                    std::min(9.0f, std::abs(band.gainDb) * 0.45f), band.gainDb);
                corrective.q = std::min(1.0f, band.q);
                return makeCoefficients(corrective, rate, false);
            }
            blend = std::min(1.0, std::abs(gain) / 18.0);
            if (band.shape == notch)
            {
                b0 = 1.0; b1 = -2.0 * cs; b2 = 1.0;
                a0 = 1.0 + alpha; a1 = -2.0 * cs; a2 = 1.0 - alpha;
            }
            else if (band.shape == lowCut)
            {
                b0 = (1.0 + cs) * 0.5; b1 = -(1.0 + cs); b2 = b0;
                a0 = 1.0 + alpha; a1 = -2.0 * cs; a2 = 1.0 - alpha;
            }
            else
            {
                b0 = (1.0 - cs) * 0.5; b1 = 1.0 - cs; b2 = b0;
                a0 = 1.0 + alpha; a1 = -2.0 * cs; a2 = 1.0 - alpha;
            }
            break;
        }
        case bell:
        default:
            b0 = 1.0 + alpha * a;
            b1 = -2.0 * cs;
            b2 = 1.0 - alpha * a;
            a0 = 1.0 + alpha / a;
            a1 = -2.0 * cs;
            a2 = 1.0 - alpha / a;
            break;
    }
    const double invA = 1.0 / std::max(0.00001, a0);
    return { static_cast<float>(b0 * invA),
             static_cast<float>(b1 * invA),
             static_cast<float>(b2 * invA),
             static_cast<float>(a1 * invA),
             static_cast<float>(a2 * invA),
             static_cast<float>(blend) };
}

class Biquad
{
public:
    void setTarget(Coefficients value) noexcept { target = value; }
    float process(float x) noexcept
    {
        // Smoothed coefficients prevent abrupt GUI-automation steps.
        constexpr float smoothing = 0.012f;
        current.b0 += (target.b0 - current.b0) * smoothing;
        current.b1 += (target.b1 - current.b1) * smoothing;
        current.b2 += (target.b2 - current.b2) * smoothing;
        current.a1 += (target.a1 - current.a1) * smoothing;
        current.a2 += (target.a2 - current.a2) * smoothing;
        current.blend += (target.blend - current.blend) * smoothing;
        const float y = current.b0 * x + z1;
        z1 = current.b1 * x - current.a1 * y + z2;
        z2 = current.b2 * x - current.a2 * y;
        if (! std::isfinite(y) || ! std::isfinite(z1) || ! std::isfinite(z2))
        {
            reset();
            return 0.0f;
        }
        // Fast decay of denormal states.
        if (std::abs(z1) < 1.e-19f) z1 = 0.0f;
        if (std::abs(z2) < 1.e-19f) z2 = 0.0f;
        return x + (y - x) * current.blend;
    }
    void reset() noexcept { z1 = z2 = 0.0f; current = target; }
private:
    Coefficients current {}, target {};
    float z1 = 0.0f, z2 = 0.0f;
};

class GraphicEQ
{
public:
    void setSampleRate(double rate) noexcept
    {
        sampleRate = std::max(8000.0, rate);
        hasBands = false;
        update(bands);
        reset();
    }
    void update(const std::array<Band, bandCount>& requested) noexcept
    {
        bool changed = ! hasBands;
        if (! changed)
            for (int i = 0; i < bandCount; ++i)
            {
                const auto& a = bands[static_cast<size_t>(i)];
                const auto& b = requested[static_cast<size_t>(i)];
                if (a.frequency != b.frequency || a.gainDb != b.gainDb
                    || a.q != b.q || a.shape != b.shape)
                    changed = true;
            }
        if (! changed)
            return;
        bands = requested;
        hasBands = true;
        for (int i = 0; i < bandCount; ++i)
        {
            const auto front = makeCoefficients(bands[static_cast<size_t>(i)],
                                                 sampleRate, false);
            const auto inverse = makeCoefficients(bands[static_cast<size_t>(i)],
                                                   sampleRate, true);
            for (int ch = 0; ch < 2; ++ch)
            {
                preFilters[static_cast<size_t>(ch)][static_cast<size_t>(i)].setTarget(front);
                postFilters[static_cast<size_t>(ch)][static_cast<size_t>(i)].setTarget(inverse);
            }
        }
    }
    float processPre(float sample, int channel) noexcept
    {
        for (auto& filter : preFilters[static_cast<size_t>(std::clamp(channel, 0, 1))])
            sample = filter.process(sample);
        return sample;
    }
    float processPost(float sample, int channel, bool corrective) noexcept
    {
        if (corrective)
            for (auto& filter : postFilters[static_cast<size_t>(std::clamp(channel, 0, 1))])
                sample = filter.process(sample);
        if (! corrective)
            for (auto& filter : preFilters[static_cast<size_t>(std::clamp(channel, 0, 1))])
                sample = filter.process(sample);
        return sample;
    }
    void reset() noexcept
    {
        for (auto& channel : preFilters)
            for (auto& filter : channel) filter.reset();
        for (auto& channel : postFilters)
            for (auto& filter : channel) filter.reset();
    }
private:
    double sampleRate = 44100.0;
    bool hasBands = false;
    std::array<Band, bandCount> bands {};
    std::array<std::array<Biquad, bandCount>, 2> preFilters {}, postFilters {};
};
} // namespace crusher::eq
