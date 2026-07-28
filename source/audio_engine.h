#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

class AudioEngine
{
public:
    void setVoiceEnabled(bool shouldBeEnabled);
    void setOutputGain(float newGain);

    bool isVoiceEnabled() const;
    float getOutputGain() const;

    void processBlock(
        const juce::AudioSourceChannelInfo& bufferToFill,
        const juce::AudioIODevice& device
    );

private:
    std::atomic<float> outputGain { 2.0f };
    std::atomic<bool> voiceEnabled { false };
};