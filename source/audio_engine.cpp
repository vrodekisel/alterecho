#include "audio_engine.h"

void AudioEngine::setVoiceEnabled(bool shouldBeEnabled)
{
    voiceEnabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void AudioEngine::setOutputGain(float newGain)
{
    outputGain.store(newGain, std::memory_order_relaxed);
}

bool AudioEngine::isVoiceEnabled() const
{
    return voiceEnabled.load(std::memory_order_relaxed);
}

float AudioEngine::getOutputGain() const
{
    return outputGain.load(std::memory_order_relaxed);
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

            auto currentGain = getOutputGain();

            for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto boostedSample = inputData[sample] * currentGain;

                outputData[sample] = juce::jlimit(
                    -1.0f,
                    1.0f,
                    boostedSample
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