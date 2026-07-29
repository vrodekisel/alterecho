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
    void setPitchTrackingEnabled(bool shouldBeEnabled);
    void setTargetPitchHz(float hz);
    void setPitchTrackingMix(float amount);
    void setPitchShiftRange(float minimumSemitones, float maximumSemitones);

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        int startSample,
        int numSamples
    );

private:
    void updateSmoothedParameters(int numSamples);
    void writePitchTrackingInput(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    float estimateInputPitchHz() const;
    float calculateAdaptivePitchShiftSemitones(float detectedPitchHz) const;
    float processPitchShiftSample(float inputSample, int channel, float semitones);
    float readPitchBuffer(int channel, float delaySamples) const;

    std::atomic<bool> enabled { false };
    std::atomic<float> pitchShiftSemitones { 0.0f };
    std::atomic<float> formantShiftSemitones { 0.0f };
    std::atomic<float> formantMix { 0.0f };
    std::atomic<float> mix { 1.0f };
    std::atomic<bool> pitchTrackingEnabled { false };
    std::atomic<float> targetPitchHz { 0.0f };
    std::atomic<float> pitchTrackingMix { 0.0f };
    std::atomic<float> minPitchShiftSemitones { -24.0f };
    std::atomic<float> maxPitchShiftSemitones { 24.0f };

    double currentSampleRate = 44100.0;
    int currentMaximumBlockSize = 512;
    float pitchPhase[2] { 0.0f, 0.5f };
    int pitchWriteIndex[2] { 0, 0 };
    int pitchSamplesWritten[2] { 0, 0 };
    std::vector<float> pitchBuffer[2];
    std::vector<float> pitchTrackingBuffer;
    int pitchTrackingWriteIndex = 0;
    int pitchTrackingSamplesWritten = 0;
    int pitchTrackingSamplesUntilAnalysis = 0;
    float lastDetectedPitchHz = 0.0f;
    float lastAdaptivePitchShiftSemitones = 0.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPitchShiftSemitones;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFormantShiftSemitones;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFormantMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAdaptivePitchShiftSemitones;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPitchTrackingMix;
};
