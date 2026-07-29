#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

class ToneEffect
{
public:
    void prepareToPlay(double sampleRate);

    void setEnabled(bool shouldBeEnabled);
    void setHighPassHz(float hz);
    void setLowPassHz(float hz);
    void setDrive(float amount);
    void setCompression(float amount);
    void setNoise(float amount);

    float processSample(float inputSample, int channel);

private:
    void updateFilters();

    std::atomic<bool> enabled { false };
    std::atomic<float> highPassHz { 120.0f };
    std::atomic<float> lowPassHz { 8000.0f };
    std::atomic<float> drive { 0.0f };
    std::atomic<float> compression { 0.0f };
    std::atomic<float> noise { 0.0f };

    double currentSampleRate = 44100.0;
    float envelope[2] { 0.0f, 0.0f };
    float crackleEnvelope[2] { 0.0f, 0.0f };
    float flutter[2] { 0.0f, 0.0f };
    unsigned int randomState[2] { 0x12345678u, 0x87654321u };

    juce::dsp::IIR::Filter<float> highPassFilters[2];
    juce::dsp::IIR::Filter<float> lowPassFilters[2];
};
