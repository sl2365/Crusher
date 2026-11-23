#pragma once

#include <JuceHeader.h>
#include <array>
#include <algorithm>
#include <cmath>

namespace crusher::sequencer
{
constexpr int stepCount = 32;
constexpr int targetSlotCount = 16;
constexpr int rateChoiceCount = 14;

enum class Direction
{
    loop = 0,
    bounce,
    reverse,
    played,
    random
};

enum class GateStepMode
{
    off = 0,
    shortStep,
    longStep,
    rest
};

enum class Waveform
{
    saw = 0,
    sawDouble,
    sawDown,
    sawDownDouble,
    sine,
    sineDouble,
    triangle,
    triangleDouble,
    pulse25,
    pulse25Double,
    square,
    squareDouble,
    pulse75,
    pulse75Double
};

struct TargetInfo
{
    const char* parameterId;
    const char* displayName;
};

inline constexpr std::array<TargetInfo, 139> targets
{{
    { "distOutput", "Dist Output" },
    { "mix", "Dry / Wet" },
    { "output", "Output" },
    { "drive", "Dist" },
    { "sequencerAttack", "Seq Attack" },
    { "sequencerRelease", "Seq Release" },
    { "sequencerPeak", "Seq Depth" },
    { "sequencerMove", "Seq Offset" },
    { "sequencerStart", "Seq Start" },
    { "sequencerEnd", "Seq End" },
    { "sequencerGateShortLength", "Gate Short" },
    { "sequencerGateLongLength", "Gate Long" },
    { "mech", "Mech" },
    { "tube", "Tube" },
    { "filterCutoff", "Cutoff" },
    { "filterResonance", "Resonance" },
    { "compAmount", "Comp Amount" },
    { "ampSoftClip", "Amp Soft Clip" },
    { "ampTopBoost", "Amp Top Boost" },
    { "ampBias", "Amp Bias" },
    { "asymBias", "Asym Bias" },
    { "asymSkew", "Asym Skew" },
    { "asymShape", "Asym Shape" },
    { "asymTone", "Asym Tone" },
    { "bumpyRipple", "Bumpy Ripple" },
    { "bumpyRate", "Bumpy Rate" },
    { "bumpyContour", "Bumpy Contour" },
    { "bumpyBias", "Bumpy Bias" },
    { "bumpyTone", "Bumpy Tone" },
    { "combTime", "Comb Time" },
    { "combFeedback", "Comb Feedback" },
    { "combDamp", "Comb Damp" },
    { "combSpread", "Comb Spread" },
    { "combTone", "Comb Tone" },
    { "crackleDensity", "Crackle Density" },
    { "crackleBurst", "Crackle Burst" },
    { "crackleDecay", "Crackle Decay" },
    { "crackleGrit", "Crackle Grit" },
    { "crackleTone", "Crackle Tone" },
    { "freshFrequency", "Fresh Freq" },
    { "freshExcite", "Fresh Excite" },
    { "freshAir", "Fresh Air" },
    { "freshSaturation", "Fresh Sat" },
    { "freshTone", "Fresh Tone" },
    { "fuzzGate", "Fuzz Gate" },
    { "fuzzBias", "Fuzz Bias" },
    { "fuzzClip", "Fuzz Clip" },
    { "fuzzBody", "Fuzz Body" },
    { "fuzzTone", "Fuzz Tone" },
    { "hardClipThreshold", "Hard Clip Thresh" },
    { "hardClipBias", "Hard Clip Bias" },
    { "hardClipKnee", "Hard Clip Knee" },
    { "hardClipFold", "Hard Clip Fold" },
    { "hardClipTone", "Hard Clip Tone" },
    { "humpBody", "Hump Body" },
    { "humpBias", "Hump Bias" },
    { "humpFold", "Hump Fold" },
    { "humpTone", "Hump Tone" },
    { "lightningSpark", "Lightning Spark" },
    { "lightningReact", "Lightning React" },
    { "lightningClip", "Lightning Clip" },
    { "lightningEdge", "Lightning Edge" },
    { "lightningTone", "Lightning Tone" },
    { "pixelatedRate", "Pixelated Rate" },
    { "pixelatedBits", "Pixelated Bits" },
    { "pixelatedDither", "Pixelated Dither" },
    { "pixelatedJitter", "Pixelated Jitter" },
    { "pixelatedTone", "Pixelated Tone" },
    { "razorEdge", "Razor Edge" },
    { "razorFreq", "Razor Freq" },
    { "razorRing", "Razor Ring" },
    { "razorBite", "Razor Bite" },
    { "razorTone", "Razor Tone" },
    { "rectifyWave", "Rectify Wave" },
    { "rectifyBias", "Rectify Bias" },
    { "rectifyCurve", "Rectify Curve" },
    { "rectifyFold", "Rectify Fold" },
    { "rectifyTone", "Rectify Tone" },
    { "samuraiEdge", "Samurai Edge" },
    { "samuraiGate", "Samurai Gate" },
    { "samuraiBlade", "Samurai Blade" },
    { "samuraiRing", "Samurai Ring" },
    { "samuraiTone", "Samurai Tone" },
    { "saturationCurve", "Saturation Curve" },
    { "saturationBias", "Saturation Bias" },
    { "saturationWarm", "Saturation Warm" },
    { "saturationBody", "Saturation Body" },
    { "saturationTone", "Saturation Tone" },
    { "softClipThreshold", "Soft Clip Thresh" },
    { "softClipKnee", "Soft Clip Knee" },
    { "softClipBias", "Soft Clip Bias" },
    { "softClipBody", "Soft Clip Body" },
    { "softClipTone", "Soft Clip Tone" },
    { "stressedPress", "Stressed Press" },
    { "stressedFold", "Stressed Fold" },
    { "stressedTension", "Stressed Tension" },
    { "stressedBias", "Stressed Bias" },
    { "stressedTone", "Stressed Tone" },
    { "tubeBias", "Tube Bias" },
    { "tubeHeat", "Tube Heat" },
    { "tubeSag", "Tube Sag" },
    { "tubeBody", "Tube Body" },
    { "tubeTone", "Tube Tone" },
    { "diodeInputHP", "Diode In HP" },
    { "diodeEnvelope", "Diode Env" },
    { "diodeShape", "Diode Shape" },
    { "diodeOutputHP", "Diode Out HP" },
    { "diodeOutputLP", "Diode Out LP" },
    // EQ graph: vertical movement adjusts gain; horizontal movement adjusts frequency.
    { "eq1Gain", "EQ 1 Gain" },
    { "eq1Freq", "EQ 1 Freq" },
    { "eq2Gain", "EQ 2 Gain" },
    { "eq2Freq", "EQ 2 Freq" },
    { "eq3Gain", "EQ 3 Gain" },
    { "eq3Freq", "EQ 3 Freq" },
    { "eq4Gain", "EQ 4 Gain" },
    { "eq4Freq", "EQ 4 Freq" },
    { "eq5Gain", "EQ 5 Gain" },
    { "eq5Freq", "EQ 5 Freq" },
    { "eq6Gain", "EQ 6 Gain" },
    { "eq6Freq", "EQ 6 Freq" },
    { "eq7Gain", "EQ 7 Gain" },
    { "eq7Freq", "EQ 7 Freq" },
    { "loFiRate", "Lo-Fi Rate" },
    { "loFiBits", "Lo-Fi Bits" },
    { "loFiClip", "Lo-Fi Clip" },
    { "loFiRectify", "Lo-Fi Rectify" },
    { "loFiNoise", "Lo-Fi Noise" },
    { "loFiPreAA", "Lo-Fi Pre AA" },
    { "loFiPostAA", "Lo-Fi Post AA" },
    { "codecMode", "Codec Mode" },
    { "codecRate", "Codec Rate" },
    { "codecBits", "Codec Bits" },
    { "codecDamage", "Codec Damage" },
    { "codecTone", "Codec Tone" },
    { "vocalCharacter", "Vocal Character" },
    { "vocalBody", "Vocal Body" },
    { "vocalPresence", "Vocal Presence" },
    { "vocalBite", "Vocal Bite" },
    { "vocalSmooth", "Vocal Smooth" },
}};

inline constexpr int distOutputTargetChoice = 1;
inline constexpr int mixTargetChoice = 2;
inline constexpr int outputTargetChoice = 3;
inline constexpr int driveTargetChoice = 4;
inline constexpr int sequencerAttackTargetChoice = 5;
inline constexpr int sequencerReleaseTargetChoice = 6;
inline constexpr int sequencerDepthTargetChoice = 7;
inline constexpr int sequencerOffsetTargetChoice = 8;
inline constexpr int sequencerStartTargetChoice = 9;
inline constexpr int sequencerEndTargetChoice = 10;
inline constexpr int sequencerGateShortTargetChoice = 11;
inline constexpr int sequencerGateLongTargetChoice = 12;
inline constexpr int mechTargetChoice = 13;
inline constexpr int tubeTargetChoice = 14;
inline constexpr int filterCutoffTargetChoice = 15;
inline constexpr int filterResonanceTargetChoice = 16;
inline constexpr int compAmountTargetChoice = 17;
inline constexpr int ampSoftClipTargetChoice = 18;
inline constexpr int ampTopBoostTargetChoice = 19;
inline constexpr int ampBiasTargetChoice = 20;
inline constexpr int asymBiasTargetChoice = 21;
inline constexpr int asymSkewTargetChoice = 22;
inline constexpr int asymShapeTargetChoice = 23;
inline constexpr int asymToneTargetChoice = 24;
inline constexpr int bumpyRippleTargetChoice = 25;
inline constexpr int bumpyRateTargetChoice = 26;
inline constexpr int bumpyContourTargetChoice = 27;
inline constexpr int bumpyBiasTargetChoice = 28;
inline constexpr int bumpyToneTargetChoice = 29;
inline constexpr int combTimeTargetChoice = 30;
inline constexpr int combFeedbackTargetChoice = 31;
inline constexpr int combDampTargetChoice = 32;
inline constexpr int combSpreadTargetChoice = 33;
inline constexpr int combToneTargetChoice = 34;
inline constexpr int crackleDensityTargetChoice = 35;
inline constexpr int crackleBurstTargetChoice = 36;
inline constexpr int crackleDecayTargetChoice = 37;
inline constexpr int crackleGritTargetChoice = 38;
inline constexpr int crackleToneTargetChoice = 39;
inline constexpr int freshFrequencyTargetChoice = 40;
inline constexpr int freshExciteTargetChoice = 41;
inline constexpr int freshAirTargetChoice = 42;
inline constexpr int freshSaturationTargetChoice = 43;
inline constexpr int freshToneTargetChoice = 44;
inline constexpr int fuzzGateTargetChoice = 45;
inline constexpr int fuzzBiasTargetChoice = 46;
inline constexpr int fuzzClipTargetChoice = 47;
inline constexpr int fuzzBodyTargetChoice = 48;
inline constexpr int fuzzToneTargetChoice = 49;
inline constexpr int hardClipThresholdTargetChoice = 50;
inline constexpr int hardClipBiasTargetChoice = 51;
inline constexpr int hardClipKneeTargetChoice = 52;
inline constexpr int hardClipFoldTargetChoice = 53;
inline constexpr int hardClipToneTargetChoice = 54;
inline constexpr int humpBodyTargetChoice = 55;
inline constexpr int humpBiasTargetChoice = 56;
inline constexpr int humpFoldTargetChoice = 57;
inline constexpr int humpToneTargetChoice = 58;
inline constexpr int lightningSparkTargetChoice = 59;
inline constexpr int lightningReactTargetChoice = 60;
inline constexpr int lightningClipTargetChoice = 61;
inline constexpr int lightningEdgeTargetChoice = 62;
inline constexpr int lightningToneTargetChoice = 63;
inline constexpr int pixelatedRateTargetChoice = 64;
inline constexpr int pixelatedBitsTargetChoice = 65;
inline constexpr int pixelatedDitherTargetChoice = 66;
inline constexpr int pixelatedJitterTargetChoice = 67;
inline constexpr int pixelatedToneTargetChoice = 68;
inline constexpr int razorEdgeTargetChoice = 69;
inline constexpr int razorFreqTargetChoice = 70;
inline constexpr int razorRingTargetChoice = 71;
inline constexpr int razorBiteTargetChoice = 72;
inline constexpr int razorToneTargetChoice = 73;
inline constexpr int rectifyWaveTargetChoice = 74;
inline constexpr int rectifyBiasTargetChoice = 75;
inline constexpr int rectifyCurveTargetChoice = 76;
inline constexpr int rectifyFoldTargetChoice = 77;
inline constexpr int rectifyToneTargetChoice = 78;
inline constexpr int samuraiEdgeTargetChoice = 79;
inline constexpr int samuraiGateTargetChoice = 80;
inline constexpr int samuraiBladeTargetChoice = 81;
inline constexpr int samuraiRingTargetChoice = 82;
inline constexpr int samuraiToneTargetChoice = 83;
inline constexpr int saturationCurveTargetChoice = 84;
inline constexpr int saturationBiasTargetChoice = 85;
inline constexpr int saturationWarmTargetChoice = 86;
inline constexpr int saturationBodyTargetChoice = 87;
inline constexpr int saturationToneTargetChoice = 88;
inline constexpr int softClipThresholdTargetChoice = 89;
inline constexpr int softClipKneeTargetChoice = 90;
inline constexpr int softClipBiasTargetChoice = 91;
inline constexpr int softClipBodyTargetChoice = 92;
inline constexpr int softClipToneTargetChoice = 93;
inline constexpr int stressedPressTargetChoice = 94;
inline constexpr int stressedFoldTargetChoice = 95;
inline constexpr int stressedTensionTargetChoice = 96;
inline constexpr int stressedBiasTargetChoice = 97;
inline constexpr int stressedToneTargetChoice = 98;
inline constexpr int tubeBiasTargetChoice = 99;
inline constexpr int tubeHeatTargetChoice = 100;
inline constexpr int tubeSagTargetChoice = 101;
inline constexpr int tubeBodyTargetChoice = 102;
inline constexpr int tubeToneTargetChoice = 103;
inline constexpr int diodeInputHPTargetChoice = 104;
inline constexpr int diodeEnvelopeTargetChoice = 105;
inline constexpr int diodeShapeTargetChoice = 106;
inline constexpr int diodeOutputHPTargetChoice = 107;
inline constexpr int diodeOutputLPTargetChoice = 108;
// Two sequencer targets for each of the seven EQ graph points.
inline constexpr int eqTargetFirstChoice = 109;
inline constexpr int eqVerticalTargetChoice(int band) noexcept
{
    return eqTargetFirstChoice + band * 2;
}
inline constexpr int eqHorizontalTargetChoice(int band) noexcept
{
    return eqTargetFirstChoice + band * 2 + 1;
}
static_assert(eqHorizontalTargetChoice(6) == 122,
              "Existing EQ target IDs must stay at 109-122");
inline constexpr int loFiRateTargetChoice = 123;
inline constexpr int loFiBitsTargetChoice = 124;
inline constexpr int loFiClipTargetChoice = 125;
inline constexpr int loFiRectifyTargetChoice = 126;
inline constexpr int loFiNoiseTargetChoice = 127;
inline constexpr int loFiPreAATargetChoice = 128;
inline constexpr int loFiPostAATargetChoice = 129;
static_assert(loFiPostAATargetChoice == 129,
              "Existing Lo-Fi target IDs must not change");
inline constexpr int codecModeTargetChoice = 130;
inline constexpr int codecRateTargetChoice = 131;
inline constexpr int codecBitsTargetChoice = 132;
inline constexpr int codecDamageTargetChoice = 133;
inline constexpr int codecToneTargetChoice = 134;
static_assert(codecToneTargetChoice == 134,
              "Existing Codec target IDs must not change");
inline constexpr int vocalCharacterTargetChoice = 135;
inline constexpr int vocalBodyTargetChoice = 136;
inline constexpr int vocalPresenceTargetChoice = 137;
inline constexpr int vocalBiteTargetChoice = 138;
inline constexpr int vocalSmoothTargetChoice = 139;
static_assert(vocalSmoothTargetChoice == static_cast<int>(targets.size()),
              "Vocal target IDs must match the target table");

inline int targetChoiceForParameter(const juce::String& parameterId)
{
    for (size_t index = 0; index < targets.size(); ++index)
        if (parameterId == targets[index].parameterId)
            return static_cast<int>(index) + 1;
    return 0;
}

inline juce::String targetName(int choice)
{
    if (choice <= 0 || choice > static_cast<int>(targets.size()))
        return {};
    return targets[static_cast<size_t>(choice - 1)].displayName;
}

inline juce::String stepParameterId(int step)
{
    return "seqStep" + juce::String(step + 1);
}

inline juce::String gateStepParameterId(int step)
{
    return "sequencerGateStep" + juce::String(step + 1);
}

inline GateStepMode gateStepModeFromChoice(float choice) noexcept
{
    return static_cast<GateStepMode>(juce::jlimit(
        static_cast<int>(GateStepMode::off),
        static_cast<int>(GateStepMode::rest),
        static_cast<int>(std::lround(choice))));
}

inline float gateStepTarget(GateStepMode mode, float position,
                            float shortLength, float longLength) noexcept
{
    position = juce::jlimit(0.0f, 1.0f, position);
    shortLength = juce::jlimit(0.10f, 0.60f, shortLength);
    longLength = juce::jlimit(0.65f, 0.95f, longLength);

    switch (mode)
    {
        case GateStepMode::shortStep:
            return position < shortLength ? 1.0f : 0.0f;
        case GateStepMode::longStep:
            return position < longLength ? 1.0f : 0.0f;
        case GateStepMode::rest:
            return 1.0f;
        case GateStepMode::off:
        default:
            return 0.0f;
    }
}

inline juce::String targetParameterId(int slot)
{
    return "seqTarget" + juce::String(slot + 1);
}

inline juce::String targetEnabledParameterId(int slot)
{
    return "seqTargetEnabled" + juce::String(slot + 1);
}

inline juce::String targetDepthParameterId(int slot)
{
    return targetParameterId(slot) + "_depth";
}

inline float applyTargetDepth(float canonicalSequenceValue, bool,
                              float depth) noexcept
{
    // Both sequence displays share one canonical domain: 0.5 is
    // neutral, Unipolar uses 0.5-1.0 and Bipolar uses 0.0-1.0. Moving towards
    // that neutral point makes 0% hold the saved destination and 100% retain
    // the established modulation exactly.
    canonicalSequenceValue = juce::jlimit(
        0.0f, 1.0f, canonicalSequenceValue);
    depth = juce::jlimit(0.0f, 1.0f, depth);
    return 0.5f + (canonicalSequenceValue - 0.5f) * depth;
}

inline double beatsForRate(int rateIndex) noexcept
{
    constexpr std::array<double, rateChoiceCount> beats
    {
        1.0 / 32.0, 1.0 / 24.0, 1.0 / 16.0, 1.0 / 12.0,
        1.0 / 8.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 3.0,
        1.0 / 2.0, 2.0 / 3.0, 1.0, 4.0 / 3.0, 2.0, 4.0
    };
    return beats[static_cast<size_t>(juce::jlimit(
        0, rateChoiceCount - 1, rateIndex))];
}

inline unsigned hash(unsigned value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

inline int greatestCommonDivisor(int left, int right) noexcept
{
    while (right != 0)
    {
        const int remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

inline int randomOffset(int tick, int length) noexcept
{
    if (length <= 1)
        return 0;
    const int shuffle = tick / length;
    const int position = tick % length;
    const unsigned seed = hash(static_cast<unsigned>(length * 131
                                                     + shuffle * 977));
    int stride = 1 + static_cast<int>(seed
        % static_cast<unsigned>(length - 1));
    while (greatestCommonDivisor(stride, length) != 1)
        stride = stride % (length - 1) + 1;
    const int offset = static_cast<int>(
        hash(static_cast<unsigned>(length) ^ 0x9e3779b9U)
        % static_cast<unsigned>(length));
    return (offset + (shuffle & 1) + position * stride) % length;
}

struct Position
{
    int current = 0;
    int previous = 0;
    float fraction = 0.0f;
};

inline Position positionForPhase(double phase, int length,
                                 Direction direction) noexcept
{
    length = juce::jmax(1, length);
    const int cycleLength = direction == Direction::bounce && length > 1
                          ? 2 * (length - 1)
                          : direction == Direction::random ? length * 8
                          : length;
    phase = std::fmod(phase, static_cast<double>(cycleLength));
    if (phase < 0.0)
        phase += static_cast<double>(cycleLength);
    const int tick = juce::jlimit(
        0, cycleLength - 1, static_cast<int>(std::floor(phase)));
    const auto offsetAt = [=](int value)
    {
        value = (value % cycleLength + cycleLength) % cycleLength;
        if (direction == Direction::reverse)
            return length - 1 - value;
        if (direction == Direction::bounce && value >= length)
            return cycleLength - value;
        if (direction == Direction::random)
            return randomOffset(value, length);
        return value;
    };
    return { offsetAt(tick), offsetAt(tick - 1),
             static_cast<float>(phase - std::floor(phase)) };
}

inline float transition(float previous, float current, float position,
                        float attack, float release) noexcept
{
    previous = juce::jlimit(0.0f, 1.0f, previous);
    current = juce::jlimit(0.0f, 1.0f, current);
    if (std::abs(current - previous) <= 0.0001f)
        return current;
    const float amount = juce::jlimit(
        0.0f, 1.0f, current >= previous ? attack : release);
    if (amount <= 0.000001f)
        return current;
    const float progress = juce::jlimit(0.0f, 1.0f, position / amount);
    const float eased = 1.0f - (1.0f - progress) * (1.0f - progress);
    return previous + eased * (current - previous);
}

inline float applyDepthOffset(float value, float minimum, float maximum,
                              float depth, float offsetAmount,
                              bool bipolar) noexcept
{
    value = juce::jlimit(0.0f, 1.0f, value);
    minimum = juce::jlimit(0.0f, 1.0f, minimum);
    maximum = juce::jlimit(minimum, 1.0f, maximum);
    depth = juce::jlimit(-1.0f, 1.0f, depth);
    offsetAmount = juce::jlimit(-1.0f, 1.0f, offsetAmount);
    const float lowerLimit = bipolar ? -1.0f : 0.0f;
    const auto toWorking = [lowerLimit](float input)
    {
        return juce::jlimit(lowerLimit, 1.0f, 2.0f * input - 1.0f);
    };
    const auto fromWorking = [](float input)
    {
        return juce::jlimit(0.0f, 1.0f, 0.5f * (input + 1.0f));
    };
    float workingValue = toWorking(value);
    float workingMinimum = toWorking(minimum);
    float workingMaximum = toWorking(maximum);
    const float excursion = bipolar
        ? juce::jmax(std::abs(workingMinimum),
                     std::abs(workingMaximum))
        : workingMaximum;
    float scale = depth <= 0.0f ? 1.0f + depth : 1.0f;
    if (depth > 0.0f && excursion > 0.000001f)
        scale = 1.0f + depth * (1.0f / excursion - 1.0f);
    workingValue *= scale;
    workingMinimum *= scale;
    workingMaximum *= scale;
    const float offset = offsetAmount >= 0.0f
        ? offsetAmount * (1.0f - workingMaximum)
        : (-offsetAmount) * (lowerLimit - workingMinimum);
    return fromWorking(workingValue + offset);
}

inline float evaluate(
    const std::array<float, stepCount>& steps,
    double phase, int firstStep, int lastStep, Direction direction,
    float attack, float release, int* activeStep = nullptr) noexcept
{
    firstStep = juce::jlimit(0, stepCount - 1, firstStep);
    lastStep = juce::jlimit(firstStep, stepCount - 1, lastStep);
    const auto position = positionForPhase(
        phase, lastStep - firstStep + 1, direction);
    const int currentStep = firstStep + position.current;
    const int previousStep = firstStep + position.previous;
    if (activeStep != nullptr)
        *activeStep = currentStep;
    return transition(steps[static_cast<size_t>(previousStep)],
                      steps[static_cast<size_t>(currentStep)],
                      position.fraction, attack, release);
}

inline std::array<float, stepCount> makeWaveform(Waveform waveform,
                                                 bool bipolar) noexcept
{
    std::array<float, stepCount> result {};
    const int preset = static_cast<int>(waveform);
    const bool doubled = (preset & 1) != 0;
    const int shape = preset / 2;
    const int period = doubled ? stepCount / 2 : stepCount;
    for (int step = 0; step < stepCount; ++step)
    {
        const int local = step % period;
        const float phase = static_cast<float>(local)
                          / static_cast<float>(period);
        float value = 0.0f;
        if (shape == 0)
            value = static_cast<float>(local) / static_cast<float>(period - 1);
        else if (shape == 1)
            value = 1.0f - static_cast<float>(local)
                          / static_cast<float>(period - 1);
        else if (shape == 2)
            value = 0.5f + 0.5f * std::sin(
                juce::MathConstants<float>::twoPi * phase);
        else if (shape == 3)
            value = 1.0f - std::abs(2.0f * phase - 1.0f);
        else if (shape == 4)
            value = phase < 0.25f ? 1.0f : 0.0f;
        else if (shape == 5)
            value = phase < 0.50f ? 1.0f : 0.0f;
        else
            value = phase < 0.75f ? 1.0f : 0.0f;
        result[static_cast<size_t>(step)] = bipolar
            ? juce::jlimit(0.0f, 1.0f, value)
            : 0.5f + 0.5f * juce::jlimit(0.0f, 1.0f, value);
    }
    return result;
}
}
