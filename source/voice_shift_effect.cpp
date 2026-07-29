#include "voice_shift_effect.h"

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
    juce::AudioBuffer<float>&,
    int,
    int numSamples
)
{
    if (numSamples <= 0)
        return;

    updateSmoothedParameters(numSamples);

    if (!enabled.load(std::memory_order_relaxed))
        return;

    // Transparent for now. The block API and smoothed parameters are the stable
    // attachment point for the later STFT/pitch/formant processing.
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

    smoothedPitchShiftSemitones.skip(numSamples);
    smoothedFormantShiftSemitones.skip(numSamples);
    smoothedFormantMix.skip(numSamples);
    smoothedMix.skip(numSamples);
}
