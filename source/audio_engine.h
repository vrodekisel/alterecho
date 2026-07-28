#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

#include "echo_effect.h"

class AudioEngine
{
public:
    void prepareToPlay(double sampleRate);

    void setVoiceEnabled(bool shouldBeEnabled);
    void setInputGain(float newGain);
    void setOutputGain(float newGain);
    void setEffectsBypassed(bool shouldBeBypassed);

    void setEchoEnabled(bool shouldBeEnabled);
    void setEchoDelayMs(float newDelayMs);
    void setEchoFeedback(float newFeedback);
    void setEchoMix(float newMix);

    bool isVoiceEnabled() const;
    float getInputGain() const;
    float getOutputGain() const;
    bool areEffectsBypassed() const;
    float getInputLevel() const;
    float getOutputLevel() const;

    void processBlock(
        const juce::AudioSourceChannelInfo& bufferToFill,
        const juce::AudioIODevice& device
    );

private:
    std::atomic<float> inputGain { 1.0f };
    std::atomic<float> outputGain { 1.0f };
    std::atomic<bool> voiceEnabled { false };
    std::atomic<bool> effectsBypassed { false };
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

    EchoEffect echoEffect;
};
