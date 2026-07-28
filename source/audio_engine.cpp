#include "audio_engine.h"

void AudioEngine::prepareToPlay(double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    auto delayBufferSize = static_cast<int>(currentSampleRate * maxDelaySeconds);
    delayBuffer.setSize(2, delayBufferSize);
    delayBuffer.clear();

    delayWritePosition = 0;
}

void AudioEngine::setVoiceEnabled(bool shouldBeEnabled)
{
    voiceEnabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void AudioEngine::setOutputGain(float newGain)
{
    outputGain.store(newGain, std::memory_order_relaxed);
}

void AudioEngine::setEchoEnabled(bool shouldBeEnabled)
{
    echoEnabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void AudioEngine::setEchoDelayMs(float newDelayMs)
{
    echoDelayMs.store(
        juce::jlimit(1.0f, 2000.0f, newDelayMs),
        std::memory_order_relaxed
    );
}

void AudioEngine::setEchoFeedback(float newFeedback)
{
    echoFeedback.store(
        juce::jlimit(0.0f, 0.95f, newFeedback),
        std::memory_order_relaxed
    );
}

void AudioEngine::setEchoMix(float newMix)
{
    echoMix.store(
        juce::jlimit(0.0f, 1.0f, newMix),
        std::memory_order_relaxed
    );
}

bool AudioEngine::isVoiceEnabled() const
{
    return voiceEnabled.load(std::memory_order_relaxed);
}

float AudioEngine::getOutputGain() const
{
    return outputGain.load(std::memory_order_relaxed);
}

bool AudioEngine::isEchoEnabled() const
{
    return echoEnabled.load(std::memory_order_relaxed);
}

float AudioEngine::getEchoDelayMs() const
{
    return echoDelayMs.load(std::memory_order_relaxed);
}

float AudioEngine::getEchoFeedback() const
{
    return echoFeedback.load(std::memory_order_relaxed);
}

float AudioEngine::getEchoMix() const
{
    return echoMix.load(std::memory_order_relaxed);
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

    if (delayBuffer.getNumSamples() == 0)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    auto activeInputChannels = device.getActiveInputChannels();
    auto activeOutputChannels = device.getActiveOutputChannels();

    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;

    auto currentGain = getOutputGain();
    auto echoIsEnabled = isEchoEnabled();
    auto delaySamples = static_cast<int>(
        currentSampleRate * getEchoDelayMs() / 1000.0
    );

    delaySamples = juce::jlimit(1, delayBuffer.getNumSamples() - 1, delaySamples);

    auto feedback = getEchoFeedback();
    auto mix = getEchoMix();

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

            auto delayChannel = channel % delayBuffer.getNumChannels();
            auto* delayData = delayBuffer.getWritePointer(delayChannel);

            auto localWritePosition = delayWritePosition;

            for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto readPosition = localWritePosition - delaySamples;

                if (readPosition < 0)
                    readPosition += delayBuffer.getNumSamples();

                auto drySample = inputData[sample] * currentGain;
                auto delayedSample = delayData[readPosition];

                auto outputSample = echoIsEnabled
                    ? drySample * (1.0f - mix) + delayedSample * mix
                    : drySample;

                delayData[localWritePosition] = echoIsEnabled
                    ? drySample + delayedSample * feedback
                    : drySample;

                outputData[sample] = juce::jlimit(
                    -1.0f,
                    1.0f,
                    outputSample
                );

                ++localWritePosition;

                if (localWritePosition >= delayBuffer.getNumSamples())
                    localWritePosition = 0;
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

    delayWritePosition += bufferToFill.numSamples;

    while (delayWritePosition >= delayBuffer.getNumSamples())
        delayWritePosition -= delayBuffer.getNumSamples();
}