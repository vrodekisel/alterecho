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
    smoothedAdaptivePitchShiftSemitones.reset(currentSampleRate, 0.09);
    smoothedPitchTrackingMix.reset(currentSampleRate, smoothingSeconds);

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
    smoothedAdaptivePitchShiftSemitones.setCurrentAndTargetValue(
        pitchShiftSemitones.load(std::memory_order_relaxed)
    );
    smoothedPitchTrackingMix.setCurrentAndTargetValue(
        pitchTrackingMix.load(std::memory_order_relaxed)
    );

    pitchPhase[0] = 0.0f;
    pitchPhase[1] = 0.5f;
    pitchWriteIndex[0] = 0;
    pitchWriteIndex[1] = 0;
    pitchSamplesWritten[0] = 0;
    pitchSamplesWritten[1] = 0;
    pitchTrackingWriteIndex = 0;
    pitchTrackingSamplesWritten = 0;
    pitchTrackingSamplesUntilAnalysis = 0;
    lastDetectedPitchHz = 0.0f;
    lastAdaptivePitchShiftSemitones = pitchShiftSemitones.load(std::memory_order_relaxed);

    auto bufferSize = static_cast<int>(currentSampleRate * 0.18);
    bufferSize = juce::jmax(bufferSize, currentMaximumBlockSize * 4);
    bufferSize = juce::jmax(bufferSize, 8192);
    pitchBuffer[0].assign(static_cast<size_t>(bufferSize), 0.0f);
    pitchBuffer[1].assign(static_cast<size_t>(bufferSize), 0.0f);

    auto pitchTrackingBufferSize = static_cast<int>(currentSampleRate * 0.08);
    pitchTrackingBufferSize = juce::jmax(pitchTrackingBufferSize, currentMaximumBlockSize * 2);
    pitchTrackingBufferSize = juce::jmax(pitchTrackingBufferSize, 4096);
    pitchTrackingBuffer.assign(static_cast<size_t>(pitchTrackingBufferSize), 0.0f);
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

void VoiceShiftEffect::setPitchTrackingEnabled(bool shouldBeEnabled)
{
    pitchTrackingEnabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void VoiceShiftEffect::setTargetPitchHz(float hz)
{
    targetPitchHz.store(juce::jlimit(0.0f, 500.0f, hz), std::memory_order_relaxed);
}

void VoiceShiftEffect::setPitchTrackingMix(float amount)
{
    pitchTrackingMix.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void VoiceShiftEffect::setPitchShiftRange(float minimumSemitones, float maximumSemitones)
{
    auto minimum = juce::jlimit(-24.0f, 24.0f, minimumSemitones);
    auto maximum = juce::jlimit(-24.0f, 24.0f, maximumSemitones);

    if (minimum > maximum)
        std::swap(minimum, maximum);

    minPitchShiftSemitones.store(minimum, std::memory_order_relaxed);
    maxPitchShiftSemitones.store(maximum, std::memory_order_relaxed);
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

    auto manualPitchTarget = pitchShiftSemitones.load(std::memory_order_relaxed);
    auto adaptivePitchTarget = pitchTrackingEnabled.load(std::memory_order_relaxed)
        ? lastAdaptivePitchShiftSemitones
        : manualPitchTarget;

    if (pitchTrackingEnabled.load(std::memory_order_relaxed))
    {
        writePitchTrackingInput(buffer, startSample, numSamples);
        pitchTrackingSamplesUntilAnalysis -= numSamples;

        if (pitchTrackingSamplesUntilAnalysis <= 0)
        {
            auto detectedPitchHz = estimateInputPitchHz();

            if (detectedPitchHz > 0.0f)
            {
                lastDetectedPitchHz = detectedPitchHz;
                adaptivePitchTarget = calculateAdaptivePitchShiftSemitones(detectedPitchHz);
            }
            else if (lastDetectedPitchHz > 0.0f)
            {
                adaptivePitchTarget = calculateAdaptivePitchShiftSemitones(lastDetectedPitchHz);
            }
            else
            {
                adaptivePitchTarget = manualPitchTarget;
            }

            lastAdaptivePitchShiftSemitones = adaptivePitchTarget;
            pitchTrackingSamplesUntilAnalysis = juce::jmax(
                1,
                static_cast<int>(currentSampleRate * 0.02)
            );
        }
    }

    smoothedAdaptivePitchShiftSemitones.setTargetValue(adaptivePitchTarget);

    auto channelsToProcess = juce::jmin(buffer.getNumChannels(), 2);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto manualPitch = smoothedPitchShiftSemitones.getNextValue();
        auto adaptivePitch = smoothedAdaptivePitchShiftSemitones.getNextValue();
        auto trackingAmount = smoothedPitchTrackingMix.getNextValue();
        auto pitch = manualPitch * (1.0f - trackingAmount) + adaptivePitch * trackingAmount;
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
    smoothedPitchTrackingMix.setTargetValue(
        pitchTrackingEnabled.load(std::memory_order_relaxed)
            ? pitchTrackingMix.load(std::memory_order_relaxed)
            : 0.0f
    );

    if (!enabled.load(std::memory_order_relaxed))
    {
        smoothedPitchShiftSemitones.skip(numSamples);
        smoothedFormantShiftSemitones.skip(numSamples);
        smoothedFormantMix.skip(numSamples);
        smoothedMix.skip(numSamples);
        smoothedAdaptivePitchShiftSemitones.skip(numSamples);
        smoothedPitchTrackingMix.skip(numSamples);
    }
}

void VoiceShiftEffect::writePitchTrackingInput(
    const juce::AudioBuffer<float>& buffer,
    int startSample,
    int numSamples
)
{
    if (pitchTrackingBuffer.empty())
        return;

    auto channelsToRead = juce::jmin(buffer.getNumChannels(), 2);

    if (channelsToRead <= 0)
        return;

    auto bufferSize = static_cast<int>(pitchTrackingBuffer.size());

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto monoSample = 0.0f;

        for (int channel = 0; channel < channelsToRead; ++channel)
            monoSample += buffer.getSample(channel, startSample + sample);

        monoSample /= static_cast<float>(channelsToRead);
        pitchTrackingBuffer[static_cast<size_t>(pitchTrackingWriteIndex)] = monoSample;

        ++pitchTrackingWriteIndex;

        if (pitchTrackingWriteIndex >= bufferSize)
            pitchTrackingWriteIndex = 0;

        if (pitchTrackingSamplesWritten < bufferSize)
            ++pitchTrackingSamplesWritten;
    }
}

float VoiceShiftEffect::estimateInputPitchHz() const
{
    if (pitchTrackingBuffer.empty())
        return 0.0f;

    constexpr auto minimumPitchHz = 60.0f;
    constexpr auto maximumPitchHz = 360.0f;
    auto windowSamples = static_cast<int>(currentSampleRate * 0.045);
    auto bufferSize = static_cast<int>(pitchTrackingBuffer.size());
    windowSamples = juce::jlimit(128, bufferSize, windowSamples);

    if (pitchTrackingSamplesWritten < windowSamples)
        return 0.0f;

    auto minimumLag = juce::jmax(1, static_cast<int>(currentSampleRate / maximumPitchHz));
    auto maximumLag = juce::jmin(windowSamples - 2, static_cast<int>(currentSampleRate / minimumPitchHz));

    if (minimumLag >= maximumLag)
        return 0.0f;

    auto readSample = [this, bufferSize, windowSamples](int index)
    {
        auto position = pitchTrackingWriteIndex - windowSamples + index;

        while (position < 0)
            position += bufferSize;

        position %= bufferSize;
        return pitchTrackingBuffer[static_cast<size_t>(position)];
    };

    auto mean = 0.0f;

    for (int index = 0; index < windowSamples; ++index)
        mean += readSample(index);

    mean /= static_cast<float>(windowSamples);

    auto energy = 0.0f;

    for (int index = 0; index < windowSamples; ++index)
    {
        auto sample = readSample(index) - mean;
        energy += sample * sample;
    }

    auto rms = std::sqrt(energy / static_cast<float>(windowSamples));

    if (rms < 0.006f)
        return 0.0f;

    auto bestCorrelation = 0.0f;
    auto bestLag = 0;

    for (int lag = minimumLag; lag <= maximumLag; ++lag)
    {
        auto correlation = 0.0f;
        auto laggedEnergy = 0.0f;
        auto currentEnergy = 0.0f;
        auto samplesToCompare = windowSamples - lag;

        for (int index = 0; index < samplesToCompare; ++index)
        {
            auto current = readSample(index + lag) - mean;
            auto lagged = readSample(index) - mean;
            correlation += current * lagged;
            currentEnergy += current * current;
            laggedEnergy += lagged * lagged;
        }

        auto normalisedCorrelation = correlation
            / juce::jmax(0.000001f, std::sqrt(currentEnergy * laggedEnergy));

        if (normalisedCorrelation > bestCorrelation)
        {
            bestCorrelation = normalisedCorrelation;
            bestLag = lag;
        }
    }

    if (bestLag <= 0 || bestCorrelation < 0.42f)
        return 0.0f;

    return static_cast<float>(currentSampleRate) / static_cast<float>(bestLag);
}

float VoiceShiftEffect::calculateAdaptivePitchShiftSemitones(float detectedPitchHz) const
{
    auto targetHz = targetPitchHz.load(std::memory_order_relaxed);

    if (detectedPitchHz <= 0.0f || targetHz <= 0.0f)
        return pitchShiftSemitones.load(std::memory_order_relaxed);

    auto semitones = 12.0f * std::log2(targetHz / detectedPitchHz);
    return juce::jlimit(
        minPitchShiftSemitones.load(std::memory_order_relaxed),
        maxPitchShiftSemitones.load(std::memory_order_relaxed),
        semitones
    );
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
