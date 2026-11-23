#pragma once

#include "HumpDSP/Filter.h"
#include <algorithm>
#include <cmath>

namespace crusher::filtermodes
{
// Alternative shared FILTER shapes. The original hump::Filter is still used
// unchanged for Low-pass (shape 0); this engine only handles shapes 1 and 2.
class Processor
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate = std::max(8000.0, rate);
        reset();
    }

    void reset() noexcept
    {
        for (auto& state : channels) state = {};
        previousShape = -1;
    }

    void process(float* left, float* right, int samples,
                 float cutoff, float resonance, int shape) noexcept
    {
        if (shape != 1 && shape != 2 || samples <= 0) return;
        if (shape != previousShape)
        {
            reset();
            previousShape = shape;
        }
        const double frequency = std::clamp(static_cast<double>(
            hump::Filter::cutoffToHz(cutoff)), 25.0, sampleRate * 0.45);
        const double q = std::clamp(static_cast<double>(
            hump::Filter::resonanceToQ(resonance)), 0.5, 7.0);
        const double phase = 6.283185307179586 * frequency / sampleRate;
        const double cosine = std::cos(phase);
        const double alpha = std::sin(phase) / (2.0 * q);
        const double denom = 1.0 + alpha;
        const double a1 = -2.0 * cosine / denom;
        const double a2 = (1.0 - alpha) / denom;
        const double b0 = shape == 1 ? alpha / denom
            : (1.0 + cosine) * 0.5 / denom;
        const double b1 = shape == 1 ? 0.0
            : -(1.0 + cosine) / denom;
        const double b2 = shape == 1 ? -alpha / denom : b0;
        for (auto& channel : channels)
        {
            channel.b0 = b0; channel.b1 = b1; channel.b2 = b2;
            channel.a1 = a1; channel.a2 = a2;
        }
        for (int i = 0; i < samples; ++i)
        {
            left[i] = channels[0].tick(left[i]);
            right[i] = channels[1].tick(right[i]);
        }
    }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float tick(float input) noexcept
        {
            const double x = std::isfinite(input) ? input : 0.0;
            const double result = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
            x2 = x1; x1 = x; y2 = y1;
            y1 = std::isfinite(result) ? std::clamp(result, -32.0, 32.0) : 0.0;
            return static_cast<float>(y1);
        }
    };
    Biquad channels[2] {};
    double sampleRate = 44100.0;
    int previousShape = -1;
};
} // namespace crusher::filtermodes
