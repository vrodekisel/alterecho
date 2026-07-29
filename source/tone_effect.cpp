#include "tone_effect.h"

#include <cmath>

void ToneEffect::prepareToPlay(double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateFilters();

    for (auto& filter : highPassFilters)
        filter.reset();

    for (auto& filter : lowPassFilters)
        filter.reset();

    for (auto& filter : bodyFilters)
        filter.reset();

    for (auto& filter : presenceFilters)
        filter.reset();

    envelope[0] = 0.0f;
    envelope[1] = 0.0f;
    crackleEnvelope[0] = 0.0f;
    crackleEnvelope[1] = 0.0f;
    flutter[0] = 0.0f;
    flutter[1] = 0.0f;
}

void ToneEffect::setEnabled(bool shouldBeEnabled)
{
    enabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

void ToneEffect::setHighPassHz(float hz)
{
    highPassHz.store(juce::jlimit(20.0f, 4000.0f, hz), std::memory_order_relaxed);
    updateFilters();
}

void ToneEffect::setLowPassHz(float hz)
{
    lowPassHz.store(juce::jlimit(400.0f, 16000.0f, hz), std::memory_order_relaxed);
    updateFilters();
}

void ToneEffect::setDrive(float amount)
{
    drive.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void ToneEffect::setCompression(float amount)
{
    compression.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void ToneEffect::setNoise(float amount)
{
    noise.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void ToneEffect::setBody(float amount)
{
    body.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

void ToneEffect::setPresence(float amount)
{
    presence.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed);
}

float ToneEffect::processSample(float inputSample, int channel)
{
    if (!enabled.load(std::memory_order_relaxed))
        return inputSample;

    auto filterChannel = channel % 2;
    auto sample = highPassFilters[filterChannel].processSample(inputSample);
    sample = lowPassFilters[filterChannel].processSample(sample);
    auto toneSample = sample;

    auto bodyAmount = body.load(std::memory_order_relaxed);

    if (bodyAmount > 0.0f)
    {
        auto bodySample = bodyFilters[filterChannel].processSample(toneSample);
        sample += bodySample * juce::jmap(bodyAmount, 0.0f, 0.34f);
    }

    auto presenceAmount = presence.load(std::memory_order_relaxed);

    if (presenceAmount > 0.0f)
    {
        auto presenceSample = presenceFilters[filterChannel].processSample(toneSample);
        sample += presenceSample * juce::jmap(presenceAmount, 0.0f, 0.26f);
    }

    auto magnitude = std::abs(sample);

    auto attack = 1.0f - std::exp(-1.0f / static_cast<float>(0.005 * currentSampleRate));
    auto release = 1.0f - std::exp(-1.0f / static_cast<float>(0.08 * currentSampleRate));
    auto coefficient = magnitude > envelope[filterChannel] ? attack : release;
    envelope[filterChannel] += coefficient * (magnitude - envelope[filterChannel]);

    auto compressionAmount = compression.load(std::memory_order_relaxed);

    if (compressionAmount > 0.0f)
    {
        auto threshold = juce::jmap(compressionAmount, 0.22f, 0.08f);
        auto ratio = juce::jmap(compressionAmount, 1.0f, 5.5f);

        if (envelope[filterChannel] > threshold)
        {
            auto compressedLevel = threshold
                + (envelope[filterChannel] - threshold) / ratio;
            auto gain = compressedLevel / juce::jmax(envelope[filterChannel], 0.0001f);
            sample *= gain;
        }
    }

    auto driveAmount = drive.load(std::memory_order_relaxed);

    if (driveAmount > 0.0f)
    {
        auto gain = juce::jmap(driveAmount, 1.0f, 4.0f);
        sample = std::tanh(sample * gain) / std::tanh(gain);
    }

    auto noiseAmount = noise.load(std::memory_order_relaxed);

    if (noiseAmount > 0.0f)
    {
        randomState[filterChannel] = randomState[filterChannel] * 1664525u + 1013904223u;
        auto randomValue = static_cast<float>((randomState[filterChannel] >> 8) & 0x00ffffff)
            / static_cast<float>(0x00ffffff);
        auto noiseSample = randomValue * 2.0f - 1.0f;

        randomState[filterChannel] = randomState[filterChannel] * 1664525u + 1013904223u;
        auto crackleRandom = static_cast<float>((randomState[filterChannel] >> 8) & 0x00ffffff)
            / static_cast<float>(0x00ffffff);

        randomState[filterChannel] = randomState[filterChannel] * 1664525u + 1013904223u;
        auto flutterRandom = static_cast<float>((randomState[filterChannel] >> 8) & 0x00ffffff)
            / static_cast<float>(0x00ffffff);

        auto voicePresence = juce::jlimit(0.0f, 1.0f, envelope[filterChannel] * 8.0f);
        auto duckedNoise = juce::jmap(voicePresence, 1.0f, 0.22f);
        auto noiseGain = juce::jmap(noiseAmount, 0.0f, 0.045f) * duckedNoise;
        auto voiceHissGain = juce::jmap(noiseAmount, 0.0f, 0.026f) * voicePresence;

        auto crackleProbability = 0.000025f + noiseAmount * 0.00022f;

        if (crackleRandom < crackleProbability)
            crackleEnvelope[filterChannel] = juce::jmap(noiseAmount, 0.08f, 0.42f);

        crackleEnvelope[filterChannel] *= 0.985f;

        auto flutterTarget = (flutterRandom * 2.0f - 1.0f) * noiseAmount;
        flutter[filterChannel] += 0.0008f * (flutterTarget - flutter[filterChannel]);
        auto flutterGain = 1.0f + flutter[filterChannel] * 0.08f;

        sample *= flutterGain;
        sample += noiseSample * (noiseGain + voiceHissGain);
        sample += noiseSample * crackleEnvelope[filterChannel];
    }

    return sample;
}

void ToneEffect::updateFilters()
{
    auto hp = highPassHz.load(std::memory_order_relaxed);
    auto lp = lowPassHz.load(std::memory_order_relaxed);

    hp = juce::jmin(hp, lp - 50.0f);

    auto highPassCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        currentSampleRate,
        hp
    );

    auto lowPassCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        currentSampleRate,
        lp
    );

    auto bodyCoefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(
        currentSampleRate,
        180.0f,
        0.8f
    );

    auto presenceCoefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(
        currentSampleRate,
        3200.0f,
        0.9f
    );

    for (auto& filter : highPassFilters)
        filter.coefficients = highPassCoefficients;

    for (auto& filter : lowPassFilters)
        filter.coefficients = lowPassCoefficients;

    for (auto& filter : bodyFilters)
        filter.coefficients = bodyCoefficients;

    for (auto& filter : presenceFilters)
        filter.coefficients = presenceCoefficients;
}
