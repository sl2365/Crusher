#include "PluginProcessor.h"
#include <cmath>

float CrusherAudioProcessor::processRectifyDrive(float sample, float amount,
                                                    int channel) noexcept
{
    return rectifyProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processCrackleDrive(float sample, float amount,
                                                   int channel) noexcept
{
    return crackleProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processLightningDrive(float sample, float amount,
                                                      int channel) noexcept
{
    return lightningProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processAmpDrive(float sample, float amount,
                                               int channel) noexcept
{
    const auto index = static_cast<size_t>(juce::jlimit(
        0, channelCount - 1, channel));
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    sample = ampSimulator.pre(sample, channel);
    const float preampGain = 1.0f + safeAmount * 13.0f;
    const float bias = 0.08f + safeAmount * 0.22f;
    const float zeroOffset = std::tanh(bias);
    const float reference = juce::jmax(
        0.001f, std::tanh(preampGain * 0.75f + bias) - zeroOffset);
    const float preamp = (std::tanh(sample * preampGain + bias) - zeroOffset)
                       / reference;
    const float powerGain = 1.0f + safeAmount * 3.5f;
    const float powerReference = juce::jmax(0.001f, std::tanh(powerGain));
    const float powerStage = std::tanh(preamp * powerGain) / powerReference;

    ampLowpassState[index] = (1.0f - ampToneCoefficient) * powerStage
                           + ampToneCoefficient * ampLowpassState[index];
    const float dcBlocked = ampLowpassState[index] - ampPreviousInput[index]
                          + ampDcCoefficient * ampPreviousOutput[index];
    ampPreviousInput[index] = ampLowpassState[index];
    ampPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.92f / (1.0f + safeAmount * 0.35f);
    const float shaped = ampPreviousOutput[index] * outputCompensation;
    // Keep the original core unchanged when the new Amp settings are neutral.
    const float driven = sample + (shaped - sample) * safeAmount;
    return ampSimulator.post(driven, channel);
}

float CrusherAudioProcessor::processBumpyDrive(float sample, float amount,
                                                 int channel) noexcept
{
    return bumpyProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processAsymmetricalDrive(
    float sample, float amount, int channel) noexcept
{
    return asymProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processRazorDrive(float sample, float amount,
                                                 int channel) noexcept
{
    return razorProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processPixelatedDrive(
    float sample, float amount, int channel) noexcept
{
    return pixelatedProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processCombDrive(float sample, float amount,
                                               int channel) noexcept
{
    return combProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processStressedDrive(
    float sample, float amount, int channel) noexcept
{
    return stressedProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processSamuraiDrive(
    float sample, float amount, int channel) noexcept
{
    return samuraiProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processFreshDrive(float sample, float amount,
                                                 int channel) noexcept
{
    return freshProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processHardClipDrive(float sample,
                                                    float amount, int channel) noexcept
{
    return hardClipProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processFuzzDrive(float sample, float amount,
                                                int channel) noexcept
{
    return fuzzProcessor.process(sample, amount, channel);
}

float CrusherAudioProcessor::processDistortionType(
    float sample, float amount, int channel, int distortionType) noexcept
{
    // Several stateful distortion models still perform filters, noise and
    // nonlinear functions when their wet amount is exactly zero.  Their
    // output is the untouched sample in that state, so avoid dispatching the
    // inactive model altogether.  The smoothed Drive value still exercises
    // the full path as soon as it moves above zero.
    // Amp cabinet/tone controls remain effective even at zero DRIVE.
    if (amount <= 0.0f && distortionType != 0 && distortionType != 1 && distortionType != 4 && distortionType != 5
        && distortionType != 6 && distortionType != 7 && distortionType != 11
        && distortionType != 14 && distortionType != 15
        && distortionType != 16 && distortionType != 10
        && distortionType != 9 && distortionType != 13 && distortionType != 12
        && distortionType != 2 && distortionType != 18)
        return sample;

    switch (juce::jlimit(0, distortionTypeCount - 1, distortionType))
    {
        case 0:  return softClipProcessor.process(sample, amount, channel);
        case 1:  return saturationProcessor.process(sample, amount, channel);
        case 2:  return tubeProcessor.process(sample, amount, channel);
        case 3:  return processRectifyDrive(sample, amount, channel);
        case 4:  return processCrackleDrive(sample, amount, channel);
        case 5:  return processLightningDrive(sample, amount, channel);
        case 6:  return processAmpDrive(sample, amount, channel);
        case 7:  return processBumpyDrive(sample, amount, channel);
        case 8:  return processAsymmetricalDrive(sample, amount, channel);
        case 9:  return processRazorDrive(sample, amount, channel);
        case 10: return processPixelatedDrive(sample, amount, channel);
        case 11: return processCombDrive(sample, amount, channel);
        case 12: return processStressedDrive(sample, amount, channel);
        case 13: return processSamuraiDrive(sample, amount, channel);
        case 14: return processFreshDrive(sample, amount, channel);
        case 15: return processHardClipDrive(sample, amount, channel);
        case 16: return processFuzzDrive(sample, amount, channel);
        case 18: return diodeProcessor.process(sample, amount, channel);
        case 19: return loFiProcessor.process(sample, amount, channel);
        case 20: return codecProcessor.process(sample, amount, channel);
        case 21: return vocalProcessor.process(sample, amount, channel);
        default: break;
    }

    return sample;
}

void CrusherAudioProcessor::resetDistortionState() noexcept
{
    rectifyProcessor.reset();
    crackleProcessor.reset();
    diodeProcessor.reset();
    loFiProcessor.reset();
    codecProcessor.reset();
    vocalProcessor.reset();
    lightningProcessor.reset();
    ampLowpassState.fill(0.0f);
    ampPreviousInput.fill(0.0f);
    ampPreviousOutput.fill(0.0f);
    bumpyProcessor.reset();
    asymProcessor.reset();
    razorProcessor.reset();
    pixelatedProcessor.reset();
    combProcessor.reset();
    tubeProcessor.reset();
    stressedProcessor.reset();
    softClipProcessor.reset();
    saturationProcessor.reset();
    samuraiProcessor.reset();
    freshProcessor.reset();
    fuzzProcessor.reset();
    hardClipProcessor.reset();
}

