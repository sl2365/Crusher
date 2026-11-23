#pragma once

// Crusher's independently developed Amp tone/cabinet model.
// Uses filter-derived cabinet responses; no third-party IRs or binaries.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace crusher::amp
{
struct Controls
{
    int cabinet = 0;              // 0 = direct (no cabinet coloration)
    float softClip = 0.0f;        // 0..1
    float topBoost = 0.0f;        // 0..1
    float bias = 0.0f;            // 0..1
    float outputDb = 0.0f;       // -12..12
};

class AmpSim final
{
public:
    static constexpr int cabinetCount = 16;

    void setSampleRate(double sampleRate) noexcept
    {
        rate = std::max(8000.0, sampleRate);
        updateCoefficients();
        reset();
    }

    void setControls(Controls next) noexcept
    {
        next.cabinet = std::clamp(next.cabinet, 0, cabinetCount - 1);
        next.softClip = std::clamp(next.softClip, 0.0f, 1.0f);
        next.topBoost = std::clamp(next.topBoost, 0.0f, 1.0f);
        next.bias = std::clamp(next.bias, 0.0f, 1.0f);
        next.outputDb = std::clamp(next.outputDb, -12.0f, 12.0f);
        const bool cabinetChanged = controls.cabinet != next.cabinet;
        const bool outputChanged = controls.outputDb != next.outputDb;
        controls = next;
        // Avoid exp() recalculation during ordinary audio blocks or while
        // sequencing other Amp controls. No sound/parameter change.
        if (outputChanged)
            outputGain = std::pow(10.0f, controls.outputDb / 20.0f);
        if (cabinetChanged)
            updateCabinetCoefficients();
    }


    // TOP BOOST is a stronger pre-emphasis ahead of the Amp drive. Its
    // complementary post-drive presence shelf is in post(); both stages are
    // exactly neutral at zero. BIAS also shapes the preamp asymmetrically.
    float pre(float input, int channel) noexcept
    {
        auto& s = states[index(channel)];
        s.topLow += (1.0f - topPole) * (input - s.topLow);
        const float high = input - s.topLow;
        float y = input + high * controls.topBoost * 4.0f;
        if (controls.bias > 0.0f)
        {
            const float offset = controls.bias * 0.90f;
            const float atZero = std::tanh(offset);
            const float zeroSlope = std::max(0.25f, 1.0f - atZero * atZero);
            const float asymmetric = (std::tanh(y + offset) - atZero)
                                   / zeroSlope;
            y += controls.bias * 0.95f * (asymmetric - y);
        }
        return y;
    }

    // Post-gain shaping, tone stack and independently modelled cabinet EQ.
    float post(float input, int channel) noexcept
    {
        auto& s = states[index(channel)];
        float y = input;
        if (controls.softClip > 0.0f)
        {
            constexpr float normaliser = 1.0f / 0.9640275800758f;
            const float softened = std::tanh(y * 2.0f) * normaliser;
            y += (softened - y) * controls.softClip;
        }

        // Stronger asymmetric post-drive saturation makes BIAS change the
        // clipping character even after the main Amp gain stages have
        // compressed away most of the preamp's bias difference.
        // The subtraction is zero at silence; a gentle DC blocker removes
        // the nonzero mean that asymmetric distortion creates on AC input.
        if (controls.bias > 0.0f)
        {
            const float offset = controls.bias * 1.30f;
            const float atZero = std::tanh(offset);
            const float positive = std::tanh(2.0f + offset) - atZero;
            const float negative = atZero - std::tanh(offset - 2.0f);
            const float scale = std::max(0.5f, 0.5f * (positive + negative));
            const float asymmetric = (std::tanh(2.0f * y + offset) - atZero)
                                   / scale;
            y += controls.bias * 0.85f * (asymmetric - y);
        }
        const float biasDcFiltered = y - s.biasPreviousInput
                                   + biasDcPole * s.biasPreviousOutput;
        s.biasPreviousInput = y;
        s.biasPreviousOutput = biasDcFiltered;
        y += controls.bias * (biasDcFiltered - y);

        // The shared PRE/EMPH/POST EQ replaces Amp's old tone stack.
        // TOP BOOST retains its independent post-drive presence stage.
        s.trebleLow += (1.0f - treblePole) * (y - s.trebleLow);
        const float treblePart = y - s.trebleLow;
        y += controls.topBoost * 2.40f * treblePart;

        // D.I. deliberately omits cabinet coloration.
        if (controls.cabinet != 0)
        {
            s.cabLow += (1.0f - cabLowPole) * (y - s.cabLow);
            float filtered = y - s.cabLow; // speaker LF roll-off
            s.cabHigh1 += (1.0f - cabHighPole) * (filtered - s.cabHigh1);
            s.cabHigh2 += (1.0f - cabHighPole) * (s.cabHigh1 - s.cabHigh2);
            filtered = s.cabHigh2; // 12dB/oct HF roll-off
            s.bodyLow += (1.0f - bodyLowPole) * (filtered - s.bodyLow);
            s.bodyHigh += (1.0f - bodyHighPole) * (filtered - s.bodyHigh);
            const float cabinetBody = s.bodyHigh - s.bodyLow;
            y = filtered * cabVolume + cabinetBody * cabBodyGain;
        }
        y *= outputGain;
        // Leave final limiting to Crusher's existing output stage.
        return std::isfinite(y) ? y : 0.0f;
    }

    void reset() noexcept { states = {}; }

private:
    struct Cabinet
    {
        float lowCut, highCut, bodyCentre, bodyGain, level;
    };
    // Not impulse-response captures; these are original broad tonal sketches.
    static constexpr std::array<Cabinet, cabinetCount> cabinets {{
        {  10, 18000,  800, 0.00f, 1.00f }, // D.I.
        { 105,  5700, 1350, 0.24f, 0.96f }, // Brit
        { 170,  4400, 1150, 0.35f, 0.91f }, // 1x8
        {  90,  5400,  930, 0.22f, 0.96f }, // 1x12
        { 125,  6400, 1100, 0.26f, 0.97f }, // 2x10
        {  85,  6500,  990, 0.22f, 1.00f }, // 2x12
        { 110,  7200, 1050, 0.21f, 1.00f }, // 4x10
        {  85,  6250,  900, 0.29f, 0.98f }, // 4x12
        {  46,  4100,  560, 0.31f, 1.00f }, // 1x15 Bass
        {  50,  5650,  680, 0.21f, 1.00f }, // 4x10 Bass
        { 340,  2850, 1250, 0.34f, 0.89f }, // Radio
        { 130,  4800,  870, 0.31f, 0.94f }, // 1x10 Vintage
        {  36,  3600,  480, 0.35f, 1.00f }, // 2x15 Bass
        {  47,  6600,  760, 0.19f, 1.00f }, // 8x10 Bass
        {  90,  7700, 1800, 0.23f, 0.98f }, // 4x12 Modern
        { 275,  3200,  990, 0.42f, 0.88f }  // 1x6 Portable
    }};
    struct State
    {
        float topLow = 0.0f;
        float biasPreviousInput = 0.0f, biasPreviousOutput = 0.0f;
        float trebleLow = 0.0f;
        float cabLow = 0.0f, cabHigh1 = 0.0f, cabHigh2 = 0.0f;
        float bodyLow = 0.0f, bodyHigh = 0.0f;
    };
    static std::size_t index(int channel) noexcept
    {
        return static_cast<std::size_t>(std::clamp(channel, 0, 1));
    }
    float pole(float frequency) const noexcept
    {
        constexpr float pi2 = 6.283185307179586f;
        return std::exp(-pi2 * std::clamp(frequency, 8.0f,
                     static_cast<float>(rate * 0.45)) / static_cast<float>(rate));
    }
    void updateCoefficients() noexcept
    {
        topPole = pole(2800.0f);
        biasDcPole = pole(18.0f);
        treblePole = pole(3200.0f);
        updateCabinetCoefficients();
    }
    void updateCabinetCoefficients() noexcept
    {
        const auto& cab = cabinets[static_cast<std::size_t>(controls.cabinet)];
        cabLowPole = pole(cab.lowCut);
        cabHighPole = pole(cab.highCut);
        bodyLowPole = pole(cab.bodyCentre * 0.69f);
        bodyHighPole = pole(cab.bodyCentre * 1.45f);
        cabBodyGain = cab.bodyGain;
        cabVolume = cab.level;
    }

    Controls controls;
    double rate = 44100.0;
    std::array<State, 2> states {};
    float topPole = 0.0f, biasDcPole = 0.0f, treblePole = 0.0f;
    float cabLowPole = 0.0f, cabHighPole = 0.0f,
          bodyLowPole = 0.0f, bodyHighPole = 0.0f;
    float cabBodyGain = 0.0f, cabVolume = 1.0f;
    float outputGain = 1.0f;
};
} // namespace crusher::amp
