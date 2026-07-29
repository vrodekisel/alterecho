#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <vector>

class VoiceShiftEffect
{
public:
    void prepareToPlay(double sampleRate, int maximumBlockSize);

    void setEnabled(bool shouldBeEnabled);
    void setPitchShiftSemitones(float semitones);
    void setFormantShiftSemitones(float semitones);
    void setFormantMix(float amount);
    void setMix(float amount);

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        int startSample,
        int numSamples
    );

private:
    void updateSmoothedParameters(int numSamples);
    float processPitchShiftSample(float inputSample, int channel, float semitones);
    float readPitchBuffer(int channel, float delaySamples) const;

    std::atomic<bool> enabled { false };
    std::atomic<float> pitchShiftSemitones { 0.0f };
    std::atomic<float> formantShiftSemitones { 0.0f };
    std::atomic<float> formantMix { 0.0f };
    std::atomic<float> mix { 1.0f };

    double currentSampleRate = 44100.0;
    int currentMaximumBlockSize = 512;
    float pitchPhase[2] { 0.0f, 0.5f };
    int pitchWriteIndex[2] { 0, 0 };
    int pitchSamplesWritten[2] { 0, 0 };
    std::vector<float> pitchBuffer[2];

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPitchShiftSemitones;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFormantShiftSemitones;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFormantMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;
};
