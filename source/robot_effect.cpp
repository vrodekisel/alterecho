#include "robot_effect.h"

#include <cmath>

void RobotEffect::prepareToPlay(double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    phase[0] = 0.0f;
    phase[1] = 0.25f;
    pitchPhase[0] = 0.0f;
    pitchPhase[1] = 0.5f;
    heldSample[0] = 0.0f;
    heldSample[1] = 0.0f;
    holdCounter[0] = 0;
    holdCounter[1] = 0;
    pitchWriteIndex[0] = 0;
    pitchWriteIndex[1] = 0;
    pitchSamplesWritten[0] = 0;
    pitchSamplesWritten[1] = 0;

    auto bufferSize = static_cast<int>(currentSampleRate * 0.12);
    bufferSize = juce::jmax(bufferSize, 4096);
    pitchBuffer[0].assign(static_cast<size_t>(bufferSize), 0.0f);
    pitchBuffer[1].assign(static_cast<size_t>(bufferSize), 0.0f);
}

void RobotEffect::setEnabled(bool shouldBeEnabled)
{
    enabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void RobotEffect::setFrequencyHz(float hz)
{
    frequencyHz.store(juce::jlimit(8.0f, 180.0f, hz), std::memory_order_relaxed);
}

void RobotEffect::setDepth(float amount)
{
    depth.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void RobotEffect::setCrush(float amount)
{
    crush.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void RobotEffect::setMix(float amount)
{
    mix.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void RobotEffect::setPitchShiftSemitones(float semitones)
{
    pitchShiftSemitones.store(juce::jlimit(0.0f, 5.0f, semitones), std::memory_order_relaxed);
}

float RobotEffect::processSample(float inputSample, int channel)
{
    if (!enabled.load(std::memory_order_relaxed))
        return inputSample;

    auto robotMix = mix.load(std::memory_order_relaxed);

    if (robotMix <= 0.0f)
        return inputSample;

    auto robotDepth = depth.load(std::memory_order_relaxed);
    auto shiftedSample = processPitchShift(inputSample, channel);
    auto modulator = getNextModulatorSample(channel);
    auto tremolo = shiftedSample * (0.72f + 0.28f * modulator);
    auto ringModulated = shiftedSample * modulator;

    auto robotSample = shiftedSample * 0.5f
        + tremolo * 0.35f
        + ringModulated * (0.15f * robotDepth);

    robotSample = crushSample(robotSample, channel);

    auto driveGain = juce::jmap(robotDepth, 1.0f, 2.6f);
    robotSample = std::tanh(robotSample * driveGain) / std::tanh(driveGain);

    return inputSample * (1.0f - robotMix) + robotSample * robotMix;
}

float RobotEffect::processPitchShift(float inputSample, int channel)
{
    auto semitones = pitchShiftSemitones.load(std::memory_order_relaxed);

    if (semitones <= 0.01f || pitchBuffer[0].empty())
        return inputSample;

    auto robotChannel = channel % 2;
    auto& buffer = pitchBuffer[robotChannel];
    auto bufferSize = static_cast<int>(buffer.size());
    auto& writeIndex = pitchWriteIndex[robotChannel];

    buffer[static_cast<size_t>(writeIndex)] = inputSample;

    constexpr auto minDelaySamples = 96.0f;
    constexpr auto maxDelaySamples = 2048.0f;
    constexpr auto delayRangeSamples = maxDelaySamples - minDelaySamples;

    auto pitchRatio = std::pow(2.0f, semitones / 12.0f);
    auto phaseIncrement = (pitchRatio - 1.0f) / delayRangeSamples;

    auto phaseA = pitchPhase[robotChannel];
    auto phaseB = phaseA + 0.5f;

    if (phaseB >= 1.0f)
        phaseB -= 1.0f;

    auto delayA = maxDelaySamples - phaseA * delayRangeSamples;
    auto delayB = maxDelaySamples - phaseB * delayRangeSamples;
    auto windowA = 0.5f - 0.5f * std::cos(phaseA * juce::MathConstants<float>::twoPi);
    auto windowB = 0.5f - 0.5f * std::cos(phaseB * juce::MathConstants<float>::twoPi);
    auto windowTotal = windowA + windowB;

    auto shiftedSample = (
        readPitchBuffer(robotChannel, delayA) * windowA
        + readPitchBuffer(robotChannel, delayB) * windowB
    ) / juce::jmax(0.001f, windowTotal);

    pitchPhase[robotChannel] += phaseIncrement;

    if (pitchPhase[robotChannel] >= 1.0f)
        pitchPhase[robotChannel] -= 1.0f;

    ++writeIndex;

    if (writeIndex >= bufferSize)
        writeIndex = 0;

    if (pitchSamplesWritten[robotChannel] < static_cast<int>(maxDelaySamples))
        ++pitchSamplesWritten[robotChannel];

    auto pitchBlend = juce::jlimit(0.0f, 0.48f, semitones * 0.14f);

    if (pitchSamplesWritten[robotChannel] < static_cast<int>(maxDelaySamples))
        pitchBlend *= static_cast<float>(pitchSamplesWritten[robotChannel]) / maxDelaySamples;

    return inputSample * (1.0f - pitchBlend) + shiftedSample * pitchBlend;
}

float RobotEffect::readPitchBuffer(int channel, float delaySamples) const
{
    auto robotChannel = channel % 2;
    const auto& buffer = pitchBuffer[robotChannel];
    auto bufferSize = static_cast<int>(buffer.size());
    auto readPosition = static_cast<float>(pitchWriteIndex[robotChannel]) - delaySamples;

    while (readPosition < 0.0f)
        readPosition += static_cast<float>(bufferSize);

    auto indexA = static_cast<int>(readPosition) % bufferSize;
    auto indexB = (indexA + 1) % bufferSize;
    auto fraction = readPosition - static_cast<float>(indexA);

    return buffer[static_cast<size_t>(indexA)] * (1.0f - fraction)
        + buffer[static_cast<size_t>(indexB)] * fraction;
}

float RobotEffect::getNextModulatorSample(int channel)
{
    auto robotChannel = channel % 2;
    auto frequency = frequencyHz.load(std::memory_order_relaxed);

    auto modulator = std::sin(phase[robotChannel] * juce::MathConstants<float>::twoPi);

    phase[robotChannel] += frequency / static_cast<float>(currentSampleRate);

    if (phase[robotChannel] >= 1.0f)
        phase[robotChannel] -= 1.0f;

    return modulator;
}

float RobotEffect::crushSample(float sample, int channel)
{
    auto crushAmount = crush.load(std::memory_order_relaxed);

    if (crushAmount <= 0.0f)
        return sample;

    auto robotChannel = channel % 2;

    auto holdLength = static_cast<int>(juce::jmap(crushAmount, 1.0f, 18.0f));

    if (holdCounter[robotChannel] <= 0)
    {
        heldSample[robotChannel] = sample;
        holdCounter[robotChannel] = holdLength;
    }

    --holdCounter[robotChannel];

    auto bitLevels = juce::jmap(crushAmount, 256.0f, 24.0f);
    auto crushed = std::round(heldSample[robotChannel] * bitLevels) / bitLevels;

    return crushed;
}
