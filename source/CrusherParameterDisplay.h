#pragma once

// Display-only conversions. Parameter ranges and the audio DSP remain unchanged.
// Values are the original 0..100 host parameters, NOT newly ranged parameters.
// This file intentionally has no JUCE dependency so mappings can be tested alone.
#include <algorithm>
#include <cmath>
#include <string_view>

namespace crusher::display
{
enum class Kind
{
    percent, filterCutoff, filterQ, codecMode, codecRate, codecBits,
    loFiRate, loFiBits, diodeInputHP, diodeEnv, diodeOutputHP,
    diodeOutputLP, freshFreq, razorFreq, fuzzTone, crackleDensity
};

constexpr Kind kindFor(std::string_view id) noexcept
{
    using K = Kind;
    return id == "filterCutoff"    ? K::filterCutoff
         : id == "filterResonance" ? K::filterQ
         : id == "codecMode"      ? K::codecMode
         : id == "codecRate"      ? K::codecRate
         : id == "codecBits"      ? K::codecBits
         : id == "loFiRate"       ? K::loFiRate
         : id == "loFiBits"       ? K::loFiBits
         : id == "diodeInputHP"   ? K::diodeInputHP
         : id == "diodeEnvelope"  ? K::diodeEnv
         : id == "diodeOutputHP"  ? K::diodeOutputHP
         : id == "diodeOutputLP"  ? K::diodeOutputLP
         : id == "freshFrequency" ? K::freshFreq
         : id == "razorFreq"      ? K::razorFreq
         : id == "fuzzTone"       ? K::fuzzTone
         : id == "crackleDensity" ? K::crackleDensity
         : K::percent;
}

constexpr bool isFrequency(Kind k) noexcept
{
    using K = Kind;
    return k == K::filterCutoff || k == K::codecRate || k == K::loFiRate
        || k == K::diodeInputHP || k == K::diodeOutputHP
        || k == K::diodeOutputLP || k == K::freshFreq
        || k == K::razorFreq || k == K::fuzzTone;
}

inline double toPhysical(Kind kind, double percent, double sampleRate) noexcept
{
    const double p = std::clamp(percent * 0.01, 0.0, 1.0);
    const double sr = std::max(1000.0, sampleRate);
    using K = Kind;
    switch (kind)
    {
        case K::filterCutoff:
        {
            // Exact original Hump Filter's 0..1 -> Hz transfer function.
            if (p <= 0.0) return 80.0;
            if (p >= 1.0) return 16000.0;
            const double shaped = -0.25 * std::log10(1.0 - p);
            return std::clamp(shaped * 15920.0 + 80.0, 80.0, 16000.0);
        }
        case K::filterQ:       return 1.0 + 6.0 * p;
        case K::codecMode:     return static_cast<double>(std::min(4, static_cast<int>(5.0 * p)));
        case K::codecRate:     return sr * std::pow(800.0 / sr, p);
        case K::codecBits:     return static_cast<double>(std::clamp(
                                        static_cast<int>(std::lround(2.0 + 10.0 * p)), 2, 12));
        case K::loFiRate:      return sr * std::pow(180.0 / sr, p);
        case K::loFiBits:      return 1.0 + 15.0 * p;
        case K::diodeInputHP:  return 35.0 * std::pow(1000.0 / 35.0, p);
        case K::diodeEnv:      return 1.5 + 65.0 * p;
        case K::diodeOutputHP: return 30.0 * std::pow(4000.0 / 30.0, p);
        case K::diodeOutputLP: return 1200.0 * std::pow(16000.0 / 1200.0, p);
        case K::freshFreq:     return p <= 0.5 ? 500.0 + 5800.0 * p
                                            : 3400.0 + 19200.0 * (p - 0.5);
        case K::razorFreq:     return p <= 0.5 ? 350.0 + 3500.0 * p
                                            : 2100.0 + 16800.0 * (p - 0.5);
        case K::fuzzTone:      return p <= 0.5 ? 800.0 + 8800.0 * p
                                            : 5200.0 + 15600.0 * (p - 0.5);
        case K::crackleDensity:return 0.20 + 1.60 * p;
        default:               return percent;
    }
}

inline double toPercent(Kind kind, double physical, double sampleRate) noexcept
{
    if (!std::isfinite(physical)) return 0.0;
    const double sr = std::max(1000.0, sampleRate);
    const auto logPosition = [physical](double low, double high)
    {
        return std::log(std::max(physical, 0.001) / low) / std::log(high / low);
    };
    using K = Kind;
    double p = 0.0;
    switch (kind)
    {
        case K::filterCutoff:
            p = physical <= 80.0 ? 0.0 : physical >= 16000.0 ? 1.0
                : 1.0 - std::pow(10.0, -4.0 * (physical - 80.0) / 15920.0);
            break;
        case K::filterQ:       p = (physical - 1.0) / 6.0; break;
        case K::codecMode:     p = (std::clamp(std::lround(physical), 0L, 4L) + 0.5) / 5.0; break;
        case K::codecRate:     p = logPosition(sr, 800.0); break;
        case K::codecBits:     p = (physical - 2.0) / 10.0; break;
        case K::loFiRate:      p = logPosition(sr, 180.0); break;
        case K::loFiBits:      p = (physical - 1.0) / 15.0; break;
        case K::diodeInputHP:  p = logPosition(35.0, 1000.0); break;
        case K::diodeEnv:      p = (physical - 1.5) / 65.0; break;
        case K::diodeOutputHP: p = logPosition(30.0, 4000.0); break;
        case K::diodeOutputLP: p = logPosition(1200.0, 16000.0); break;
        case K::freshFreq:     p = physical <= 3400.0 ? (physical - 500.0) / 5800.0
                                                : 0.5 + (physical - 3400.0) / 19200.0; break;
        case K::razorFreq:     p = physical <= 2100.0 ? (physical - 350.0) / 3500.0
                                                : 0.5 + (physical - 2100.0) / 16800.0; break;
        case K::fuzzTone:      p = physical <= 5200.0 ? (physical - 800.0) / 8800.0
                                                : 0.5 + (physical - 5200.0) / 15600.0; break;
        case K::crackleDensity:p = (physical - 0.20) / 1.60; break;
        default:               return physical;
    }
    return 100.0 * std::clamp(p, 0.0, 1.0);
}
} // namespace crusher::display
