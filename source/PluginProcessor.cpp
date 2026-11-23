#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <array>
#include <utility>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
namespace ParameterIds
{
constexpr auto distortionType = "distortionType";
constexpr auto distortionEnabled = "distortionEnabled";
constexpr auto drive = "drive";
constexpr auto ampCabinet = "ampCabinet";
constexpr auto distMode = "distMode";
constexpr auto distOutput = "distOutput";
constexpr auto ampSoftClip = "ampSoftClip";
constexpr auto ampTopBoost = "ampTopBoost";
constexpr auto ampBias = "ampBias";
constexpr auto bumpyRipple = "bumpyRipple";
constexpr auto bumpyRate = "bumpyRate";
constexpr auto bumpyContour = "bumpyContour";
constexpr auto bumpyBias = "bumpyBias";
constexpr auto bumpyTone = "bumpyTone";
constexpr auto combTime = "combTime";
constexpr auto combFeedback = "combFeedback";
constexpr auto combDamp = "combDamp";
constexpr auto combSpread = "combSpread";
constexpr auto combTone = "combTone";
constexpr auto crackleDensity = "crackleDensity";
constexpr auto crackleBurst = "crackleBurst";
constexpr auto crackleDecay = "crackleDecay";
constexpr auto crackleGrit = "crackleGrit";
constexpr auto crackleTone = "crackleTone";
constexpr auto lightningSpark = "lightningSpark";
constexpr auto lightningReact = "lightningReact";
constexpr auto lightningClip = "lightningClip";
constexpr auto lightningEdge = "lightningEdge";
constexpr auto lightningTone = "lightningTone";
constexpr auto pixelatedRate = "pixelatedRate";
constexpr auto pixelatedBits = "pixelatedBits";
constexpr auto pixelatedDither = "pixelatedDither";
constexpr auto pixelatedJitter = "pixelatedJitter";
constexpr auto pixelatedTone = "pixelatedTone";
constexpr auto rectifyWave = "rectifyWave";
constexpr auto rectifyBias = "rectifyBias";
constexpr auto rectifyCurve = "rectifyCurve";
constexpr auto rectifyFold = "rectifyFold";
constexpr auto rectifyTone = "rectifyTone";
constexpr auto softClipThreshold = "softClipThreshold";
constexpr auto softClipKnee = "softClipKnee";
constexpr auto softClipBias = "softClipBias";
constexpr auto softClipBody = "softClipBody";
constexpr auto softClipTone = "softClipTone";
constexpr auto tubeBias = "tubeBias";
constexpr auto tubeHeat = "tubeHeat";
constexpr auto tubeSag = "tubeSag";
constexpr auto tubeBody = "tubeBody";
constexpr auto tubeTone = "tubeTone";
constexpr auto stressedPress = "stressedPress";
constexpr auto stressedFold = "stressedFold";
constexpr auto stressedTension = "stressedTension";
constexpr auto stressedBias = "stressedBias";
constexpr auto stressedTone = "stressedTone";
constexpr auto saturationCurve = "saturationCurve";
constexpr auto saturationBias = "saturationBias";
constexpr auto saturationWarm = "saturationWarm";
constexpr auto saturationBody = "saturationBody";
constexpr auto saturationTone = "saturationTone";
constexpr auto samuraiEdge = "samuraiEdge";
constexpr auto samuraiGate = "samuraiGate";
constexpr auto samuraiBlade = "samuraiBlade";
constexpr auto samuraiRing = "samuraiRing";
constexpr auto samuraiTone = "samuraiTone";
constexpr auto razorEdge = "razorEdge";
constexpr auto razorFreq = "razorFreq";
constexpr auto razorRing = "razorRing";
constexpr auto razorBite = "razorBite";
constexpr auto razorTone = "razorTone";
constexpr auto freshFrequency = "freshFrequency";
constexpr auto freshExcite = "freshExcite";
constexpr auto freshAir = "freshAir";
constexpr auto freshSaturation = "freshSaturation";
constexpr auto freshTone = "freshTone";
constexpr auto fuzzGate = "fuzzGate";
constexpr auto fuzzBias = "fuzzBias";
constexpr auto fuzzClip = "fuzzClip";
constexpr auto fuzzBody = "fuzzBody";
constexpr auto fuzzTone = "fuzzTone";
constexpr auto hardClipThreshold = "hardClipThreshold";
constexpr auto hardClipBias = "hardClipBias";
constexpr auto hardClipKnee = "hardClipKnee";
constexpr auto hardClipFold = "hardClipFold";
constexpr auto hardClipTone = "hardClipTone";
constexpr auto asymBias = "asymBias";
constexpr auto asymSkew = "asymSkew";
constexpr auto asymShape = "asymShape";
constexpr auto asymTone = "asymTone";
constexpr auto eqEnabled = "eqEnabled";
constexpr auto eqMode = "eqMode";

constexpr auto mech = "mech";
constexpr auto tube = "tube";
constexpr auto humpBody = "humpBody";
constexpr auto humpBias = "humpBias";
constexpr auto humpFold = "humpFold";
constexpr auto humpTone = "humpTone";
constexpr auto filterEnabled = "filterEnabled";
constexpr auto filterCutoff = "filterCutoff";
constexpr auto filterResonance = "filterResonance";
constexpr auto compEnabled = "compEnabled";
constexpr auto compAmount = "compAmount";
constexpr auto compPhat = "compPhat";
constexpr auto mix = "mix";
constexpr auto output = "output";
constexpr auto bypass = "bypass";
}

juce::String eqParameterId(int index, const char* field)
{
    return "eq" + juce::String(index + 1) + field;
}

constexpr float smoothingSeconds = 0.020f;
constexpr float maximumDriveGain = 16.0f;

float onePoleCoefficient(float cutoffHz, double sampleRate) noexcept
{
    const auto safeRate = juce::jmax(1.0, sampleRate);
    const auto safeCutoff = juce::jlimit(
        1.0f, static_cast<float>(safeRate * 0.45), cutoffHz);
    return std::exp(-juce::MathConstants<float>::twoPi * safeCutoff
                    / static_cast<float>(safeRate));
}
}

CrusherAudioProcessor::CrusherAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "CrusherParameters", createParameterLayout())
{
    bindParameterPointers();
    for (auto& value : sequencerAnimatedNormalised)
        value.store(-1.0f, std::memory_order_relaxed);
    parameters.state.setProperty("schemaVersion", stateSchemaVersion, nullptr);
    setSavedParameterState(parameters.copyState());
    getSettingsDirectory().createDirectory();
    getPresetsDirectory().createDirectory();
    getSequencesDirectory().createDirectory();
}

juce::File CrusherAudioProcessor::getDataDirectory()
{
#if JUCE_WINDOWS
    std::array<wchar_t, 32768> modulePath {};
    const auto moduleHandle = static_cast<HMODULE>(
        juce::Process::getCurrentModuleInstanceHandle());
    if (moduleHandle != nullptr)
    {
        const auto pathLength = ::GetModuleFileNameW(
            moduleHandle, modulePath.data(),
            static_cast<DWORD>(modulePath.size()));
        if (pathLength > 0 && pathLength < modulePath.size() - 1)
        {
            const juce::File moduleFile(juce::String(modulePath.data()));
            auto folder = moduleFile.getParentDirectory();
            for (int depth = 0; depth < 6; ++depth)
            {
                if (folder.hasFileExtension(".vst3"))
                    return folder.getParentDirectory().getChildFile("Crusher_Data");
                const auto parent = folder.getParentDirectory();
                if (parent == folder)
                    break;
                folder = parent;
            }
            return moduleFile.getParentDirectory().getChildFile("Crusher_Data");
        }
    }
#endif
    return juce::File::getSpecialLocation(juce::File::currentApplicationFile)
        .getParentDirectory().getChildFile("Crusher_Data");
}

juce::File CrusherAudioProcessor::getSettingsDirectory()
{
    return getDataDirectory().getChildFile("Settings");
}

juce::File CrusherAudioProcessor::getPresetsDirectory()
{
    return getDataDirectory().getChildFile("Presets");
}

juce::File CrusherAudioProcessor::getSequencesDirectory()
{
    return getDataDirectory().getChildFile("Sequences");
}

juce::Result CrusherAudioProcessor::savePresetToFile(
    const juce::File& presetFile)
{
    if (presetFile == juce::File())
        return juce::Result::fail("No preset file was selected.");

    const auto parent = presetFile.getParentDirectory();
    const auto root = getPresetsDirectory();
    if (parent != root && ! parent.isAChildOf(root))
        return juce::Result::fail(
            "Presets must be saved inside the Crusher Presets folder.");
    if (! parent.isDirectory() && parent.createDirectory().failed())
        return juce::Result::fail("The preset folder could not be created.");

    auto state = parameters.copyState();
    state.setProperty("schemaVersion", stateSchemaVersion, nullptr);
    state.removeProperty("currentPresetPath", nullptr);
    const auto xml = state.createXml();
    if (xml == nullptr)
        return juce::Result::fail("The plugin state could not be encoded.");

    const juce::TemporaryFile temporaryFile(presetFile);
    if (! temporaryFile.getFile().replaceWithText(xml->toString())
        || ! temporaryFile.overwriteTargetFileWithTemporary())
        return juce::Result::fail("The preset could not be written safely.");

    parameters.state.setProperty(
        "currentPresetPath", presetFile.getFullPathName(), nullptr);
    setSavedParameterState(state);
    return juce::Result::ok();
}

juce::Result CrusherAudioProcessor::loadPresetFromFile(
    const juce::File& presetFile)
{
    if (! presetFile.existsAsFile())
        return juce::Result::fail("The preset file no longer exists.");
    const auto parent = presetFile.getParentDirectory();
    const auto root = getPresetsDirectory();
    if (parent != root && ! parent.isAChildOf(root))
        return juce::Result::fail(
            "Presets must be loaded from the Crusher Presets folder.");

    const auto xml = juce::XmlDocument::parse(presetFile);
    if (xml == nullptr || ! xml->hasTagName("CrusherParameters"))
        return juce::Result::fail("This is not a Crusher preset.");
    auto restored = juce::ValueTree::fromXml(*xml);
    if (static_cast<int>(restored.getProperty("schemaVersion", 0))
        != stateSchemaVersion)
        return juce::Result::fail(
            "This preset does not use the current Crusher schema.");

    restored.removeProperty("currentPresetPath", nullptr);
    setSavedParameterState(restored);
    parameters.replaceState(restored);
    parameters.state.setProperty(
        "currentPresetPath", presetFile.getFullPathName(), nullptr);
    pendingProcessingReset.store(true, std::memory_order_release);
    return juce::Result::ok();
}

juce::File CrusherAudioProcessor::getCurrentPresetFile() const
{
    const auto path = parameters.state.getProperty(
        "currentPresetPath", juce::String()).toString();
    return path.isEmpty() ? juce::File() : juce::File(path);
}

void CrusherAudioProcessor::setSavedParameterState(const juce::ValueTree& state)
{
    const juce::ScopedLock lock(savedParameterStateLock);
    savedParameterState = state.createCopy();
    savedParameterState.removeProperty("currentPresetPath", nullptr);
}

std::optional<float> CrusherAudioProcessor::getSavedParameterValue(
    const juce::String& parameterId) const
{
    const juce::ScopedLock lock(savedParameterStateLock);
    for (int index = 0; index < savedParameterState.getNumChildren(); ++index)
    {
        const auto child = savedParameterState.getChild(index);
        if (child.getProperty("id").toString() != parameterId)
            continue;
        const float value = child.getProperty("value").toString()
            .getFloatValue();
        return std::isfinite(value) ? std::optional<float>(value)
                                    : std::nullopt;
    }
    return std::nullopt;
}

juce::AudioProcessorValueTreeState::ParameterLayout
CrusherAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using API = juce::AudioParameterInt;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;

    const auto percentageAttributes = juce::AudioParameterFloatAttributes()
        .withLabel("%");
    const auto targetDepthAttributes = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int)
        {
            return juce::String(juce::roundToInt(value * 100.0f)) + "%";
        })
        .withValueFromStringFunction([](const juce::String& text)
        {
            return text.getFloatValue() * 0.01f;
        });
    auto outputRange = juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f);
    outputRange.setSkewForCentre(0.0f);

    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::distortionType, 1),
        "Distortion Type",
        juce::StringArray { "Soft Clip", "Saturation", "Tube", "Rectify",
            "Crackle", "Lightning", "Amp", "Bumpy", "Asym", "Razor",
            "Pixelated", "Comb", "Stressed", "Samurai", "Fresh",
            "Hard Clip", "Fuzz", "Hump" }, 6));
    // Shared wet-engine controls: stereo is the original, untouched default.
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::distMode, 1), "Distortion Input Mode",
        juce::StringArray { "Stereo", "Mono" }, 0));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::distOutput, 1), "Distortion Output",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    // Amp-only cabinet and waveshaper settings.
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::ampCabinet, 1), "Amp Cabinet",
        juce::StringArray { "D.I.", "Brit", "1x8", "1x12", "2x10",
            "2x12", "4x10", "4x12", "1x15 Bass", "4x10 Bass",
            "Radio", "1x10 Vintage", "2x15 Bass", "8x10 Bass",
            "4x12 Modern", "1x6 Portable" }, 0));
    for (const auto& item : std::array<std::pair<const char*, const char*>, 3> {{
             { ParameterIds::ampSoftClip, "Amp Soft Clip" },
             { ParameterIds::ampTopBoost, "Amp Top Boost" },
             { ParameterIds::ampBias, "Amp Bias" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            percentageAttributes));

    // Bumpy-only ripple and tonal parameters. 50% is the original character.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::bumpyRipple, "Bumpy Ripple" },
             { ParameterIds::bumpyRate, "Bumpy Rate" },
             { ParameterIds::bumpyContour, "Bumpy Contour" },
             { ParameterIds::bumpyBias, "Bumpy Bias" },
             { ParameterIds::bumpyTone, "Bumpy Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Comb-only resonant-delay controls; 50% preserves the existing sound.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::combTime, "Comb Time" },
             { ParameterIds::combFeedback, "Comb Feedback" },
             { ParameterIds::combDamp, "Comb Damp" },
             { ParameterIds::combSpread, "Comb Spread" },
             { ParameterIds::combTone, "Comb Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Crackle: neutral positions retain the original stochastic sound.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::crackleDensity, "Crackle Density" },
             { ParameterIds::crackleBurst, "Crackle Burst" },
             { ParameterIds::crackleDecay, "Crackle Decay" },
             { ParameterIds::crackleGrit, "Crackle Grit" },
             { ParameterIds::crackleTone, "Crackle Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Pixelated: neutral 50% preserves original sample hold, quantiser and dither.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::pixelatedRate, "Pixelated Rate" },
             { ParameterIds::pixelatedBits, "Pixelated Bits" },
             { ParameterIds::pixelatedDither, "Pixelated Dither" },
             { ParameterIds::pixelatedJitter, "Pixelated Jitter" },
             { ParameterIds::pixelatedTone, "Pixelated Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Rectify: original full-wave shaper stays exact at neutral defaults.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 4> {{
             { ParameterIds::rectifyWave, "Rectify Wave" },
             { ParameterIds::rectifyBias, "Rectify Bias" },
             { ParameterIds::rectifyCurve, "Rectify Curve" },
             { ParameterIds::rectifyTone, "Rectify Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::rectifyFold, 1), "Rectify Fold",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));

    // Soft Clip: the six additions default to the original tanh response.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::softClipThreshold, "Soft Clip Threshold" },
             { ParameterIds::softClipKnee, "Soft Clip Knee" },
             { ParameterIds::softClipBias, "Soft Clip Bias" },
             { ParameterIds::softClipBody, "Soft Clip Body" },
             { ParameterIds::softClipTone, "Soft Clip Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Tube-only: neutral positions retain the original standalone Tube shaper.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::tubeBias, "Tube Bias" },
             { ParameterIds::tubeHeat, "Tube Heat" },
             { ParameterIds::tubeSag, "Tube Sag" },
             { ParameterIds::tubeBody, "Tube Body" },
             { ParameterIds::tubeTone, "Tube Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Stressed: compression/folding controls are neutral at 50%, OUTPUT 0 dB.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::stressedPress, "Stressed Press" },
             { ParameterIds::stressedFold, "Stressed Fold" },
             { ParameterIds::stressedTension, "Stressed Tension" },
             { ParameterIds::stressedBias, "Stressed Bias" },
             { ParameterIds::stressedTone, "Stressed Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Saturation: neutral controls retain the original open arctangent curve.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::saturationCurve, "Saturation Curve" },
             { ParameterIds::saturationBias, "Saturation Bias" },
             { ParameterIds::saturationWarm, "Saturation Warm" },
             { ParameterIds::saturationBody, "Saturation Body" },
             { ParameterIds::saturationTone, "Saturation Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Samurai: original blade path remains exact with neutral settings.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::samuraiEdge, "Samurai Edge" },
             { ParameterIds::samuraiGate, "Samurai Gate" },
             { ParameterIds::samuraiBlade, "Samurai Blade" },
             { ParameterIds::samuraiRing, "Samurai Ring" },
             { ParameterIds::samuraiTone, "Samurai Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Razor: 50% means the original high-frequency shaper; OUTPUT is unity at 0 dB.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::razorEdge, "Razor Edge" },
             { ParameterIds::razorFreq, "Razor Freq" },
             { ParameterIds::razorRing, "Razor Ring" },
             { ParameterIds::razorBite, "Razor Bite" },
             { ParameterIds::razorTone, "Razor Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Lightning: neutral 50% controls preserve its original spark/noise clipper.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::lightningSpark, "Lightning Spark" },
             { ParameterIds::lightningReact, "Lightning React" },
             { ParameterIds::lightningClip, "Lightning Clip" },
             { ParameterIds::lightningEdge, "Lightning Edge" },
             { ParameterIds::lightningTone, "Lightning Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Fresh-only exciter and saturation controls. 50% retains the original tone.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::freshFrequency, "Fresh Frequency" },
             { ParameterIds::freshExcite, "Fresh Excite" },
             { ParameterIds::freshAir, "Fresh Air" },
             { ParameterIds::freshSaturation, "Fresh Saturation" },
             { ParameterIds::freshTone, "Fresh Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Hard Clip: 50% retains the original threshold, bias, hard knee and tone.
    // FOLD is neutral at zero; OUTPUT is neutral at 0 dB.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 4> {{
             { ParameterIds::hardClipThreshold, "Hard Clip Threshold" },
             { ParameterIds::hardClipBias, "Hard Clip Bias" },
             { ParameterIds::hardClipKnee, "Hard Clip Knee" },
             { ParameterIds::hardClipTone, "Hard Clip Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::hardClipFold, 1), "Hard Clip Fold",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));

    // Fuzz-only gate, asymmetric clipping, pre-emphasis, post-tone and trim.
    // 50% preserves the original high-gain Fuzz; OUTPUT defaults to 0 dB.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 5> {{
             { ParameterIds::fuzzGate, "Fuzz Gate" },
             { ParameterIds::fuzzBias, "Fuzz Bias" },
             { ParameterIds::fuzzClip, "Fuzz Clip" },
             { ParameterIds::fuzzBody, "Fuzz Body" },
             { ParameterIds::fuzzTone, "Fuzz Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Asym-only tonal and waveshaping controls, with neutral defaults.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 2> {{
             { ParameterIds::asymBias, "Asym Bias" },
             { ParameterIds::asymSkew, "Asym Skew" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f,
            percentageAttributes));
    for (const auto& item : std::array<std::pair<const char*, const char*>, 2> {{
             { ParameterIds::asymShape, "Asym Shape" },
             { ParameterIds::asymTone, "Asym Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));

    // Global seven-band EQ, bypassed by default for a neutral init patch.
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::eqEnabled, 1), "Graphic EQ On", false));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::eqMode, 1), "Graphic EQ Mode",
        juce::StringArray { "Pre", "Emph", "Post" }, 0));
    for (int band = 0; band < crusher::eq::bandCount; ++band)
    {
        const juce::String label = "EQ Band " + juce::String(band + 1);
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(eqParameterId(band, "Freq"), 1),
            label + " Frequency",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.28f),
            crusher::eq::defaultFrequencies[static_cast<size_t>(band)],
            juce::AudioParameterFloatAttributes().withLabel("Hz")));
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(eqParameterId(band, "Gain"), 1),
            label + " Gain", juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f),
            0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(eqParameterId(band, "Q"), 1),
            label + " Width", juce::NormalisableRange<float>(0.35f, 8.0f, 0.01f),
            1.0f));
        layout.push_back(std::make_unique<APC>(
            juce::ParameterID(eqParameterId(band, "Shape"), 1),
            label + " Shape",
            juce::StringArray { "Bell", "Low Shelf", "High Shelf", "Notch",
                "Low Cut", "High Cut" }, 0));
    }

    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::distortionEnabled, 1),
        "Distortion On", true));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::mech, 1), "Mech Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::tube, 1), "Tube Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    // Hump-specific extensions: neutral values are exact no-ops around
    // the original Mech -> Filter -> Tube processing path.
    for (const auto& item : std::array<std::pair<const char*, const char*>, 3> {{
             { ParameterIds::humpBody, "Hump Body" },
             { ParameterIds::humpBias, "Hump Bias" },
             { ParameterIds::humpTone, "Hump Tone" } }})
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(item.first, 1), item.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
            percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::humpFold, 1), "Hump Fold",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::filterEnabled, 1), "Filter On", false));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filterCutoff, 1), "Filter Cutoff",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filterResonance, 1), "Filter Resonance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::compEnabled, 1), "Compressor On", false));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::compAmount, 1), "Compressor Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::compPhat, 1), "Phat Detector", false));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::drive, 1), "Distortion Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::mix, 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::output, 1), "Output", outputRange,
        0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::bypass, 1), "Bypass", false));

    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerEnabled", 1), "Sequencer Enabled", true));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerHostSync", 1), "Sequencer Host Sync", true));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID("sequencerRate", 1), "Sequencer Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T", "1/32",
            "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T",
            "1/2", "1/1" }, 6));
    layout.push_back(std::make_unique<API>(
        juce::ParameterID("sequencerStart", 1), "Sequencer Start", 1,
        crusher::sequencer::stepCount - 1, 1));
    layout.push_back(std::make_unique<API>(
        juce::ParameterID("sequencerEnd", 1), "Sequencer End", 2,
        crusher::sequencer::stepCount, crusher::sequencer::stepCount));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerRangeLink", 1),
        "Sequencer Range Link", false));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID("sequencerDirection", 1), "Sequencer Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" },
        0));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerBipolar", 1), "Sequencer Bipolar", false));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerUnipolarNegative", 1),
        "Sequencer Unipolar Negative", false));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerAttack", 1), "Sequencer Attack",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        targetDepthAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerRelease", 1), "Sequencer Release",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        targetDepthAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerPeak", 1), "Sequencer Depth",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerMove", 1), "Sequencer Offset",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerHue", 1), "Sequencer Hue",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerSaturation", 1), "Sequencer Saturation",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerLuminance", 1), "Sequencer Luminance",
        juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.5f));

    for (int step = 0; step < crusher::sequencer::stepCount; ++step)
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(crusher::sequencer::stepParameterId(step), 1),
            "Sequencer Step " + juce::String(step + 1),
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 1.0f));

    juce::StringArray targetNames { "None" };
    for (const auto& target : crusher::sequencer::targets)
        targetNames.add(target.displayName);
    for (int slot = 0; slot < crusher::sequencer::targetSlotCount; ++slot)
    {
        layout.push_back(std::make_unique<APC>(
            juce::ParameterID(crusher::sequencer::targetParameterId(slot), 1),
            "Sequencer Target " + juce::String(slot + 1), targetNames, 0));
        layout.push_back(std::make_unique<APB>(
            juce::ParameterID(
                crusher::sequencer::targetEnabledParameterId(slot), 1),
            "Sequencer Target " + juce::String(slot + 1) + " Enabled", true));
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(
                crusher::sequencer::targetDepthParameterId(slot), 1),
            "Sequencer Target " + juce::String(slot + 1) + " Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f,
            targetDepthAttributes));
    }

    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerGateShortLength", 1),
        "Sequencer Gate Short Length",
        juce::NormalisableRange<float>(0.10f, 0.60f, 0.01f), 0.50f,
        targetDepthAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerGateLongLength", 1),
        "Sequencer Gate Long Length",
        juce::NormalisableRange<float>(0.65f, 0.95f, 0.01f), 0.90f,
        targetDepthAttributes));
    for (int step = 0; step < crusher::sequencer::stepCount; ++step)
        layout.push_back(std::make_unique<APC>(
            juce::ParameterID(crusher::sequencer::gateStepParameterId(step), 1),
            "Sequencer Gate Step " + juce::String(step + 1),
            juce::StringArray { "Off", "Short", "Long", "Rest" },
            static_cast<int>(crusher::sequencer::GateStepMode::rest)));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID("sequencerGateMode", 1), "Sequencer Gate Mode",
        juce::StringArray { "Input", "Effect", "Output", "Off" }, 0));

    return { layout.begin(), layout.end() };
}

void CrusherAudioProcessor::bindParameterPointers()
{
    const auto bind = [this](const juce::String& id)
    {
        auto* value = parameters.getRawParameterValue(id);
        jassert(value != nullptr);
        return value;
    };
    raw.distortionType = bind(ParameterIds::distortionType);
    raw.distortionEnabled = bind(ParameterIds::distortionEnabled);
    raw.drive = bind(ParameterIds::drive);
    raw.ampCabinet = bind(ParameterIds::ampCabinet);
    raw.distMode = bind(ParameterIds::distMode);
    raw.distOutput = bind(ParameterIds::distOutput);
    raw.ampSoftClip = bind(ParameterIds::ampSoftClip);
    raw.ampTopBoost = bind(ParameterIds::ampTopBoost);
    raw.ampBias = bind(ParameterIds::ampBias);
    raw.bumpyRipple = bind(ParameterIds::bumpyRipple);
    raw.bumpyRate = bind(ParameterIds::bumpyRate);
    raw.bumpyContour = bind(ParameterIds::bumpyContour);
    raw.bumpyBias = bind(ParameterIds::bumpyBias);
    raw.bumpyTone = bind(ParameterIds::bumpyTone);
    raw.combTime = bind(ParameterIds::combTime);
    raw.combFeedback = bind(ParameterIds::combFeedback);
    raw.combDamp = bind(ParameterIds::combDamp);
    raw.combSpread = bind(ParameterIds::combSpread);
    raw.combTone = bind(ParameterIds::combTone);
    raw.crackleDensity = bind(ParameterIds::crackleDensity);
    raw.crackleBurst = bind(ParameterIds::crackleBurst);
    raw.crackleDecay = bind(ParameterIds::crackleDecay);
    raw.crackleGrit = bind(ParameterIds::crackleGrit);
    raw.crackleTone = bind(ParameterIds::crackleTone);
    raw.lightningSpark = bind(ParameterIds::lightningSpark);
    raw.lightningReact = bind(ParameterIds::lightningReact);
    raw.lightningClip = bind(ParameterIds::lightningClip);
    raw.lightningEdge = bind(ParameterIds::lightningEdge);
    raw.lightningTone = bind(ParameterIds::lightningTone);
    raw.pixelatedRate = bind(ParameterIds::pixelatedRate);
    raw.pixelatedBits = bind(ParameterIds::pixelatedBits);
    raw.pixelatedDither = bind(ParameterIds::pixelatedDither);
    raw.pixelatedJitter = bind(ParameterIds::pixelatedJitter);
    raw.pixelatedTone = bind(ParameterIds::pixelatedTone);
    raw.rectifyWave = bind(ParameterIds::rectifyWave);
    raw.rectifyBias = bind(ParameterIds::rectifyBias);
    raw.rectifyCurve = bind(ParameterIds::rectifyCurve);
    raw.rectifyFold = bind(ParameterIds::rectifyFold);
    raw.rectifyTone = bind(ParameterIds::rectifyTone);
    raw.softClipThreshold = bind(ParameterIds::softClipThreshold);
    raw.softClipKnee = bind(ParameterIds::softClipKnee);
    raw.softClipBias = bind(ParameterIds::softClipBias);
    raw.softClipBody = bind(ParameterIds::softClipBody);
    raw.softClipTone = bind(ParameterIds::softClipTone);
    raw.tubeBias = bind(ParameterIds::tubeBias);
    raw.tubeHeat = bind(ParameterIds::tubeHeat);
    raw.tubeSag = bind(ParameterIds::tubeSag);
    raw.tubeBody = bind(ParameterIds::tubeBody);
    raw.tubeTone = bind(ParameterIds::tubeTone);
    raw.stressedPress = bind(ParameterIds::stressedPress);
    raw.stressedFold = bind(ParameterIds::stressedFold);
    raw.stressedTension = bind(ParameterIds::stressedTension);
    raw.stressedBias = bind(ParameterIds::stressedBias);
    raw.stressedTone = bind(ParameterIds::stressedTone);
    raw.saturationCurve = bind(ParameterIds::saturationCurve);
    raw.saturationBias = bind(ParameterIds::saturationBias);
    raw.saturationWarm = bind(ParameterIds::saturationWarm);
    raw.saturationBody = bind(ParameterIds::saturationBody);
    raw.saturationTone = bind(ParameterIds::saturationTone);
    raw.samuraiEdge = bind(ParameterIds::samuraiEdge);
    raw.samuraiGate = bind(ParameterIds::samuraiGate);
    raw.samuraiBlade = bind(ParameterIds::samuraiBlade);
    raw.samuraiRing = bind(ParameterIds::samuraiRing);
    raw.samuraiTone = bind(ParameterIds::samuraiTone);
    raw.razorEdge = bind(ParameterIds::razorEdge);
    raw.razorFreq = bind(ParameterIds::razorFreq);
    raw.razorRing = bind(ParameterIds::razorRing);
    raw.razorBite = bind(ParameterIds::razorBite);
    raw.razorTone = bind(ParameterIds::razorTone);
    raw.freshFrequency = bind(ParameterIds::freshFrequency);
    raw.freshExcite = bind(ParameterIds::freshExcite);
    raw.freshAir = bind(ParameterIds::freshAir);
    raw.freshSaturation = bind(ParameterIds::freshSaturation);
    raw.freshTone = bind(ParameterIds::freshTone);
    raw.fuzzGate = bind(ParameterIds::fuzzGate);
    raw.fuzzBias = bind(ParameterIds::fuzzBias);
    raw.fuzzClip = bind(ParameterIds::fuzzClip);
    raw.fuzzBody = bind(ParameterIds::fuzzBody);
    raw.fuzzTone = bind(ParameterIds::fuzzTone);
    raw.hardClipThreshold = bind(ParameterIds::hardClipThreshold);
    raw.hardClipBias = bind(ParameterIds::hardClipBias);
    raw.hardClipKnee = bind(ParameterIds::hardClipKnee);
    raw.hardClipFold = bind(ParameterIds::hardClipFold);
    raw.hardClipTone = bind(ParameterIds::hardClipTone);
    raw.asymBias = bind(ParameterIds::asymBias);
    raw.asymSkew = bind(ParameterIds::asymSkew);
    raw.asymShape = bind(ParameterIds::asymShape);
    raw.asymTone = bind(ParameterIds::asymTone);
    raw.eqEnabled = bind(ParameterIds::eqEnabled);
    raw.eqMode = bind(ParameterIds::eqMode);
    for (int band = 0; band < crusher::eq::bandCount; ++band)
    {
        const auto index = static_cast<size_t>(band);
        raw.eqFrequency[index] = bind(eqParameterId(band, "Freq"));
        raw.eqGain[index] = bind(eqParameterId(band, "Gain"));
        raw.eqQ[index] = bind(eqParameterId(band, "Q"));
        raw.eqShape[index] = bind(eqParameterId(band, "Shape"));
    }
    raw.mech = bind(ParameterIds::mech);
    raw.tube = bind(ParameterIds::tube);
    raw.humpBody = bind(ParameterIds::humpBody);
    raw.humpBias = bind(ParameterIds::humpBias);
    raw.humpFold = bind(ParameterIds::humpFold);
    raw.humpTone = bind(ParameterIds::humpTone);
    raw.filterEnabled = bind(ParameterIds::filterEnabled);
    raw.filterCutoff = bind(ParameterIds::filterCutoff);
    raw.filterResonance = bind(ParameterIds::filterResonance);
    raw.compEnabled = bind(ParameterIds::compEnabled);
    raw.compAmount = bind(ParameterIds::compAmount);
    raw.compPhat = bind(ParameterIds::compPhat);
    raw.mix = bind(ParameterIds::mix);
    raw.output = bind(ParameterIds::output);
    raw.bypass = bind(ParameterIds::bypass);
    raw.sequencerEnabled = bind("sequencerEnabled");
    raw.sequencerHostSync = bind("sequencerHostSync");
    raw.sequencerRate = bind("sequencerRate");
    raw.sequencerStart = bind("sequencerStart");
    raw.sequencerEnd = bind("sequencerEnd");
    raw.sequencerDirection = bind("sequencerDirection");
    raw.sequencerBipolar = bind("sequencerBipolar");
    raw.sequencerUnipolarNegative = bind("sequencerUnipolarNegative");
    raw.sequencerAttack = bind("sequencerAttack");
    raw.sequencerRelease = bind("sequencerRelease");
    raw.sequencerPeak = bind("sequencerPeak");
    raw.sequencerMove = bind("sequencerMove");
    raw.sequencerGateShortLength = bind("sequencerGateShortLength");
    raw.sequencerGateLongLength = bind("sequencerGateLongLength");
    raw.sequencerGateMode = bind("sequencerGateMode");
    for (int step = 0; step < crusher::sequencer::stepCount; ++step)
    {
        const auto index = static_cast<size_t>(step);
        raw.sequencerSteps[index] = bind(
            crusher::sequencer::stepParameterId(step));
        raw.sequencerGateSteps[index] = bind(
            crusher::sequencer::gateStepParameterId(step));
    }
    for (int slot = 0; slot < crusher::sequencer::targetSlotCount; ++slot)
    {
        const auto index = static_cast<size_t>(slot);
        raw.sequencerTargets[index] = bind(
            crusher::sequencer::targetParameterId(slot));
        raw.sequencerTargetEnabled[index] = bind(
            crusher::sequencer::targetEnabledParameterId(slot));
        raw.sequencerTargetDepth[index] = bind(
            crusher::sequencer::targetDepthParameterId(slot));
    }
    for (size_t target = 0; target < crusher::sequencer::targets.size(); ++target)
        sequencerTargetParameters[target] = parameters.getParameter(
            crusher::sequencer::targets[target].parameterId);
}

float CrusherAudioProcessor::load(const std::atomic<float>* value) noexcept
{
    const float loaded = value != nullptr
        ? value->load(std::memory_order_relaxed) : 0.0f;
    return std::isfinite(loaded) ? loaded : 0.0f;
}

bool CrusherAudioProcessor::isSequencerTargetAssigned(int targetChoice) const noexcept
{
    if (targetChoice <= 0)
        return false;
    for (size_t slot = 0; slot < raw.sequencerTargets.size(); ++slot)
        if (load(raw.sequencerTargetEnabled[slot]) >= 0.5f
            && static_cast<int>(std::lround(load(raw.sequencerTargets[slot])))
                   == targetChoice)
            return true;
    return false;
}

float CrusherAudioProcessor::sequencerTargetDepth(int targetChoice) const noexcept
{
    if (targetChoice <= 0)
        return 1.0f;
    for (size_t slot = 0; slot < raw.sequencerTargets.size(); ++slot)
        if (load(raw.sequencerTargetEnabled[slot]) >= 0.5f
            && static_cast<int>(std::lround(load(raw.sequencerTargets[slot])))
                   == targetChoice)
            return juce::jlimit(
                0.0f, 1.0f, load(raw.sequencerTargetDepth[slot]));
    return 1.0f;
}

float CrusherAudioProcessor::sequencerModulatedValue(
    int targetChoice, float baseValue, float canonicalSequenceValue,
    bool bipolar, bool unipolarNegative,
    juce::RangedAudioParameter* valueParameter) const noexcept
{
    if (! isSequencerTargetAssigned(targetChoice)
        || targetChoice > static_cast<int>(sequencerTargetParameters.size()))
        return baseValue;
    auto* parameter = valueParameter != nullptr
        ? valueParameter
        : sequencerTargetParameters[static_cast<size_t>(targetChoice - 1)];
    if (parameter == nullptr)
        return baseValue;

    canonicalSequenceValue = crusher::sequencer::applyTargetDepth(
        canonicalSequenceValue, bipolar, sequencerTargetDepth(targetChoice));
    const float baseNormalised = juce::jlimit(
        0.0f, 1.0f, parameter->convertTo0to1(baseValue));
    float modulatedNormalised = baseNormalised;
    if (bipolar)
    {
        const float deviation = 2.0f * canonicalSequenceValue - 1.0f;
        const float radius = juce::jmin(
            baseNormalised, 1.0f - baseNormalised);
        modulatedNormalised = baseNormalised + deviation * radius;
    }
    else
    {
        const float amount = juce::jlimit(
            0.0f, 1.0f, 2.0f * canonicalSequenceValue - 1.0f);
        modulatedNormalised = unipolarNegative
            ? baseNormalised - amount * baseNormalised
            : baseNormalised + amount * (1.0f - baseNormalised);
    }
    modulatedNormalised = juce::jlimit(
        0.0f, 1.0f, modulatedNormalised);
    sequencerAnimatedNormalised[static_cast<size_t>(targetChoice - 1)].store(
        modulatedNormalised, std::memory_order_relaxed);
    return parameter->convertFrom0to1(modulatedNormalised);
}

std::optional<float> CrusherAudioProcessor::getSequencerAnimatedNormalised(
    const juce::String& parameterId) const noexcept
{
    const int choice = crusher::sequencer::targetChoiceForParameter(parameterId);
    if (choice <= 0
        || choice > static_cast<int>(sequencerAnimatedNormalised.size())
        || load(raw.sequencerEnabled) < 0.5f
        || ! isSequencerTargetAssigned(choice))
        return std::nullopt;
    const float value = sequencerAnimatedNormalised[
        static_cast<size_t>(choice - 1)].load(std::memory_order_relaxed);
    if (! std::isfinite(value) || value < 0.0f)
        return std::nullopt;
    return juce::jlimit(0.0f, 1.0f, value);
}

bool CrusherAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo());
}

void CrusherAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = juce::jmax(1.0, sampleRate);
    rectifyProcessor.prepare(currentSampleRate);
    crackleProcessor.prepare(currentSampleRate);
    lightningProcessor.prepare(currentSampleRate);
    pixelatedProcessor.prepare(currentSampleRate);
    razorProcessor.prepare(currentSampleRate);
    softClipProcessor.prepare(currentSampleRate);
    tubeProcessor.prepare(currentSampleRate);
    stressedProcessor.prepare(currentSampleRate);
    saturationProcessor.prepare(currentSampleRate);
    samuraiProcessor.prepare(currentSampleRate);
    freshProcessor.prepare(currentSampleRate);
    fuzzProcessor.prepare(currentSampleRate);
    hardClipProcessor.prepare(currentSampleRate);
    humpProcessor.prepare(currentSampleRate);
    ampDcCoefficient = onePoleCoefficient(24.0f, currentSampleRate);
    ampToneCoefficient = onePoleCoefficient(9000.0f, currentSampleRate);
    bumpyProcessor.prepare(currentSampleRate);
    asymProcessor.prepare(currentSampleRate);
    combProcessor.prepare(currentSampleRate);
    resetDistortionState();
    ampSimulator.setSampleRate(currentSampleRate);
    graphicEQ.setSampleRate(currentSampleRate);
    humpFilter.setSampleRate(currentSampleRate);
    humpCompressor.setSampleRate(currentSampleRate);
    for (auto* ramp : { &mechRamp, &tubeRamp, &filterCutoffRamp,
                        &filterResonanceRamp, &compAmountRamp })
        ramp->setSampleRate(currentSampleRate);
    mechRamp.setTarget(juce::jlimit(0.0f, 1.0f, load(raw.mech) * 0.01f));
    tubeRamp.setTarget(juce::jlimit(0.0f, 1.0f, load(raw.tube) * 0.01f));
    filterCutoffRamp.setTarget(juce::jlimit(0.0f, 1.0f,
        load(raw.filterCutoff) * 0.01f));
    filterResonanceRamp.setTarget(juce::jlimit(0.0f, 1.0f,
        load(raw.filterResonance) * 0.01f));
    compAmountRamp.setTarget(juce::jlimit(0.0f, 1.0f,
        load(raw.compAmount) * 0.01f) * hump::Compressor::kMaxAmount);
    for (auto* ramp : { &mechRamp, &tubeRamp, &filterCutoffRamp,
                        &filterResonanceRamp, &compAmountRamp })
        ramp->snapToTarget();

    const auto resetSmoother = [this](LinearSmoother& smoother, float value)
    {
        smoother.reset(currentSampleRate, smoothingSeconds);
        smoother.setCurrentAndTargetValue(value);
    };
    resetSmoother(distortionEnabledValue,
        load(raw.distortionEnabled) >= 0.5f ? 1.0f : 0.0f);
    resetSmoother(driveValue,
        juce::jlimit(0.0f, 1.0f, load(raw.drive) * 0.01f));
    resetSmoother(bumpyRippleValue, load(raw.bumpyRipple) * 0.01f);
    resetSmoother(bumpyRateValue, load(raw.bumpyRate) * 0.01f);
    resetSmoother(bumpyContourValue, load(raw.bumpyContour) * 0.01f);
    resetSmoother(bumpyBiasValue, load(raw.bumpyBias) * 0.01f);
    resetSmoother(bumpyToneValue, load(raw.bumpyTone) * 0.01f);
    resetSmoother(combTimeValue, load(raw.combTime) * 0.01f);
    resetSmoother(combFeedbackValue, load(raw.combFeedback) * 0.01f);
    resetSmoother(combDampValue, load(raw.combDamp) * 0.01f);
    resetSmoother(combSpreadValue, load(raw.combSpread) * 0.01f);
    resetSmoother(combToneValue, load(raw.combTone) * 0.01f);
    resetSmoother(crackleDensityValue, load(raw.crackleDensity) * 0.01f);
    resetSmoother(crackleBurstValue, load(raw.crackleBurst) * 0.01f);
    resetSmoother(crackleDecayValue, load(raw.crackleDecay) * 0.01f);
    resetSmoother(crackleGritValue, load(raw.crackleGrit) * 0.01f);
    resetSmoother(crackleToneValue, load(raw.crackleTone) * 0.01f);
    resetSmoother(lightningSparkValue, load(raw.lightningSpark) * 0.01f);
    resetSmoother(lightningReactValue, load(raw.lightningReact) * 0.01f);
    resetSmoother(lightningClipValue, load(raw.lightningClip) * 0.01f);
    resetSmoother(lightningEdgeValue, load(raw.lightningEdge) * 0.01f);
    resetSmoother(lightningToneValue, load(raw.lightningTone) * 0.01f);
    resetSmoother(pixelatedRateValue, load(raw.pixelatedRate) * 0.01f);
    resetSmoother(pixelatedBitsValue, load(raw.pixelatedBits) * 0.01f);
    resetSmoother(pixelatedDitherValue, load(raw.pixelatedDither) * 0.01f);
    resetSmoother(pixelatedJitterValue, load(raw.pixelatedJitter) * 0.01f);
    resetSmoother(pixelatedToneValue, load(raw.pixelatedTone) * 0.01f);
    resetSmoother(rectifyWaveValue, load(raw.rectifyWave) * 0.01f);
    resetSmoother(rectifyBiasValue, load(raw.rectifyBias) * 0.01f);
    resetSmoother(rectifyCurveValue, load(raw.rectifyCurve) * 0.01f);
    resetSmoother(rectifyFoldValue, load(raw.rectifyFold) * 0.01f);
    resetSmoother(rectifyToneValue, load(raw.rectifyTone) * 0.01f);
    resetSmoother(softClipThresholdValue, load(raw.softClipThreshold) * 0.01f);
    resetSmoother(softClipKneeValue, load(raw.softClipKnee) * 0.01f);
    resetSmoother(softClipBiasValue, load(raw.softClipBias) * 0.01f);
    resetSmoother(softClipBodyValue, load(raw.softClipBody) * 0.01f);
    resetSmoother(softClipToneValue, load(raw.softClipTone) * 0.01f);
    resetSmoother(tubeBiasValue, load(raw.tubeBias) * 0.01f);
    resetSmoother(tubeHeatValue, load(raw.tubeHeat) * 0.01f);
    resetSmoother(tubeSagValue, load(raw.tubeSag) * 0.01f);
    resetSmoother(tubeBodyValue, load(raw.tubeBody) * 0.01f);
    resetSmoother(tubeToneValue, load(raw.tubeTone) * 0.01f);
    resetSmoother(stressedPressValue, load(raw.stressedPress) * 0.01f);
    resetSmoother(stressedFoldValue, load(raw.stressedFold) * 0.01f);
    resetSmoother(stressedTensionValue, load(raw.stressedTension) * 0.01f);
    resetSmoother(stressedBiasValue, load(raw.stressedBias) * 0.01f);
    resetSmoother(stressedToneValue, load(raw.stressedTone) * 0.01f);
    resetSmoother(saturationCurveValue, load(raw.saturationCurve) * 0.01f);
    resetSmoother(saturationBiasValue, load(raw.saturationBias) * 0.01f);
    resetSmoother(saturationWarmValue, load(raw.saturationWarm) * 0.01f);
    resetSmoother(saturationBodyValue, load(raw.saturationBody) * 0.01f);
    resetSmoother(saturationToneValue, load(raw.saturationTone) * 0.01f);
    resetSmoother(samuraiEdgeValue, load(raw.samuraiEdge) * 0.01f);
    resetSmoother(samuraiGateValue, load(raw.samuraiGate) * 0.01f);
    resetSmoother(samuraiBladeValue, load(raw.samuraiBlade) * 0.01f);
    resetSmoother(samuraiRingValue, load(raw.samuraiRing) * 0.01f);
    resetSmoother(samuraiToneValue, load(raw.samuraiTone) * 0.01f);
    resetSmoother(razorEdgeValue, load(raw.razorEdge) * 0.01f);
    resetSmoother(razorFreqValue, load(raw.razorFreq) * 0.01f);
    resetSmoother(razorRingValue, load(raw.razorRing) * 0.01f);
    resetSmoother(razorBiteValue, load(raw.razorBite) * 0.01f);
    resetSmoother(razorToneValue, load(raw.razorTone) * 0.01f);
    resetSmoother(freshFrequencyValue, load(raw.freshFrequency) * 0.01f);
    resetSmoother(freshExciteValue, load(raw.freshExcite) * 0.01f);
    resetSmoother(freshAirValue, load(raw.freshAir) * 0.01f);
    resetSmoother(freshSaturationValue, load(raw.freshSaturation) * 0.01f);
    resetSmoother(freshToneValue, load(raw.freshTone) * 0.01f);
    resetSmoother(fuzzGateValue, load(raw.fuzzGate) * 0.01f);
    resetSmoother(fuzzBiasValue, load(raw.fuzzBias) * 0.01f);
    resetSmoother(fuzzClipValue, load(raw.fuzzClip) * 0.01f);
    resetSmoother(fuzzBodyValue, load(raw.fuzzBody) * 0.01f);
    resetSmoother(fuzzToneValue, load(raw.fuzzTone) * 0.01f);
    resetSmoother(hardClipThresholdValue, load(raw.hardClipThreshold) * 0.01f);
    resetSmoother(hardClipBiasValue, load(raw.hardClipBias) * 0.01f);
    resetSmoother(hardClipKneeValue, load(raw.hardClipKnee) * 0.01f);
    resetSmoother(hardClipFoldValue, load(raw.hardClipFold) * 0.01f);
    resetSmoother(hardClipToneValue, load(raw.hardClipTone) * 0.01f);
    resetSmoother(humpBodyValue, load(raw.humpBody) * 0.01f);
    resetSmoother(humpBiasValue, load(raw.humpBias) * 0.01f);
    resetSmoother(humpFoldValue, load(raw.humpFold) * 0.01f);
    resetSmoother(humpToneValue, load(raw.humpTone) * 0.01f);
    resetSmoother(asymBiasValue, load(raw.asymBias) * 0.01f);
    resetSmoother(asymSkewValue, load(raw.asymSkew) * 0.01f);
    resetSmoother(asymShapeValue, load(raw.asymShape) * 0.01f);
    resetSmoother(asymToneValue, load(raw.asymTone) * 0.01f);
    resetSmoother(mixValue,
        juce::jlimit(0.0f, 1.0f, load(raw.mix) * 0.01f));
    resetSmoother(distOutputGainValue,
        juce::Decibels::decibelsToGain(load(raw.distOutput)));
    resetSmoother(outputGainValue,
        juce::Decibels::decibelsToGain(load(raw.output)));
    resetSmoother(bypassValue, load(raw.bypass) >= 0.5f ? 1.0f : 0.0f);
    currentDistortionType = juce::jlimit(
        0, distortionTypeCount - 1,
        static_cast<int>(std::lround(load(raw.distortionType))));
    previousDistortionType = currentDistortionType;
    resetSmoother(distortionTypeCrossfade, 1.0f);
    resetSmoother(graphicEQEnabledValue,
        load(raw.eqEnabled) >= 0.5f ? 1.0f : 0.0f);
    sequencerFreePhase = 0.0;
    sequencerHeldNotes = 0;
    previousSequencerCanonical = 1.0f;
    sequencerGateGain = 1.0f;
}

void CrusherAudioProcessor::releaseResources()
{
    resetDistortionState();
    humpFilter.reset();
    humpCompressor.reset();
    ampSimulator.reset();
    asymProcessor.reset();
    combProcessor.reset();
    graphicEQ.reset();
    humpProcessor.reset();
    activeSequencerStep.store(-1, std::memory_order_relaxed);
}

float CrusherAudioProcessor::nextNoiseSample(uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(static_cast<int32_t>(state))
        / 2147483648.0f;
}

float CrusherAudioProcessor::sanitiseSample(float sample, float limit) noexcept
{
    if (! std::isfinite(sample))
        return 0.0f;
    return juce::jlimit(-limit, limit, sample);
}

void CrusherAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int sampleCount = buffer.getNumSamples();
    const int hostChannels = buffer.getNumChannels();
    const int activeChannels = juce::jmin(channelCount, hostChannels);
    for (int channel = activeChannels; channel < hostChannels; ++channel)
        buffer.clear(channel, 0, sampleCount);
    if (sampleCount <= 0 || activeChannels <= 0)
        return;
    if (pendingProcessingReset.exchange(false, std::memory_order_acq_rel))
    {
        ampSimulator.reset();
        asymProcessor.reset();
        graphicEQ.reset();
        humpProcessor.reset();
        resetDistortionState();
        humpFilter.reset();
        humpCompressor.reset();
    }

    const bool sequenceEnabled = load(raw.sequencerEnabled) >= 0.5f;
    const bool hostSync = load(raw.sequencerHostSync) >= 0.5f;
    const auto direction = static_cast<crusher::sequencer::Direction>(
        juce::jlimit(0, 4, static_cast<int>(std::lround(
            load(raw.sequencerDirection)))));

    juce::Optional<juce::AudioPlayHead::PositionInfo> hostPosition;
    if (sequenceEnabled && hostSync
        && direction != crusher::sequencer::Direction::played)
        if (auto* playhead = getPlayHead())
            hostPosition = playhead->getPosition();
    double bpm = 120.0;
    if (hostPosition.hasValue())
        if (const auto hostBpm = hostPosition->getBpm())
            if (std::isfinite(*hostBpm) && *hostBpm > 0.0)
                bpm = juce::jlimit(10.0, 1000.0, *hostBpm);

    bool phraseTriggered = false;
    if (sequenceEnabled && direction == crusher::sequencer::Direction::played)
    {
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn())
            {
                if (sequencerHeldNotes == 0)
                    phraseTriggered = true;
                ++sequencerHeldNotes;
            }
            else if (message.isNoteOff())
                sequencerHeldNotes = juce::jmax(0, sequencerHeldNotes - 1);
            else if (message.isAllNotesOff() || message.isAllSoundOff())
                sequencerHeldNotes = 0;
        }
    }
    else
        sequencerHeldNotes = 0;

    std::array<float, crusher::sequencer::stepCount> steps {};
    float sequenceMinimum = 1.0f;
    float sequenceMaximum = 0.0f;
    for (int step = 0; step < crusher::sequencer::stepCount; ++step)
    {
        const auto index = static_cast<size_t>(step);
        steps[index] = juce::jlimit(
            0.0f, 1.0f, load(raw.sequencerSteps[index]));
        sequenceMinimum = juce::jmin(sequenceMinimum, steps[index]);
        sequenceMaximum = juce::jmax(sequenceMaximum, steps[index]);
    }
    const bool bipolar = load(raw.sequencerBipolar) >= 0.5f;
    const bool unipolarNegative = ! bipolar
        && load(raw.sequencerUnipolarNegative) >= 0.5f;
    const auto modulatedPrevious = [this, sequenceEnabled, bipolar,
                                    unipolarNegative](
        int target, const std::atomic<float>* parameter)
    {
        const float base = load(parameter);
        return sequenceEnabled
            ? sequencerModulatedValue(target, base,
                previousSequencerCanonical, bipolar, unipolarNegative)
            : base;
    };
    const float peak = juce::jlimit(-1.0f, 1.0f, modulatedPrevious(
        crusher::sequencer::sequencerDepthTargetChoice, raw.sequencerPeak));
    const float move = juce::jlimit(-1.0f, 1.0f, modulatedPrevious(
        crusher::sequencer::sequencerOffsetTargetChoice, raw.sequencerMove));
    if (sequenceEnabled
        && (std::abs(peak) > 0.000001f || std::abs(move) > 0.000001f))
        for (auto& value : steps)
            value = crusher::sequencer::applyDepthOffset(
                value, sequenceMinimum, sequenceMaximum,
                peak, move, bipolar);

    const int rate = juce::jlimit(
        0, crusher::sequencer::rateChoiceCount - 1,
        static_cast<int>(std::lround(load(raw.sequencerRate))));
    const double beatsPerStep = crusher::sequencer::beatsForRate(rate);
    const double phaseIncrement = bpm / (
        60.0 * beatsPerStep * currentSampleRate);
    if (phraseTriggered)
        sequencerFreePhase = 0.0;
    double gatePhaseAtBlockStart = sequencerFreePhase;
    double sequencePhase = sequencerFreePhase
        + 0.5 * static_cast<double>(sampleCount) * phaseIncrement;
    if (sequenceEnabled && hostSync
        && direction != crusher::sequencer::Direction::played
        && hostPosition.hasValue())
        if (const auto ppq = hostPosition->getPpqPosition())
            if (std::isfinite(*ppq))
            {
                sequencePhase = *ppq / beatsPerStep;
                gatePhaseAtBlockStart = sequencePhase;
            }
    if (sequenceEnabled)
        sequencerFreePhase += static_cast<double>(sampleCount) * phaseIncrement;
    if (sequencerFreePhase > 1048576.0)
        sequencerFreePhase = std::fmod(
            sequencerFreePhase,
            static_cast<double>(crusher::sequencer::stepCount * 8));

    const int firstStep = juce::jlimit(
        1, crusher::sequencer::stepCount - 1,
        static_cast<int>(std::lround(modulatedPrevious(
            crusher::sequencer::sequencerStartTargetChoice,
            raw.sequencerStart)))) - 1;
    const int lastStep = juce::jlimit(
        firstStep + 2, crusher::sequencer::stepCount,
        static_cast<int>(std::lround(modulatedPrevious(
            crusher::sequencer::sequencerEndTargetChoice,
            raw.sequencerEnd)))) - 1;
    const float attack = juce::jlimit(0.0f, 1.0f, modulatedPrevious(
        crusher::sequencer::sequencerAttackTargetChoice, raw.sequencerAttack));
    const float release = juce::jlimit(0.0f, 1.0f, modulatedPrevious(
        crusher::sequencer::sequencerReleaseTargetChoice,
        raw.sequencerRelease));
    int activeStep = -1;
    float canonical = 1.0f;
    if (sequenceEnabled)
        canonical = crusher::sequencer::evaluate(
            steps, sequencePhase, firstStep, lastStep, direction,
            attack, release, &activeStep);
    previousSequencerCanonical = canonical;
    activeSequencerStep.store(activeStep, std::memory_order_relaxed);

    const auto sequenced = [this, sequenceEnabled, canonical, bipolar,
                            unipolarNegative](
        int target, const std::atomic<float>* parameter)
    {
        const float base = load(parameter);
        return sequenceEnabled
            ? sequencerModulatedValue(target, base, canonical,
                bipolar, unipolarNegative)
            : base;
    };
    distortionEnabledValue.setTargetValue(
        load(raw.distortionEnabled) >= 0.5f ? 1.0f : 0.0f);
    driveValue.setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(crusher::sequencer::driveTargetChoice,
            raw.drive) * 0.01f));
    mixValue.setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(crusher::sequencer::mixTargetChoice,
            raw.mix) * 0.01f));
    distOutputGainValue.setTargetValue(juce::Decibels::decibelsToGain(
        sequenced(crusher::sequencer::distOutputTargetChoice, raw.distOutput)));
    outputGainValue.setTargetValue(juce::Decibels::decibelsToGain(
        sequenced(crusher::sequencer::outputTargetChoice, raw.output)));
    bypassValue.setTargetValue(load(raw.bypass) >= 0.5f ? 1.0f : 0.0f);
    mechRamp.setTarget(juce::jlimit(0.0f, 1.0f, sequenced(
        crusher::sequencer::mechTargetChoice, raw.mech) * 0.01f));
    tubeRamp.setTarget(juce::jlimit(0.0f, 1.0f, sequenced(
        crusher::sequencer::tubeTargetChoice, raw.tube) * 0.01f));
    filterCutoffRamp.setTarget(juce::jlimit(0.0f, 1.0f, sequenced(
        crusher::sequencer::filterCutoffTargetChoice,
        raw.filterCutoff) * 0.01f));
    filterResonanceRamp.setTarget(juce::jlimit(0.0f, 1.0f, sequenced(
        crusher::sequencer::filterResonanceTargetChoice,
        raw.filterResonance) * 0.01f));
    compAmountRamp.setTarget(juce::jlimit(0.0f, 1.0f, sequenced(
        crusher::sequencer::compAmountTargetChoice,
        raw.compAmount) * 0.01f) * hump::Compressor::kMaxAmount);
    humpCompressor.setPhatMode(load(raw.compPhat) >= 0.5f);
    const bool filterOn = load(raw.filterEnabled) >= 0.5f;
    const bool compOn = load(raw.compEnabled) >= 0.5f;
    // Dispatch only the selected algorithm; during a short type-change
    // crossfade process the outgoing model too, never all 18 engines.
    const int selectedDistortion = juce::jlimit(
        0, distortionTypeCount - 1,
        static_cast<int>(std::lround(load(raw.distortionType))));
    if (selectedDistortion != currentDistortionType)
    {
        previousDistortionType = currentDistortionType;
        currentDistortionType = selectedDistortion;
        distortionTypeCrossfade.setCurrentAndTargetValue(0.0f);
        distortionTypeCrossfade.reset(currentSampleRate, smoothingSeconds);
        distortionTypeCrossfade.setTargetValue(1.0f);
    }
    const bool transitioningModels = previousDistortionType != currentDistortionType
        && distortionTypeCrossfade.isSmoothing();
    const bool distortionMayRun = load(raw.distortionEnabled) >= 0.5f
        || distortionEnabledValue.isSmoothing();
    const bool ampInUse = distortionMayRun && (currentDistortionType == 6
        || (transitioningModels && previousDistortionType == 6));
    const bool bumpyInUse = distortionMayRun && (currentDistortionType == 7
        || (transitioningModels && previousDistortionType == 7));
    const bool rectifyInUse = distortionMayRun && (currentDistortionType == 3
        || (transitioningModels && previousDistortionType == 3));
    const bool softClipInUse = distortionMayRun && (currentDistortionType == 0
        || (transitioningModels && previousDistortionType == 0));
    const bool tubeInUse = distortionMayRun && (currentDistortionType == 2
        || (transitioningModels && previousDistortionType == 2));
    const bool stressedInUse = distortionMayRun && (currentDistortionType == 12
        || (transitioningModels && previousDistortionType == 12));
    const bool saturationInUse = distortionMayRun && (currentDistortionType == 1
        || (transitioningModels && previousDistortionType == 1));
    const bool samuraiInUse = distortionMayRun && (currentDistortionType == 13
        || (transitioningModels && previousDistortionType == 13));
    const bool razorInUse = distortionMayRun && (currentDistortionType == 9
        || (transitioningModels && previousDistortionType == 9));
    const bool pixelatedInUse = distortionMayRun && (currentDistortionType == 10
        || (transitioningModels && previousDistortionType == 10));
    const bool lightningInUse = distortionMayRun && (currentDistortionType == 5
        || (transitioningModels && previousDistortionType == 5));
    const bool crackleInUse = distortionMayRun && (currentDistortionType == 4
        || (transitioningModels && previousDistortionType == 4));
    const bool freshInUse = distortionMayRun && (currentDistortionType == 14
        || (transitioningModels && previousDistortionType == 14));
    const bool fuzzInUse = distortionMayRun && (currentDistortionType == 16
        || (transitioningModels && previousDistortionType == 16));
    const bool hardClipInUse = distortionMayRun && (currentDistortionType == 15
        || (transitioningModels && previousDistortionType == 15));
    const bool combInUse = distortionMayRun && (currentDistortionType == 11
        || (transitioningModels && previousDistortionType == 11));
    const bool asymInUse = distortionMayRun && (currentDistortionType == 8
        || (transitioningModels && previousDistortionType == 8));
    const bool humpInUse = distortionMayRun
        && (currentDistortionType == humpDistortionType
            || (transitioningModels && previousDistortionType == humpDistortionType));
    if (ampInUse)
    {
        crusher::amp::Controls ampControls;
        ampControls.cabinet = juce::roundToInt(load(raw.ampCabinet));
        ampControls.softClip = juce::jlimit(0.0f, 1.0f, sequenced(
            crusher::sequencer::ampSoftClipTargetChoice, raw.ampSoftClip) * 0.01f);
        ampControls.topBoost = juce::jlimit(0.0f, 1.0f, sequenced(
            crusher::sequencer::ampTopBoostTargetChoice, raw.ampTopBoost) * 0.01f);
        ampControls.bias = juce::jlimit(0.0f, 1.0f, sequenced(
            crusher::sequencer::ampBiasTargetChoice, raw.ampBias) * 0.01f);
        ampSimulator.setControls(ampControls);
    }
    // Do not perform Bumpy DSP or advance its smoothers for inactive models.
    const auto updateBumpyControl = [bumpyInUse](LinearSmoother& smoother,
                                                 float target) noexcept
    {
        if (bumpyInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateBumpyControl(bumpyRippleValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::bumpyRippleTargetChoice, raw.bumpyRipple) * 0.01f));
    updateBumpyControl(bumpyRateValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::bumpyRateTargetChoice, raw.bumpyRate) * 0.01f));
    updateBumpyControl(bumpyContourValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::bumpyContourTargetChoice, raw.bumpyContour) * 0.01f));
    updateBumpyControl(bumpyBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::bumpyBiasTargetChoice, raw.bumpyBias) * 0.01f));
    updateBumpyControl(bumpyToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::bumpyToneTargetChoice, raw.bumpyTone) * 0.01f));
    // Idle Comb controls snap to the current values without DSP processing.
    const auto updateCombControl = [combInUse](LinearSmoother& smoother,
                                               float target) noexcept
    {
        if (combInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateCombControl(combTimeValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::combTimeTargetChoice, raw.combTime) * 0.01f));
    updateCombControl(combFeedbackValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::combFeedbackTargetChoice, raw.combFeedback) * 0.01f));
    updateCombControl(combDampValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::combDampTargetChoice, raw.combDamp) * 0.01f));
    updateCombControl(combSpreadValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::combSpreadTargetChoice, raw.combSpread) * 0.01f));
    updateCombControl(combToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::combToneTargetChoice, raw.combTone) * 0.01f));
    // Idle Crackle smoothers snap to target without running Crackle DSP.
    const auto updateCrackleControl = [crackleInUse](LinearSmoother& smoother,
                                                     float target) noexcept
    {
        if (crackleInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateCrackleControl(crackleDensityValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::crackleDensityTargetChoice, raw.crackleDensity) * 0.01f));
    updateCrackleControl(crackleBurstValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::crackleBurstTargetChoice, raw.crackleBurst) * 0.01f));
    updateCrackleControl(crackleDecayValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::crackleDecayTargetChoice, raw.crackleDecay) * 0.01f));
    updateCrackleControl(crackleGritValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::crackleGritTargetChoice, raw.crackleGrit) * 0.01f));
    updateCrackleControl(crackleToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::crackleToneTargetChoice, raw.crackleTone) * 0.01f));
    // Inactive Pixelated controls snap to their targets, without DSP or
    // per-sample smoother work. An outgoing model still runs during crossfade.
    const auto updatePixelatedControl = [pixelatedInUse](LinearSmoother& smoother,
                                                        float target) noexcept
    {
        if (pixelatedInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updatePixelatedControl(pixelatedRateValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::pixelatedRateTargetChoice, raw.pixelatedRate) * 0.01f));
    updatePixelatedControl(pixelatedBitsValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::pixelatedBitsTargetChoice, raw.pixelatedBits) * 0.01f));
    updatePixelatedControl(pixelatedDitherValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::pixelatedDitherTargetChoice, raw.pixelatedDither) * 0.01f));
    updatePixelatedControl(pixelatedJitterValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::pixelatedJitterTargetChoice, raw.pixelatedJitter) * 0.01f));
    updatePixelatedControl(pixelatedToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::pixelatedToneTargetChoice, raw.pixelatedTone) * 0.01f));

    // Rectify: active or outgoing only, with all six sequencer targets.
    const auto updateRectifyControl = [rectifyInUse](LinearSmoother& smoother,
                                                      float target) noexcept
    {
        if (rectifyInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateRectifyControl(rectifyWaveValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::rectifyWaveTargetChoice, raw.rectifyWave) * 0.01f));
    updateRectifyControl(rectifyBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::rectifyBiasTargetChoice, raw.rectifyBias) * 0.01f));
    updateRectifyControl(rectifyCurveValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::rectifyCurveTargetChoice, raw.rectifyCurve) * 0.01f));
    updateRectifyControl(rectifyFoldValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::rectifyFoldTargetChoice, raw.rectifyFold) * 0.01f));
    updateRectifyControl(rectifyToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::rectifyToneTargetChoice, raw.rectifyTone) * 0.01f));

    // Soft Clip: inactive controls do not advance per sample.
    const auto updateSoftClipControl = [softClipInUse](LinearSmoother& smoother,
                                                      float target) noexcept
    {
        if (softClipInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateSoftClipControl(softClipThresholdValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::softClipThresholdTargetChoice, raw.softClipThreshold) * 0.01f));
    updateSoftClipControl(softClipKneeValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::softClipKneeTargetChoice, raw.softClipKnee) * 0.01f));
    updateSoftClipControl(softClipBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::softClipBiasTargetChoice, raw.softClipBias) * 0.01f));
    updateSoftClipControl(softClipBodyValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::softClipBodyTargetChoice, raw.softClipBody) * 0.01f));
    updateSoftClipControl(softClipToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::softClipToneTargetChoice, raw.softClipTone) * 0.01f));

    // Tube: smoothing and DSP only for the active or outgoing Tube type.
    const auto updateTubeControl = [tubeInUse](LinearSmoother& smoother,
                                               float target) noexcept
    {
        if (tubeInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateTubeControl(tubeBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::tubeBiasTargetChoice, raw.tubeBias) * 0.01f));
    updateTubeControl(tubeHeatValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::tubeHeatTargetChoice, raw.tubeHeat) * 0.01f));
    updateTubeControl(tubeSagValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::tubeSagTargetChoice, raw.tubeSag) * 0.01f));
    updateTubeControl(tubeBodyValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::tubeBodyTargetChoice, raw.tubeBody) * 0.01f));
    updateTubeControl(tubeToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::tubeToneTargetChoice, raw.tubeTone) * 0.01f));

    // Stressed: inactive model controls snap to host values without audio processing.
    const auto updateStressedControl = [stressedInUse](LinearSmoother& smoother,
                                                    float target) noexcept
    {
        if (stressedInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateStressedControl(stressedPressValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::stressedPressTargetChoice, raw.stressedPress) * 0.01f));
    updateStressedControl(stressedFoldValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::stressedFoldTargetChoice, raw.stressedFold) * 0.01f));
    updateStressedControl(stressedTensionValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::stressedTensionTargetChoice, raw.stressedTension) * 0.01f));
    updateStressedControl(stressedBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::stressedBiasTargetChoice, raw.stressedBias) * 0.01f));
    updateStressedControl(stressedToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::stressedToneTargetChoice, raw.stressedTone) * 0.01f));

    // Saturation: no per-sample smoothing or DSP for inactive models.
    const auto updateSaturationControl = [saturationInUse](LinearSmoother& smoother,
                                                           float target) noexcept
    {
        if (saturationInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateSaturationControl(saturationCurveValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::saturationCurveTargetChoice, raw.saturationCurve) * 0.01f));
    updateSaturationControl(saturationBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::saturationBiasTargetChoice, raw.saturationBias) * 0.01f));
    updateSaturationControl(saturationWarmValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::saturationWarmTargetChoice, raw.saturationWarm) * 0.01f));
    updateSaturationControl(saturationBodyValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::saturationBodyTargetChoice, raw.saturationBody) * 0.01f));
    updateSaturationControl(saturationToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::saturationToneTargetChoice, raw.saturationTone) * 0.01f));

    // Samurai: smoothers run only for the selected or fading-out model.
    const auto updateSamuraiControl = [samuraiInUse](LinearSmoother& smoother,
                                                     float target) noexcept
    {
        if (samuraiInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateSamuraiControl(samuraiEdgeValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::samuraiEdgeTargetChoice, raw.samuraiEdge) * 0.01f));
    updateSamuraiControl(samuraiGateValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::samuraiGateTargetChoice, raw.samuraiGate) * 0.01f));
    updateSamuraiControl(samuraiBladeValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::samuraiBladeTargetChoice, raw.samuraiBlade) * 0.01f));
    updateSamuraiControl(samuraiRingValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::samuraiRingTargetChoice, raw.samuraiRing) * 0.01f));
    updateSamuraiControl(samuraiToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::samuraiToneTargetChoice, raw.samuraiTone) * 0.01f));

    // Razor smoothers advance only for the selected or outgoing model.
    const auto updateRazorControl = [razorInUse](LinearSmoother& smoother,
                                                float target) noexcept
    {
        if (razorInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateRazorControl(razorEdgeValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::razorEdgeTargetChoice, raw.razorEdge) * 0.01f));
    updateRazorControl(razorFreqValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::razorFreqTargetChoice, raw.razorFreq) * 0.01f));
    updateRazorControl(razorRingValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::razorRingTargetChoice, raw.razorRing) * 0.01f));
    updateRazorControl(razorBiteValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::razorBiteTargetChoice, raw.razorBite) * 0.01f));
    updateRazorControl(razorToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::razorToneTargetChoice, raw.razorTone) * 0.01f));

    // No Lightning smoother advancement while another model is selected.
    const auto updateLightningControl = [lightningInUse](LinearSmoother& smoother,
                                                        float target) noexcept
    {
        if (lightningInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateLightningControl(lightningSparkValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::lightningSparkTargetChoice, raw.lightningSpark) * 0.01f));
    updateLightningControl(lightningReactValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::lightningReactTargetChoice, raw.lightningReact) * 0.01f));
    updateLightningControl(lightningClipValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::lightningClipTargetChoice, raw.lightningClip) * 0.01f));
    updateLightningControl(lightningEdgeValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::lightningEdgeTargetChoice, raw.lightningEdge) * 0.01f));
    updateLightningControl(lightningToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::lightningToneTargetChoice, raw.lightningTone) * 0.01f));
    // Inactive Fresh gets no per-sample DSP or smoother work.
    const auto updateFreshControl = [freshInUse](LinearSmoother& smoother,
                                                 float target) noexcept
    {
        if (freshInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateFreshControl(freshFrequencyValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::freshFrequencyTargetChoice, raw.freshFrequency) * 0.01f));
    updateFreshControl(freshExciteValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::freshExciteTargetChoice, raw.freshExcite) * 0.01f));
    updateFreshControl(freshAirValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::freshAirTargetChoice, raw.freshAir) * 0.01f));
    updateFreshControl(freshSaturationValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::freshSaturationTargetChoice, raw.freshSaturation) * 0.01f));
    updateFreshControl(freshToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::freshToneTargetChoice, raw.freshTone) * 0.01f));
    // Inactive Fuzz is never processed and its smoothers remain idle.
    const auto updateFuzzControl = [fuzzInUse](LinearSmoother& smoother,
                                               float target) noexcept
    {
        if (fuzzInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateFuzzControl(fuzzGateValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::fuzzGateTargetChoice, raw.fuzzGate) * 0.01f));
    updateFuzzControl(fuzzBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::fuzzBiasTargetChoice, raw.fuzzBias) * 0.01f));
    updateFuzzControl(fuzzClipValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::fuzzClipTargetChoice, raw.fuzzClip) * 0.01f));
    updateFuzzControl(fuzzBodyValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::fuzzBodyTargetChoice, raw.fuzzBody) * 0.01f));
    updateFuzzControl(fuzzToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::fuzzToneTargetChoice, raw.fuzzTone) * 0.01f));
    // Hump extensions are idle when another distortion model is selected.
    const auto updateHumpControl = [humpInUse](LinearSmoother& smoother,
                                              float target) noexcept
    {
        if (humpInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateHumpControl(humpBodyValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::humpBodyTargetChoice, raw.humpBody) * 0.01f));
    updateHumpControl(humpBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::humpBiasTargetChoice, raw.humpBias) * 0.01f));
    updateHumpControl(humpFoldValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::humpFoldTargetChoice, raw.humpFold) * 0.01f));
    updateHumpControl(humpToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::humpToneTargetChoice, raw.humpTone) * 0.01f));
    // Inactive Hard Clip smoothers track their targets but do not run per sample.
    const auto updateHardClipControl = [hardClipInUse](LinearSmoother& smoother,
                                                       float target) noexcept
    {
        if (hardClipInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateHardClipControl(hardClipThresholdValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::hardClipThresholdTargetChoice, raw.hardClipThreshold) * 0.01f));
    updateHardClipControl(hardClipBiasValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::hardClipBiasTargetChoice, raw.hardClipBias) * 0.01f));
    updateHardClipControl(hardClipKneeValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::hardClipKneeTargetChoice, raw.hardClipKnee) * 0.01f));
    updateHardClipControl(hardClipFoldValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::hardClipFoldTargetChoice, raw.hardClipFold) * 0.01f));
    updateHardClipControl(hardClipToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::hardClipToneTargetChoice, raw.hardClipTone) * 0.01f));
    // Idle Asym controls track parameter/modulation updates without
    // running any Asym DSP; on selection their smoothed values are ready.
    const auto updateAsymControl = [asymInUse](LinearSmoother& smoother,
                                               float target) noexcept
    {
        if (asymInUse)
            smoother.setTargetValue(target);
        else
            smoother.setCurrentAndTargetValue(target);
    };
    updateAsymControl(asymBiasValue, juce::jlimit(-1.0f, 1.0f,
        sequenced(crusher::sequencer::asymBiasTargetChoice, raw.asymBias) * 0.01f));
    updateAsymControl(asymSkewValue, juce::jlimit(-1.0f, 1.0f,
        sequenced(crusher::sequencer::asymSkewTargetChoice, raw.asymSkew) * 0.01f));
    updateAsymControl(asymShapeValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::asymShapeTargetChoice, raw.asymShape) * 0.01f));
    updateAsymControl(asymToneValue, juce::jlimit(0.0f, 1.0f,
        sequenced(crusher::sequencer::asymToneTargetChoice, raw.asymTone) * 0.01f));
    graphicEQEnabledValue.setTargetValue(
        load(raw.eqEnabled) >= 0.5f ? 1.0f : 0.0f);
    const bool monoDistInput = load(raw.distMode) >= 0.5f;
    const int eqMode = juce::jlimit(0, 2, juce::roundToInt(load(raw.eqMode)));
    std::array<crusher::eq::Band, crusher::eq::bandCount> eqBands {};
    for (int band = 0; band < crusher::eq::bandCount; ++band)
    {
        auto& b = eqBands[static_cast<size_t>(band)];
        b.frequency = load(raw.eqFrequency[static_cast<size_t>(band)]);
        b.gainDb = load(raw.eqGain[static_cast<size_t>(band)]);
        b.q = load(raw.eqQ[static_cast<size_t>(band)]);
        b.shape = juce::jlimit(0, 5,
            juce::roundToInt(load(raw.eqShape[static_cast<size_t>(band)])));
    }
    graphicEQ.update(eqBands);


    std::array<crusher::sequencer::GateStepMode,
               crusher::sequencer::stepCount> gateSteps {};
    for (int step = 0; step < crusher::sequencer::stepCount; ++step)
        gateSteps[static_cast<size_t>(step)] =
            crusher::sequencer::gateStepModeFromChoice(
                load(raw.sequencerGateSteps[static_cast<size_t>(step)]));
    const float shortLength = juce::jlimit(0.10f, 0.60f, sequenced(
        crusher::sequencer::sequencerGateShortTargetChoice,
        raw.sequencerGateShortLength));
    const float longLength = juce::jlimit(0.65f, 0.95f, sequenced(
        crusher::sequencer::sequencerGateLongTargetChoice,
        raw.sequencerGateLongLength));
    const int gateMode = juce::jlimit(
        0, 3, juce::roundToInt(load(raw.sequencerGateMode)));
    const float gateSmoothing = 1.0f - std::exp(
        -1.0f / static_cast<float>(0.002 * currentSampleRate));

    auto* left = buffer.getWritePointer(0);
    auto* right = activeChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    // Fixed 64-frame chunks avoid heap allocations on the audio thread and
    // preserve Hump Crusher's filter/compressor processing cadence.
    constexpr int chunkCapacity = 64;
    std::array<float, chunkCapacity> wetLeft {}, wetRight {};
    std::array<float, chunkCapacity> dryLeft {}, dryRight {};
    std::array<float, chunkCapacity> gateGains {};
    for (int start = 0; start < sampleCount; start += chunkCapacity)
    {
        const int count = juce::jmin(chunkCapacity, sampleCount - start);
        const float mech = mechRamp.advance(count);
        const float tube = tubeRamp.advance(count);
        const float cutoff = filterCutoffRamp.advance(count);
        const float resonance = filterResonanceRamp.advance(count);
        const float compAmount = compAmountRamp.advance(count);
        // Construct Hump waveshapers only for Hump or its outgoing fade.
        const std::optional<hump::distortion::MechShape> mechShape =
            humpInUse && mech > 0.0f
                ? std::optional<hump::distortion::MechShape>(std::in_place, mech)
                : std::nullopt;
        const std::optional<hump::distortion::TubeShape> tubeShape =
            humpInUse && tube > 0.0f
                ? std::optional<hump::distortion::TubeShape>(std::in_place, tube)
                : std::nullopt;

        // Each selected distortion is a single effect. Hump uses the
        // original Mech -> Filter -> Tube route; template types use their
        // own DIST waveshaper -> Filter route. The one filter state is never
        // duplicated or reset by a type change.
        std::array<float, chunkCapacity> humpTubeBlends {};
        std::array<float, chunkCapacity> eqBlends {};
        std::array<float, chunkCapacity> distortionBlends {};
        for (int i = 0; i < count; ++i)
        {
            const int sample = start + i;
            const float dryL = left[sample];
            const float dryR = right != nullptr ? right[sample] : dryL;
            dryLeft[static_cast<size_t>(i)] = dryL;
            dryRight[static_cast<size_t>(i)] = dryR;
            float gateTarget = 1.0f;
            if (sequenceEnabled && gateMode != 3)
            {
                const auto position = crusher::sequencer::positionForPhase(
                    gatePhaseAtBlockStart
                        + static_cast<double>(sample) * phaseIncrement,
                    lastStep - firstStep + 1, direction);
                gateTarget = crusher::sequencer::gateStepTarget(
                    gateSteps[static_cast<size_t>(firstStep + position.current)],
                    position.fraction, shortLength, longLength);
            }
            sequencerGateGain += gateSmoothing
                * (gateTarget - sequencerGateGain);
            sequencerGateGain = juce::jlimit(
                0.0f, 1.0f, sequencerGateGain);
            gateGains[static_cast<size_t>(i)] = sequencerGateGain;
            const float safeL = std::isfinite(dryL) ? dryL : 0.0f;
            const float safeR = std::isfinite(dryR) ? dryR : 0.0f;
            float inputL = gateMode == 0
                ? safeL * sequencerGateGain : safeL;
            float inputR = gateMode == 0
                ? safeR * sequencerGateGain : safeR;
            const float eqBlend = graphicEQEnabledValue.getNextValue();
            eqBlends[static_cast<size_t>(i)] = eqBlend;
            if (eqBlend > 0.0f && eqMode != crusher::eq::post)
            {
                const float shapedL = graphicEQ.processPre(inputL, 0);
                const float shapedR = graphicEQ.processPre(inputR, 1);
                inputL += (shapedL - inputL) * eqBlend;
                inputR += (shapedR - inputR) * eqBlend;
            }

            // Summing is for the distortion input only. The unprocessed wet
            // signal remains stereo if the DIST power LED is off or fading.
            float distortionInputL = inputL;
            float distortionInputR = inputR;
            if (monoDistInput)
            {
                const float summed = 0.5f * (inputL + inputR);
                distortionInputL = summed;
                distortionInputR = summed;
            }
            const float drive = driveValue.getNextValue();
            // Advance once per stereo sample so both channels use the same
            // click-smoothed Asym controls, including during model crossfade.
            if (bumpyInUse)
            {
                crusher::bumpy::Controls bumpyControls;
                bumpyControls.ripple = bumpyRippleValue.getNextValue();
                bumpyControls.rate = bumpyRateValue.getNextValue();
                bumpyControls.contour = bumpyContourValue.getNextValue();
                bumpyControls.bias = bumpyBiasValue.getNextValue();
                bumpyControls.tone = bumpyToneValue.getNextValue();
                bumpyProcessor.setControls(bumpyControls);
            }
            if (crackleInUse)
            {
                crusher::crackle::Controls crackleControls;
                crackleControls.density = crackleDensityValue.getNextValue();
                crackleControls.burst = crackleBurstValue.getNextValue();
                crackleControls.decay = crackleDecayValue.getNextValue();
                crackleControls.grit = crackleGritValue.getNextValue();
                crackleControls.tone = crackleToneValue.getNextValue();
                crackleProcessor.setControls(crackleControls);
            }
            if (pixelatedInUse)
            {
                crusher::pixelated::Controls controls;
                controls.rate = pixelatedRateValue.getNextValue();
                controls.bits = pixelatedBitsValue.getNextValue();
                controls.dither = pixelatedDitherValue.getNextValue();
                controls.jitter = pixelatedJitterValue.getNextValue();
                controls.tone = pixelatedToneValue.getNextValue();
                pixelatedProcessor.setControls(controls);
            }
            if (rectifyInUse)
            {
                crusher::rectify::Controls controls;
                controls.wave = rectifyWaveValue.getNextValue();
                controls.bias = rectifyBiasValue.getNextValue();
                controls.curve = rectifyCurveValue.getNextValue();
                controls.fold = rectifyFoldValue.getNextValue();
                controls.tone = rectifyToneValue.getNextValue();
                rectifyProcessor.setControls(controls);
            }
            if (softClipInUse)
            {
                crusher::softclip::Controls controls;
                controls.threshold = softClipThresholdValue.getNextValue();
                controls.knee = softClipKneeValue.getNextValue();
                controls.bias = softClipBiasValue.getNextValue();
                controls.body = softClipBodyValue.getNextValue();
                controls.tone = softClipToneValue.getNextValue();
                softClipProcessor.setControls(controls);
            }
            if (tubeInUse)
            {
                crusher::tube::Controls controls;
                controls.bias = tubeBiasValue.getNextValue();
                controls.heat = tubeHeatValue.getNextValue();
                controls.sag = tubeSagValue.getNextValue();
                controls.body = tubeBodyValue.getNextValue();
                controls.tone = tubeToneValue.getNextValue();
                tubeProcessor.setControls(controls);
            }
            if (stressedInUse)
            {
                crusher::stressed::Controls controls;
                controls.press = stressedPressValue.getNextValue();
                controls.fold = stressedFoldValue.getNextValue();
                controls.tension = stressedTensionValue.getNextValue();
                controls.bias = stressedBiasValue.getNextValue();
                controls.tone = stressedToneValue.getNextValue();
                stressedProcessor.setControls(controls);
            }
            if (saturationInUse)
            {
                crusher::saturation::Controls controls;
                controls.curve = saturationCurveValue.getNextValue();
                controls.bias = saturationBiasValue.getNextValue();
                controls.warm = saturationWarmValue.getNextValue();
                controls.body = saturationBodyValue.getNextValue();
                controls.tone = saturationToneValue.getNextValue();
                saturationProcessor.setControls(controls);
            }
            if (samuraiInUse)
            {
                crusher::samurai::Controls controls;
                controls.edge = samuraiEdgeValue.getNextValue();
                controls.gate = samuraiGateValue.getNextValue();
                controls.blade = samuraiBladeValue.getNextValue();
                controls.ring = samuraiRingValue.getNextValue();
                controls.tone = samuraiToneValue.getNextValue();
                samuraiProcessor.setControls(controls);
            }
            if (razorInUse)
            {
                crusher::razor::Controls controls;
                controls.edge = razorEdgeValue.getNextValue();
                controls.freq = razorFreqValue.getNextValue();
                controls.ring = razorRingValue.getNextValue();
                controls.bite = razorBiteValue.getNextValue();
                controls.tone = razorToneValue.getNextValue();
                razorProcessor.setControls(controls);
            }
            if (lightningInUse)
            {
                crusher::lightning::Controls controls;
                controls.spark = lightningSparkValue.getNextValue();
                controls.react = lightningReactValue.getNextValue();
                controls.clip = lightningClipValue.getNextValue();
                controls.edge = lightningEdgeValue.getNextValue();
                controls.tone = lightningToneValue.getNextValue();
                lightningProcessor.setControls(controls);
            }
            if (freshInUse)
            {
                crusher::fresh::Controls freshControls;
                freshControls.frequency = freshFrequencyValue.getNextValue();
                freshControls.excite = freshExciteValue.getNextValue();
                freshControls.air = freshAirValue.getNextValue();
                freshControls.saturation = freshSaturationValue.getNextValue();
                freshControls.tone = freshToneValue.getNextValue();
                freshProcessor.setControls(freshControls);
            }
            if (fuzzInUse)
            {
                crusher::fuzz::Controls fuzzControls;
                fuzzControls.gate = fuzzGateValue.getNextValue();
                fuzzControls.bias = fuzzBiasValue.getNextValue();
                fuzzControls.clip = fuzzClipValue.getNextValue();
                fuzzControls.body = fuzzBodyValue.getNextValue();
                fuzzControls.tone = fuzzToneValue.getNextValue();
                fuzzProcessor.setControls(fuzzControls);
            }
            if (hardClipInUse)
            {
                crusher::hardclip::Controls controls;
                controls.threshold = hardClipThresholdValue.getNextValue();
                controls.bias = hardClipBiasValue.getNextValue();
                controls.knee = hardClipKneeValue.getNextValue();
                controls.fold = hardClipFoldValue.getNextValue();
                controls.tone = hardClipToneValue.getNextValue();
                hardClipProcessor.setControls(controls);
            }
            if (combInUse)
            {
                crusher::comb::Controls combControls;
                combControls.time = combTimeValue.getNextValue();
                combControls.feedback = combFeedbackValue.getNextValue();
                combControls.damp = combDampValue.getNextValue();
                combControls.spread = combSpreadValue.getNextValue();
                combControls.tone = combToneValue.getNextValue();
                combProcessor.setControls(combControls);
            }
            if (asymInUse)
            {
                crusher::asym::Controls asymControls;
                asymControls.bias = asymBiasValue.getNextValue();
                asymControls.skew = asymSkewValue.getNextValue();
                asymControls.shape = asymShapeValue.getNextValue();
                asymControls.tone = asymToneValue.getNextValue();
                asymProcessor.setControls(asymControls);
            }
            if (humpInUse)
            {
                crusher::humpmodel::Controls controls;
                controls.body = humpBodyValue.getNextValue();
                controls.bias = humpBiasValue.getNextValue();
                controls.fold = humpFoldValue.getNextValue();
                controls.tone = humpToneValue.getNextValue();
                humpProcessor.setControls(controls);
            }
            const float typeBlend = distortionTypeCrossfade.getNextValue();
            const float distortionOnAmount = distortionEnabledValue.getNextValue();
            distortionBlends[static_cast<size_t>(i)] = distortionOnAmount;
            const bool changingType = previousDistortionType != currentDistortionType
                && typeBlend < 1.0f;
            float shapedL = inputL;
            float shapedR = inputR;
            if (distortionOnAmount > 0.0f)
            {
                const auto preFilterShape = [&](float sampleValue,
                                                 int channel, int type) noexcept
                {
                    if (type == humpDistortionType)
                    {
                        const float humpInput = humpProcessor.processPre(sampleValue, channel);
                        return mechShape ? (*mechShape)(humpInput) : humpInput;
                    }
                    return processDistortionType(sampleValue, drive, channel, type);
                };
                shapedL = preFilterShape(distortionInputL, 0, currentDistortionType);
                shapedR = preFilterShape(distortionInputR, 1, currentDistortionType);
                // Keep Amp's original pre-distortion Mono sound, including
                // its single processed channel being copied to both sides.
                if (monoDistInput && currentDistortionType == 6)
                    shapedR = shapedL;
                if (changingType)
                {
                    const float oldL = preFilterShape(distortionInputL, 0, previousDistortionType);
                    float oldR = preFilterShape(distortionInputR, 1, previousDistortionType);
                    if (monoDistInput && previousDistortionType == 6)
                        oldR = oldL;
                    shapedL = oldL + (shapedL - oldL) * typeBlend;
                    shapedR = oldR + (shapedR - oldR) * typeBlend;
                }
            }
            wetLeft[static_cast<size_t>(i)] = inputL
                + (shapedL - inputL) * distortionOnAmount;
            wetRight[static_cast<size_t>(i)] = inputR
                + (shapedR - inputR) * distortionOnAmount;

            // No Hump modulation or tube blend work for non-Hump types.
            if (humpInUse)
            {
                float humpFraction = currentDistortionType == humpDistortionType
                    ? 1.0f : 0.0f;
                if (changingType)
                {
                    const float oldHump = previousDistortionType == humpDistortionType
                        ? 1.0f : 0.0f;
                    humpFraction = oldHump + (humpFraction - oldHump) * typeBlend;
                }
                humpTubeBlends[static_cast<size_t>(i)] =
                    humpFraction * distortionOnAmount;
            }
        }

        // Original Hump resonant low-pass and stereo-linked fat limiter.
        // It receives the output of the selected model's pre-filter stage.
        if (filterOn)
            humpFilter.process(wetLeft.data(), wetRight.data(),
                               count, cutoff, resonance);

        // Hump Tube follows the filter exactly as in HumpCrusher; no extra
        // template distortion is inserted between those two stages.
        if (humpInUse)
            for (int i = 0; i < count; ++i)
            {
                const float blend = humpTubeBlends[static_cast<size_t>(i)];
                if (blend <= 0.0f)
                    continue;
                auto& l = wetLeft[static_cast<size_t>(i)];
                auto& r = wetRight[static_cast<size_t>(i)];
                const float newL = humpProcessor.processPost(
                    tubeShape ? (*tubeShape)(l) : l, 0);
                const float newR = humpProcessor.processPost(
                    tubeShape ? (*tubeShape)(r) : r, 1);
                l += (newL - l) * blend;
                r += (newR - r) * blend;
            }

        // Shared distortion OUTPUT affects the wet effect path only. At 0 dB,
        // skip multiplication entirely to preserve the original neutral sound.
        for (int i = 0; i < count; ++i)
        {
            const float outputGain = distOutputGainValue.getNextValue();
            const float gain = 1.0f + (outputGain - 1.0f)
                * distortionBlends[static_cast<size_t>(i)];
            if (gain != 1.0f)
            {
                wetLeft[static_cast<size_t>(i)] *= gain;
                wetRight[static_cast<size_t>(i)] *= gain;
            }
        }

        // POST shapes the resulting distortion/filter signal. EMPH uses
        // stable complementary processing: exact inverse for bell/shelves,
        // limited correction for non-invertible notch/cut shapes.
        if (eqMode != crusher::eq::pre)
            for (int i = 0; i < count; ++i)
            {
                const float blend = eqBlends[static_cast<size_t>(i)];
                if (blend <= 0.0f)
                    continue;
                auto& l = wetLeft[static_cast<size_t>(i)];
                auto& r = wetRight[static_cast<size_t>(i)];
                const bool inverse = eqMode == crusher::eq::emphasis;
                const float newL = graphicEQ.processPost(l, 0, inverse);
                const float newR = graphicEQ.processPost(r, 1, inverse);
                l += (newL - l) * blend;
                r += (newR - r) * blend;
            }

        // Original Hump stereo-linked drive/soft-limit compressor.
        if (compOn)
            humpCompressor.process(wetLeft.data(), wetRight.data(),
                                   count, compAmount);

        // Rejoin the original template's sequencer Gate, Mix, Output and
        // smoothed host bypass. All original master behaviour is retained.
        for (int i = 0; i < count; ++i)
        {
            const int sample = start + i;
            const float dryL = dryLeft[static_cast<size_t>(i)];
            const float dryR = dryRight[static_cast<size_t>(i)];
            const float gate = gateGains[static_cast<size_t>(i)];
            const float wetL = wetLeft[static_cast<size_t>(i)]
                * (gateMode == 1 ? gate : 1.0f);
            const float wetR = wetRight[static_cast<size_t>(i)]
                * (gateMode == 1 ? gate : 1.0f);
            const float mix = mixValue.getNextValue();
            const float gain = outputGainValue.getNextValue();
            float processedL = gain * (dryL * (1.0f - mix) + wetL * mix);
            float processedR = gain * (dryR * (1.0f - mix) + wetR * mix);
            if (gateMode == 2)
            {
                processedL *= gate;
                processedR *= gate;
            }
            // The original Hump master clamps the processed output to
            // +/-0.9999. This also catches compressor detector overshoots
            // before they reach the host. The bypassed dry path stays dry.
            processedL = juce::jlimit(-0.9999f, 0.9999f, processedL);
            processedR = juce::jlimit(-0.9999f, 0.9999f, processedR);
            const float bypass = bypassValue.getNextValue();
            left[sample] = sanitiseSample(
                processedL + (dryL - processedL) * bypass, 64.0f);
            if (right != nullptr)
                right[sample] = sanitiseSample(
                    processedR + (dryR - processedR) * bypass, 64.0f);
        }
    }
    if (! distortionTypeCrossfade.isSmoothing())
        previousDistortionType = currentDistortionType;
}

void CrusherAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty("schemaVersion", stateSchemaVersion, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destination);
}

void CrusherAudioProcessor::setStateInformation(const void* data, int size)
{
    const auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || ! xml->hasTagName("CrusherParameters"))
        return;
    auto state = juce::ValueTree::fromXml(*xml);
    if (static_cast<int>(state.getProperty("schemaVersion", 0))
        != stateSchemaVersion)
        return;
    setSavedParameterState(state);
    parameters.replaceState(state);
    pendingProcessingReset.store(true, std::memory_order_release);
}

juce::AudioProcessorEditor* CrusherAudioProcessor::createEditor()
{
    return new CrusherAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrusherAudioProcessor();
}
