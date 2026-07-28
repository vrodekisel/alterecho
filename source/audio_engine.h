#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

class AudioEngine
{
public:
    void prepareToPlay(double sampleRate);

    void setVoiceEnabled(bool shouldBeEnabled);
    void setOutputGain(float newGain);

    void setEchoEnabled(bool shouldBeEnabled);
    void setEchoDelayMs(float newDelayMs);
    void setEchoFeedback(float newFeedback);
    void setEchoMix(float newMix);

    bool isVoiceEnabled() const;
    float getOutputGain() const;

    bool isEchoEnabled() const;
    float getEchoDelayMs() const;
    float getEchoFeedback() const;
    float getEchoMix() const;

    void processBlock(
        const juce::AudioSourceChannelInfo& bufferToFill,
        const juce::AudioIODevice& device
    );

private:
    static constexpr double maxDelaySeconds = 2.0;

    std::atomic<float> outputGain { 2.0f };
    std::atomic<bool> voiceEnabled { false };

    std::atomic<bool> echoEnabled { false };
    std::atomic<float> echoDelayMs { 350.0f };
    std::atomic<float> echoFeedback { 0.35f };
    std::atomic<float> echoMix { 0.35f };

    double currentSampleRate = 44100.0;
    int delayWritePosition = 0;

    juce::AudioBuffer<float> delayBuffer;
};