#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

class EchoEffect
{
public:
    void prepareToPlay(double sampleRate);

    void setEnabled(bool shouldBeEnabled);
    void setDelayMs(float newDelayMs);
    void setFeedback(float newFeedback);
    void setMix(float newMix);

    bool isEnabled() const;
    float getDelayMs() const;
    float getFeedback() const;
    float getMix() const;

    float processSample(float inputSample, int channel);

private:
    static constexpr double maxDelaySeconds = 2.0;

    std::atomic<bool> enabled { false };
    std::atomic<float> delayMs { 350.0f };
    std::atomic<float> feedback { 0.35f };
    std::atomic<float> mix { 0.35f };

    double currentSampleRate = 44100.0;
    int delayWritePosition = 0;

    juce::AudioBuffer<float> delayBuffer;
};