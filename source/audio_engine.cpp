#include "audio_engine.h"

#include <cmath>

void AudioEngine::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    echoEffect.prepareToPlay(sampleRate);
    toneEffect.prepareToPlay(sampleRate);
    robotEffect.prepareToPlay(sampleRate);
    voiceShiftEffect.prepareToPlay(sampleRate, maximumBlockSize);
    processingBuffer.setSize(2, juce::jmax(1, maximumBlockSize));
    processingBuffer.clear();
}

void AudioEngine::setVoiceEnabled(bool shouldBeEnabled)
{
    voiceEnabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void AudioEngine::setInputGain(float newGain)
{
    inputGain.store(newGain, std::memory_order_relaxed);
}

void AudioEngine::setOutputGain(float newGain)
{
    outputGain.store(newGain, std::memory_order_relaxed);
}

void AudioEngine::setEffectsBypassed(bool shouldBeBypassed)
{
    effectsBypassed.store(shouldBeBypassed, std::memory_order_relaxed);
}

void AudioEngine::setVoiceProfileParameters(const TechnicalVoiceParameters& parameters)
{
    echoEffect.setEnabled(parameters.echoEnabled);
    echoEffect.setDelayMs(parameters.echoDelayMs);
    echoEffect.setFeedback(parameters.echoFeedback);
    echoEffect.setMix(parameters.echoMix);
    toneEffect.setEnabled(parameters.toneEnabled);
    toneEffect.setHighPassHz(parameters.highPassHz);
    toneEffect.setLowPassHz(parameters.lowPassHz);
    toneEffect.setDrive(parameters.drive);
    toneEffect.setCompression(parameters.compression);
    toneEffect.setNoise(parameters.noise);
    toneEffect.setBody(parameters.body);
    toneEffect.setPresence(parameters.presence);
    robotEffect.setEnabled(parameters.robotEnabled);
    robotEffect.setFrequencyHz(parameters.robotFrequencyHz);
    robotEffect.setDepth(parameters.robotDepth);
    robotEffect.setCrush(parameters.robotCrush);
    robotEffect.setMix(parameters.robotMix);
    robotEffect.setPitchShiftSemitones(parameters.robotPitchShiftSemitones);
    voiceShiftEffect.setEnabled(parameters.voiceShiftEnabled);
    voiceShiftEffect.setPitchShiftSemitones(parameters.pitchShiftSemitones);
    voiceShiftEffect.setFormantShiftSemitones(parameters.formantShiftSemitones);
    voiceShiftEffect.setFormantMix(parameters.formantMix);
    voiceShiftEffect.setMix(parameters.voiceShiftMix);
}

bool AudioEngine::isVoiceEnabled() const
{
    return voiceEnabled.load(std::memory_order_relaxed);
}

float AudioEngine::getInputGain() const
{
    return inputGain.load(std::memory_order_relaxed);
}

float AudioEngine::getOutputGain() const
{
    return outputGain.load(std::memory_order_relaxed);
}

bool AudioEngine::areEffectsBypassed() const
{
    return effectsBypassed.load(std::memory_order_relaxed);
}

float AudioEngine::getInputLevel() const
{
    return inputLevel.load(std::memory_order_relaxed);
}

float AudioEngine::getOutputLevel() const
{
    return outputLevel.load(std::memory_order_relaxed);
}

float AudioEngine::softLimit(float sample)
{
    constexpr auto threshold = 0.95f;
    auto magnitude = std::abs(sample);

    if (magnitude <= threshold)
        return sample;

    auto excess = magnitude - threshold;
    auto limitedMagnitude = threshold
        + (1.0f - threshold) * (1.0f - std::exp(-excess / (1.0f - threshold)));

    return std::copysign(limitedMagnitude, sample);
}

void AudioEngine::processBlock(
    const juce::AudioSourceChannelInfo& bufferToFill,
    const juce::AudioIODevice& device
)
{

    if (!isVoiceEnabled())
    {
        inputLevel.store(0.0f, std::memory_order_relaxed);
        outputLevel.store(0.0f, std::memory_order_relaxed);
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    auto activeInputChannels = device.getActiveInputChannels();
    auto activeOutputChannels = device.getActiveOutputChannels();

    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;

    auto firstInputChannel = -1;

    for (int channel = 0; channel < maxInputChannels; ++channel)
    {
        if (activeInputChannels[channel])
        {
            firstInputChannel = channel;
            break;
        }
    }

    if (firstInputChannel < 0)
    {
        inputLevel.store(0.0f, std::memory_order_relaxed);
        outputLevel.store(0.0f, std::memory_order_relaxed);
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    auto currentInputGain = getInputGain();
    auto currentOutputGain = getOutputGain();
    auto effectsAreBypassed = areEffectsBypassed();
    auto blockInputPeak = 0.0f;
    auto blockOutputPeak = 0.0f;

    if (processingBuffer.getNumChannels() < maxOutputChannels
        || processingBuffer.getNumSamples() < bufferToFill.numSamples)
    {
        processingBuffer.setSize(
            juce::jmax(1, maxOutputChannels),
            juce::jmax(1, bufferToFill.numSamples),
            false,
            false,
            true
        );
    }

    processingBuffer.clear(0, bufferToFill.numSamples);

    for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
    {
        for (int channel = 0; channel < maxOutputChannels; ++channel)
        {
            if (!activeOutputChannels[channel])
                continue;

            auto inputChannel = firstInputChannel;

            if (channel < maxInputChannels && activeInputChannels[channel])
                inputChannel = channel;

            auto* inputData = bufferToFill.buffer->getReadPointer(
                inputChannel,
                bufferToFill.startSample
            );

            auto outputSample = inputData[sample] * currentInputGain;
            blockInputPeak = juce::jmax(blockInputPeak, std::abs(outputSample));
            processingBuffer.setSample(channel, sample, outputSample);
        }
    }

    if (!effectsAreBypassed)
        voiceShiftEffect.processBlock(processingBuffer, 0, bufferToFill.numSamples);

    for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
    {
        for (int channel = 0; channel < maxOutputChannels; ++channel)
        {
            if (!activeOutputChannels[channel])
                continue;

            auto* outputData = bufferToFill.buffer->getWritePointer(
                channel,
                bufferToFill.startSample
            );

            auto outputSample = processingBuffer.getSample(channel, sample);
            
            if (!effectsAreBypassed)
            {
                outputSample = toneEffect.processSample(outputSample, channel);
                outputSample = robotEffect.processSample(outputSample, channel);
                outputSample = echoEffect.processSample(outputSample, channel);
            }

            outputSample *= currentOutputGain;

            outputData[sample] = softLimit(outputSample);

            blockOutputPeak = juce::jmax(
                blockOutputPeak,
                std::abs(outputData[sample])
            );
        }

        if (!effectsAreBypassed)
            echoEffect.advance();
    }

    inputLevel.store(juce::jlimit(0.0f, 1.0f, blockInputPeak), std::memory_order_relaxed);
    outputLevel.store(juce::jlimit(0.0f, 1.0f, blockOutputPeak), std::memory_order_relaxed);
}
