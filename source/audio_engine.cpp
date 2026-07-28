#include "audio_engine.h"

void AudioEngine::prepareToPlay(double sampleRate)
{
    echoEffect.prepareToPlay(sampleRate);
}

void AudioEngine::setVoiceEnabled(bool shouldBeEnabled)
{
    voiceEnabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void AudioEngine::setOutputGain(float newGain)
{
    outputGain.store(newGain, std::memory_order_relaxed);
}

void AudioEngine::setEffectsBypassed(bool shouldBeBypassed)
{
    effectsBypassed.store(shouldBeBypassed, std::memory_order_relaxed);
}

void AudioEngine::setEchoEnabled(bool shouldBeEnabled)
{
    echoEffect.setEnabled(shouldBeEnabled);
}

void AudioEngine::setEchoDelayMs(float newDelayMs)
{
    echoEffect.setDelayMs(newDelayMs);
}

void AudioEngine::setEchoFeedback(float newFeedback)
{
    echoEffect.setFeedback(newFeedback);
}

void AudioEngine::setEchoMix(float newMix)
{
    echoEffect.setMix(newMix);
}

bool AudioEngine::isVoiceEnabled() const
{
    return voiceEnabled.load(std::memory_order_relaxed);
}

float AudioEngine::getOutputGain() const
{
    return outputGain.load(std::memory_order_relaxed);
}

bool AudioEngine::areEffectsBypassed() const
{
    return effectsBypassed.load(std::memory_order_relaxed);
}

void AudioEngine::processBlock(
    const juce::AudioSourceChannelInfo& bufferToFill,
    const juce::AudioIODevice& device
)
{
    if (!isVoiceEnabled())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    auto activeInputChannels = device.getActiveInputChannels();
    auto activeOutputChannels = device.getActiveOutputChannels();

    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;

    auto currentGain = getOutputGain();
    auto effectsAreBypassed = areEffectsBypassed();

    for (int channel = 0; channel < maxOutputChannels; ++channel)
    {
        if (!activeOutputChannels[channel])
            continue;

        auto* outputData = bufferToFill.buffer->getWritePointer(
            channel,
            bufferToFill.startSample
        );

        if (channel < maxInputChannels && activeInputChannels[channel])
        {
            auto* inputData = bufferToFill.buffer->getReadPointer(
                channel,
                bufferToFill.startSample
            );

            for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto outputSample = inputData[sample] * currentGain;

                if (!effectsAreBypassed)
                    outputSample = echoEffect.processSample(outputSample, channel);

                outputData[sample] = juce::jlimit(
                    -1.0f,
                    1.0f,
                    outputSample
                );
            }
        }
        else
        {
            juce::FloatVectorOperations::clear(
                outputData,
                bufferToFill.numSamples
            );
        }
    }
}