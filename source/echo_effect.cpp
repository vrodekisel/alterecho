#include "echo_effect.h"

void EchoEffect::prepareToPlay(double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    auto delayBufferSize = static_cast<int>(currentSampleRate * maxDelaySeconds);
    delayBuffer.setSize(2, delayBufferSize);
    delayBuffer.clear();

    delayWritePosition = 0;
}

void EchoEffect::setEnabled(bool shouldBeEnabled)
{
    enabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void EchoEffect::setDelayMs(float newDelayMs)
{
    delayMs.store(
        juce::jlimit(1.0f, 2000.0f, newDelayMs),
        std::memory_order_relaxed
    );
}

void EchoEffect::setFeedback(float newFeedback)
{
    feedback.store(
        juce::jlimit(0.0f, 0.95f, newFeedback),
        std::memory_order_relaxed
    );
}

void EchoEffect::setMix(float newMix)
{
    mix.store(
        juce::jlimit(0.0f, 1.0f, newMix),
        std::memory_order_relaxed
    );
}

bool EchoEffect::isEnabled() const
{
    return enabled.load(std::memory_order_relaxed);
}

float EchoEffect::getDelayMs() const
{
    return delayMs.load(std::memory_order_relaxed);
}

float EchoEffect::getFeedback() const
{
    return feedback.load(std::memory_order_relaxed);
}

float EchoEffect::getMix() const
{
    return mix.load(std::memory_order_relaxed);
}

float EchoEffect::processSample(float inputSample, int channel)
{
    if (delayBuffer.getNumSamples() == 0)
        return inputSample;

    auto delaySamples = static_cast<int>(
        currentSampleRate * getDelayMs() / 1000.0
    );

    delaySamples = juce::jlimit(1, delayBuffer.getNumSamples() - 1, delaySamples);

    auto delayChannel = channel % delayBuffer.getNumChannels();
    auto* delayData = delayBuffer.getWritePointer(delayChannel);

    auto readPosition = delayWritePosition - delaySamples;

    if (readPosition < 0)
        readPosition += delayBuffer.getNumSamples();

    auto delayedSample = delayData[readPosition];
    auto effectIsEnabled = isEnabled();

    auto outputSample = effectIsEnabled
        ? inputSample * (1.0f - getMix()) + delayedSample * getMix()
        : inputSample;

    delayData[delayWritePosition] = effectIsEnabled
        ? inputSample + delayedSample * getFeedback()
        : inputSample;

    return outputSample;
}

void EchoEffect::advance()
{
    if (delayBuffer.getNumSamples() == 0)
        return;

    ++delayWritePosition;

    if (delayWritePosition >= delayBuffer.getNumSamples())
        delayWritePosition = 0;
}
