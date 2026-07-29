#include "voice_shift_effect.h"

#include <cmath>

void VoiceShiftEffect::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentMaximumBlockSize = juce::jmax(1, maximumBlockSize);

    constexpr auto smoothingSeconds = 0.035;

    smoothedPitchShiftSemitones.reset(currentSampleRate, smoothingSeconds);
    smoothedFormantShiftSemitones.reset(currentSampleRate, smoothingSeconds);
    smoothedFormantMix.reset(currentSampleRate, smoothingSeconds);
    smoothedMix.reset(currentSampleRate, smoothingSeconds);

    smoothedPitchShiftSemitones.setCurrentAndTargetValue(
        pitchShiftSemitones.load(std::memory_order_relaxed)
    );
    smoothedFormantShiftSemitones.setCurrentAndTargetValue(
        formantShiftSemitones.load(std::memory_order_relaxed)
    );
    smoothedFormantMix.setCurrentAndTargetValue(
        formantMix.load(std::memory_order_relaxed)
    );
    smoothedMix.setCurrentAndTargetValue(
        mix.load(std::memory_order_relaxed)
    );

    pitchPhase[0] = 0.0f;
    pitchPhase[1] = 0.5f;
    pitchWriteIndex[0] = 0;
    pitchWriteIndex[1] = 0;
    pitchSamplesWritten[0] = 0;
    pitchSamplesWritten[1] = 0;

    auto bufferSize = static_cast<int>(currentSampleRate * 0.18);
    bufferSize = juce::jmax(bufferSize, currentMaximumBlockSize * 4);
    bufferSize = juce::jmax(bufferSize, 8192);
    pitchBuffer[0].assign(static_cast<size_t>(bufferSize), 0.0f);
    pitchBuffer[1].assign(static_cast<size_t>(bufferSize), 0.0f);
}

void VoiceShiftEffect::setEnabled(bool shouldBeEnabled)
{
    enabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void VoiceShiftEffect::setPitchShiftSemitones(float semitones)
{
    pitchShiftSemitones.store(juce::jlimit(-24.0f, 24.0f, semitones), std::memory_order_relaxed);
}

void VoiceShiftEffect::setFormantShiftSemitones(float semitones)
{
    formantShiftSemitones.store(juce::jlimit(-24.0f, 24.0f, semitones), std::memory_order_relaxed);
}

void VoiceShiftEffect::setFormantMix(float amount)
{
    formantMix.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void VoiceShiftEffect::setMix(float amount)
{
    mix.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void VoiceShiftEffect::processBlock(
    juce::AudioBuffer<float>& buffer,
    int startSample,
    int numSamples
)
{
    if (numSamples <= 0)
        return;

    updateSmoothedParameters(numSamples);

    if (!enabled.load(std::memory_order_relaxed))
        return;

    auto channelsToProcess = juce::jmin(buffer.getNumChannels(), 2);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto pitch = smoothedPitchShiftSemitones.getNextValue();
        smoothedFormantShiftSemitones.getNextValue();
        smoothedFormantMix.getNextValue();
        auto wetMix = smoothedMix.getNextValue();

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto drySample = buffer.getSample(channel, startSample + sample);
            auto shiftedSample = processPitchShiftSample(drySample, channel, pitch);
            auto outputSample = drySample * (1.0f - wetMix) + shiftedSample * wetMix;
            buffer.setSample(channel, startSample + sample, outputSample);
        }
    }
}

void VoiceShiftEffect::updateSmoothedParameters(int numSamples)
{
    smoothedPitchShiftSemitones.setTargetValue(
        pitchShiftSemitones.load(std::memory_order_relaxed)
    );
    smoothedFormantShiftSemitones.setTargetValue(
        formantShiftSemitones.load(std::memory_order_relaxed)
    );
    smoothedFormantMix.setTargetValue(
        formantMix.load(std::memory_order_relaxed)
    );
    smoothedMix.setTargetValue(
        mix.load(std::memory_order_relaxed)
    );

    if (!enabled.load(std::memory_order_relaxed))
    {
        smoothedPitchShiftSemitones.skip(numSamples);
        smoothedFormantShiftSemitones.skip(numSamples);
        smoothedFormantMix.skip(numSamples);
        smoothedMix.skip(numSamples);
    }
}

float VoiceShiftEffect::processPitchShiftSample(float inputSample, int channel, float semitones)
{
    auto pitchChannel = channel % 2;

    if (pitchBuffer[pitchChannel].empty())
        return inputSample;

    auto& buffer = pitchBuffer[pitchChannel];
    auto bufferSize = static_cast<int>(buffer.size());
    auto& writeIndex = pitchWriteIndex[pitchChannel];

    buffer[static_cast<size_t>(writeIndex)] = inputSample;

    auto absoluteSemitones = std::abs(semitones);

    if (absoluteSemitones <= 0.01f)
    {
        ++writeIndex;

        if (writeIndex >= bufferSize)
            writeIndex = 0;

        if (pitchSamplesWritten[pitchChannel] < bufferSize)
            ++pitchSamplesWritten[pitchChannel];

        return inputSample;
    }

    constexpr auto minDelaySamples = 96.0f;
    constexpr auto maxDelaySamples = 3072.0f;
    constexpr auto delayRangeSamples = maxDelaySamples - minDelaySamples;

    auto pitchRatio = std::pow(2.0f, semitones / 12.0f);
    auto phaseIncrement = std::abs(pitchRatio - 1.0f) / delayRangeSamples;

    auto phaseA = pitchPhase[pitchChannel];
    auto phaseB = phaseA + 0.5f;

    if (phaseB >= 1.0f)
        phaseB -= 1.0f;

    auto delayA = semitones > 0.0f
        ? maxDelaySamples - phaseA * delayRangeSamples
        : minDelaySamples + phaseA * delayRangeSamples;
    auto delayB = semitones > 0.0f
        ? maxDelaySamples - phaseB * delayRangeSamples
        : minDelaySamples + phaseB * delayRangeSamples;

    auto windowA = 0.5f - 0.5f * std::cos(phaseA * juce::MathConstants<float>::twoPi);
    auto windowB = 0.5f - 0.5f * std::cos(phaseB * juce::MathConstants<float>::twoPi);
    auto windowTotal = windowA + windowB;

    auto shiftedSample = (
        readPitchBuffer(pitchChannel, delayA) * windowA
        + readPitchBuffer(pitchChannel, delayB) * windowB
    ) / juce::jmax(0.001f, windowTotal);

    pitchPhase[pitchChannel] += phaseIncrement;

    if (pitchPhase[pitchChannel] >= 1.0f)
        pitchPhase[pitchChannel] -= 1.0f;

    ++writeIndex;

    if (writeIndex >= bufferSize)
        writeIndex = 0;

    if (pitchSamplesWritten[pitchChannel] < static_cast<int>(maxDelaySamples))
        ++pitchSamplesWritten[pitchChannel];

    if (pitchSamplesWritten[pitchChannel] < static_cast<int>(maxDelaySamples))
    {
        auto warmup = static_cast<float>(pitchSamplesWritten[pitchChannel]) / maxDelaySamples;
        shiftedSample = inputSample * (1.0f - warmup) + shiftedSample * warmup;
    }

    return shiftedSample;
}

float VoiceShiftEffect::readPitchBuffer(int channel, float delaySamples) const
{
    auto pitchChannel = channel % 2;
    const auto& buffer = pitchBuffer[pitchChannel];
    auto bufferSize = static_cast<int>(buffer.size());
    auto readPosition = static_cast<float>(pitchWriteIndex[pitchChannel]) - delaySamples;

    while (readPosition < 0.0f)
        readPosition += static_cast<float>(bufferSize);

    auto indexA = static_cast<int>(readPosition) % bufferSize;
    auto indexB = (indexA + 1) % bufferSize;
    auto fraction = readPosition - static_cast<float>(indexA);

    return buffer[static_cast<size_t>(indexA)] * (1.0f - fraction)
        + buffer[static_cast<size_t>(indexB)] * fraction;
}
