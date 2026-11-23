#pragma once

#include <JuceHeader.h>
#include "CrusherSequencer.h"
#include "HumpDSP/Distortion.h"
#include "HumpDSP/Filter.h"
#include "HumpDSP/Compressor.h"
#include "HumpDSP/ParamRamp.h"
#include "CrusherAmpSim.h"
#include "CrusherAsym.h"
#include "CrusherBumpy.h"
#include "CrusherComb.h"
#include "CrusherCrackle.h"
#include "CrusherDiodeClip.h"
#include "CrusherFresh.h"
#include "CrusherFuzz.h"
#include "CrusherHardClip.h"
#include "CrusherHump.h"
#include "CrusherLightning.h"
#include "CrusherPixelated.h"
#include "CrusherRazor.h"
#include "CrusherRectify.h"
#include "CrusherSamurai.h"
#include "CrusherSaturation.h"
#include "CrusherSoftClip.h"
#include "CrusherStressed.h"
#include "CrusherTube.h"
#include "CrusherGraphicEQ.h"
#include <array>
#include <optional>

class CrusherAudioProcessor final : public juce::AudioProcessor
{
public:
    CrusherAudioProcessor();
    ~CrusherAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return parameters.getParameter("bypass");
    }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();
    static juce::File getDataDirectory();
    static juce::File getSettingsDirectory();
    static juce::File getPresetsDirectory();
    static juce::File getSequencesDirectory();
    juce::Result savePresetToFile(const juce::File&);
    juce::Result loadPresetFromFile(const juce::File&);
    juce::File getCurrentPresetFile() const;
    std::optional<float> getSavedParameterValue(
        const juce::String& parameterId) const;
    int getActiveSequencerStep() const noexcept
    {
        return activeSequencerStep.load(std::memory_order_relaxed);
    }
    std::optional<float> getSequencerAnimatedNormalised(
        const juce::String& parameterId) const noexcept;

private:
    static constexpr int stateSchemaVersion = 3;
    static constexpr int channelCount = 2;
    static constexpr int distortionTypeCount = 19;
    static constexpr int humpDistortionType = 17;

    using LinearSmoother = juce::SmoothedValue<
        float, juce::ValueSmoothingTypes::Linear>;

    struct ParameterPointers
    {
        std::atomic<float>* distortionType = nullptr;
        std::atomic<float>* distortionEnabled = nullptr;
        std::atomic<float>* ampCabinet = nullptr;
        std::atomic<float>* distMode = nullptr;
        std::atomic<float>* distOutput = nullptr;
        std::atomic<float>* ampSoftClip = nullptr;
        std::atomic<float>* ampTopBoost = nullptr;
        std::atomic<float>* ampBias = nullptr;
        std::atomic<float>* bumpyRipple = nullptr;
        std::atomic<float>* bumpyRate = nullptr;
        std::atomic<float>* bumpyContour = nullptr;
        std::atomic<float>* bumpyBias = nullptr;
        std::atomic<float>* bumpyTone = nullptr;
        std::atomic<float>* combTime = nullptr;
        std::atomic<float>* combFeedback = nullptr;
        std::atomic<float>* combDamp = nullptr;
        std::atomic<float>* combSpread = nullptr;
        std::atomic<float>* combTone = nullptr;
        std::atomic<float>* crackleDensity = nullptr;
        std::atomic<float>* crackleBurst = nullptr;
        std::atomic<float>* crackleDecay = nullptr;
        std::atomic<float>* crackleGrit = nullptr;
        std::atomic<float>* crackleTone = nullptr;
        std::atomic<float>* diodeInputHP = nullptr;
        std::atomic<float>* diodeEnvelope = nullptr;
        std::atomic<float>* diodeShape = nullptr;
        std::atomic<float>* diodeOutputHP = nullptr;
        std::atomic<float>* diodeOutputLP = nullptr;
        std::atomic<float>* lightningSpark = nullptr;
        std::atomic<float>* lightningReact = nullptr;
        std::atomic<float>* lightningClip = nullptr;
        std::atomic<float>* lightningEdge = nullptr;
        std::atomic<float>* lightningTone = nullptr;
        std::atomic<float>* pixelatedRate = nullptr;
        std::atomic<float>* pixelatedBits = nullptr;
        std::atomic<float>* pixelatedDither = nullptr;
        std::atomic<float>* pixelatedJitter = nullptr;
        std::atomic<float>* pixelatedTone = nullptr;
        std::atomic<float>* rectifyWave = nullptr;
        std::atomic<float>* rectifyBias = nullptr;
        std::atomic<float>* rectifyCurve = nullptr;
        std::atomic<float>* rectifyFold = nullptr;
        std::atomic<float>* rectifyTone = nullptr;
        std::atomic<float>* softClipThreshold = nullptr;
        std::atomic<float>* softClipKnee = nullptr;
        std::atomic<float>* softClipBias = nullptr;
        std::atomic<float>* softClipBody = nullptr;
        std::atomic<float>* softClipTone = nullptr;
        std::atomic<float>* tubeBias = nullptr;
        std::atomic<float>* tubeHeat = nullptr;
        std::atomic<float>* tubeSag = nullptr;
        std::atomic<float>* tubeBody = nullptr;
        std::atomic<float>* tubeTone = nullptr;
        std::atomic<float>* stressedPress = nullptr;
        std::atomic<float>* stressedFold = nullptr;
        std::atomic<float>* stressedTension = nullptr;
        std::atomic<float>* stressedBias = nullptr;
        std::atomic<float>* stressedTone = nullptr;
        std::atomic<float>* saturationCurve = nullptr;
        std::atomic<float>* saturationBias = nullptr;
        std::atomic<float>* saturationWarm = nullptr;
        std::atomic<float>* saturationBody = nullptr;
        std::atomic<float>* saturationTone = nullptr;
        std::atomic<float>* samuraiEdge = nullptr;
        std::atomic<float>* samuraiGate = nullptr;
        std::atomic<float>* samuraiBlade = nullptr;
        std::atomic<float>* samuraiRing = nullptr;
        std::atomic<float>* samuraiTone = nullptr;
        std::atomic<float>* razorEdge = nullptr;
        std::atomic<float>* razorFreq = nullptr;
        std::atomic<float>* razorRing = nullptr;
        std::atomic<float>* razorBite = nullptr;
        std::atomic<float>* razorTone = nullptr;
        std::atomic<float>* freshFrequency = nullptr;
        std::atomic<float>* freshExcite = nullptr;
        std::atomic<float>* freshAir = nullptr;
        std::atomic<float>* freshSaturation = nullptr;
        std::atomic<float>* freshTone = nullptr;
        std::atomic<float>* fuzzGate = nullptr;
        std::atomic<float>* fuzzBias = nullptr;
        std::atomic<float>* fuzzClip = nullptr;
        std::atomic<float>* fuzzBody = nullptr;
        std::atomic<float>* fuzzTone = nullptr;
        std::atomic<float>* hardClipThreshold = nullptr;
        std::atomic<float>* hardClipBias = nullptr;
        std::atomic<float>* hardClipKnee = nullptr;
        std::atomic<float>* hardClipFold = nullptr;
        std::atomic<float>* hardClipTone = nullptr;
        std::atomic<float>* asymBias = nullptr;
        std::atomic<float>* asymSkew = nullptr;
        std::atomic<float>* asymShape = nullptr;
        std::atomic<float>* asymTone = nullptr;
        std::atomic<float>* eqEnabled = nullptr;
        std::atomic<float>* eqMode = nullptr;
        std::array<std::atomic<float>*, crusher::eq::bandCount> eqFrequency {};
        std::array<std::atomic<float>*, crusher::eq::bandCount> eqGain {};
        std::array<std::atomic<float>*, crusher::eq::bandCount> eqQ {};
        std::array<std::atomic<float>*, crusher::eq::bandCount> eqShape {};
        std::atomic<float>* mech = nullptr;
        std::atomic<float>* tube = nullptr;
        std::atomic<float>* humpBody = nullptr;
        std::atomic<float>* humpBias = nullptr;
        std::atomic<float>* humpFold = nullptr;
        std::atomic<float>* humpTone = nullptr;
        std::atomic<float>* filterEnabled = nullptr;
        std::atomic<float>* filterCutoff = nullptr;
        std::atomic<float>* filterResonance = nullptr;
        std::atomic<float>* compEnabled = nullptr;
        std::atomic<float>* compAmount = nullptr;
        std::atomic<float>* compPhat = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* sequencerEnabled = nullptr;
        std::atomic<float>* sequencerHostSync = nullptr;
        std::atomic<float>* sequencerRate = nullptr;
        std::atomic<float>* sequencerStart = nullptr;
        std::atomic<float>* sequencerEnd = nullptr;
        std::atomic<float>* sequencerDirection = nullptr;
        std::atomic<float>* sequencerBipolar = nullptr;
        std::atomic<float>* sequencerUnipolarNegative = nullptr;
        std::atomic<float>* sequencerAttack = nullptr;
        std::atomic<float>* sequencerRelease = nullptr;
        std::atomic<float>* sequencerPeak = nullptr;
        std::atomic<float>* sequencerMove = nullptr;
        std::atomic<float>* sequencerGateShortLength = nullptr;
        std::atomic<float>* sequencerGateLongLength = nullptr;
        std::atomic<float>* sequencerGateMode = nullptr;
        std::array<std::atomic<float>*, crusher::sequencer::stepCount>
            sequencerSteps {};
        std::array<std::atomic<float>*, crusher::sequencer::stepCount>
            sequencerGateSteps {};
        std::array<std::atomic<float>*, crusher::sequencer::targetSlotCount>
            sequencerTargets {};
        std::array<std::atomic<float>*, crusher::sequencer::targetSlotCount>
            sequencerTargetEnabled {};
        std::array<std::atomic<float>*, crusher::sequencer::targetSlotCount>
            sequencerTargetDepth {};
    } raw;

    void bindParameterPointers();
    void setSavedParameterState(const juce::ValueTree&);
    static float load(const std::atomic<float>*) noexcept;
    bool isSequencerTargetAssigned(int targetChoice) const noexcept;
    float sequencerTargetDepth(int targetChoice) const noexcept;
    float sequencerModulatedValue(
        int targetChoice, float baseValue, float canonicalSequenceValue,
        bool bipolar, bool unipolarNegative,
        juce::RangedAudioParameter* valueParameter = nullptr) const noexcept;

    float processDistortionType(float sample, float amount, int channel,
                                int distortionType) noexcept;
    float processRectifyDrive(float sample, float amount, int channel) noexcept;
    float processCrackleDrive(float sample, float amount, int channel) noexcept;
    float processLightningDrive(float sample, float amount, int channel) noexcept;
    float processAmpDrive(float sample, float amount, int channel) noexcept;
    float processBumpyDrive(float sample, float amount, int channel) noexcept;
    float processAsymmetricalDrive(float sample, float amount,
                                   int channel) noexcept;
    float processRazorDrive(float sample, float amount, int channel) noexcept;
    float processPixelatedDrive(float sample, float amount,
                                int channel) noexcept;
    float processCombDrive(float sample, float amount, int channel) noexcept;
    float processStressedDrive(float sample, float amount,
                               int channel) noexcept;
    float processSamuraiDrive(float sample, float amount, int channel) noexcept;
    float processFreshDrive(float sample, float amount, int channel) noexcept;
    float processHardClipDrive(float sample, float amount, int channel) noexcept;
    float processFuzzDrive(float sample, float amount, int channel) noexcept;
    void resetDistortionState() noexcept;
    static float nextNoiseSample(uint32_t& state) noexcept;
    static float sanitiseSample(float sample, float limit) noexcept;

    std::atomic<int> activeSequencerStep { -1 };
    double sequencerFreePhase = 0.0;
    int sequencerHeldNotes = 0;
    float previousSequencerCanonical = 1.0f;
    float sequencerGateGain = 1.0f;
    std::array<juce::RangedAudioParameter*, crusher::sequencer::targets.size()>
        sequencerTargetParameters {};
    mutable std::array<std::atomic<float>, crusher::sequencer::targets.size()>
        sequencerAnimatedNormalised {};

    double currentSampleRate = 44100.0;
    // The original Hump Crusher DSP units, retained as independent stages.
    crusher::amp::AmpSim ampSimulator;
    crusher::bumpy::Processor bumpyProcessor;
    LinearSmoother bumpyRippleValue;
    LinearSmoother bumpyRateValue;
    LinearSmoother bumpyContourValue;
    LinearSmoother bumpyBiasValue;
    LinearSmoother bumpyToneValue;
    crusher::rectify::Processor rectifyProcessor;
    LinearSmoother rectifyWaveValue;
    LinearSmoother rectifyBiasValue;
    LinearSmoother rectifyCurveValue;
    LinearSmoother rectifyFoldValue;
    LinearSmoother rectifyToneValue;
    crusher::softclip::Processor softClipProcessor;
    LinearSmoother softClipThresholdValue;
    LinearSmoother softClipKneeValue;
    LinearSmoother softClipBiasValue;
    LinearSmoother softClipBodyValue;
    LinearSmoother softClipToneValue;
    crusher::tube::Processor tubeProcessor;
    LinearSmoother tubeBiasValue;
    LinearSmoother tubeHeatValue;
    LinearSmoother tubeSagValue;
    LinearSmoother tubeBodyValue;
    LinearSmoother tubeToneValue;
    crusher::stressed::Processor stressedProcessor;
    LinearSmoother stressedPressValue;
    LinearSmoother stressedFoldValue;
    LinearSmoother stressedTensionValue;
    LinearSmoother stressedBiasValue;
    LinearSmoother stressedToneValue;
    crusher::saturation::Processor saturationProcessor;
    LinearSmoother saturationCurveValue;
    LinearSmoother saturationBiasValue;
    LinearSmoother saturationWarmValue;
    LinearSmoother saturationBodyValue;
    LinearSmoother saturationToneValue;
    crusher::samurai::Processor samuraiProcessor;
    LinearSmoother samuraiEdgeValue;
    LinearSmoother samuraiGateValue;
    LinearSmoother samuraiBladeValue;
    LinearSmoother samuraiRingValue;
    LinearSmoother samuraiToneValue;
    crusher::razor::Processor razorProcessor;
    LinearSmoother razorEdgeValue;
    LinearSmoother razorFreqValue;
    LinearSmoother razorRingValue;
    LinearSmoother razorBiteValue;
    LinearSmoother razorToneValue;
    crusher::pixelated::Processor pixelatedProcessor;
    LinearSmoother pixelatedRateValue;
    LinearSmoother pixelatedBitsValue;
    LinearSmoother pixelatedDitherValue;
    LinearSmoother pixelatedJitterValue;
    LinearSmoother pixelatedToneValue;
    crusher::lightning::Processor lightningProcessor;
    LinearSmoother lightningSparkValue;
    LinearSmoother lightningReactValue;
    LinearSmoother lightningClipValue;
    LinearSmoother lightningEdgeValue;
    LinearSmoother lightningToneValue;
    crusher::crackle::Processor crackleProcessor;
    crusher::diodeclip::Processor diodeProcessor;
    LinearSmoother diodeInputHPValue;
    LinearSmoother diodeEnvelopeValue;
    LinearSmoother diodeShapeValue;
    LinearSmoother diodeOutputHPValue;
    LinearSmoother diodeOutputLPValue;
    LinearSmoother crackleDensityValue;
    LinearSmoother crackleBurstValue;
    LinearSmoother crackleDecayValue;
    LinearSmoother crackleGritValue;
    LinearSmoother crackleToneValue;
    crusher::fresh::Processor freshProcessor;
    LinearSmoother freshFrequencyValue;
    LinearSmoother freshExciteValue;
    LinearSmoother freshAirValue;
    LinearSmoother freshSaturationValue;
    LinearSmoother freshToneValue;
    crusher::fuzz::Processor fuzzProcessor;
    LinearSmoother fuzzGateValue;
    LinearSmoother fuzzBiasValue;
    LinearSmoother fuzzClipValue;
    LinearSmoother fuzzBodyValue;
    LinearSmoother fuzzToneValue;
    crusher::hardclip::Processor hardClipProcessor;
    LinearSmoother hardClipThresholdValue;
    LinearSmoother hardClipBiasValue;
    LinearSmoother hardClipKneeValue;
    LinearSmoother hardClipFoldValue;
    LinearSmoother hardClipToneValue;
    crusher::comb::Processor combProcessor;
    LinearSmoother combTimeValue;
    LinearSmoother combFeedbackValue;
    LinearSmoother combDampValue;
    LinearSmoother combSpreadValue;
    LinearSmoother combToneValue;
    crusher::asym::Processor asymProcessor;
    LinearSmoother asymBiasValue;
    LinearSmoother asymSkewValue;
    LinearSmoother asymShapeValue;
    LinearSmoother asymToneValue;
    crusher::eq::GraphicEQ graphicEQ;
    LinearSmoother graphicEQEnabledValue;
    crusher::humpmodel::Processor humpProcessor;
    LinearSmoother humpBodyValue;
    LinearSmoother humpBiasValue;
    LinearSmoother humpFoldValue;
    LinearSmoother humpToneValue;
    hump::Filter humpFilter;
    hump::Compressor humpCompressor;
    hump::ParamRamp mechRamp;
    hump::ParamRamp tubeRamp;
    hump::ParamRamp filterCutoffRamp { 1.0f };
    hump::ParamRamp filterResonanceRamp;
    hump::ParamRamp compAmountRamp;
    int currentDistortionType = 0;
    int previousDistortionType = 0;
    LinearSmoother distortionEnabledValue;
    LinearSmoother driveValue;
    LinearSmoother mixValue;
    LinearSmoother distOutputGainValue;
    LinearSmoother outputGainValue;
    LinearSmoother bypassValue;
    LinearSmoother distortionTypeCrossfade;

    std::array<float, channelCount> ampLowpassState {};
    std::array<float, channelCount> ampPreviousInput {};
    std::array<float, channelCount> ampPreviousOutput {};
    float ampDcCoefficient = 0.0f;
    float ampToneCoefficient = 0.0f;

    mutable juce::CriticalSection savedParameterStateLock;
    juce::ValueTree savedParameterState;
    std::atomic<bool> pendingProcessingReset { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrusherAudioProcessor)
};
