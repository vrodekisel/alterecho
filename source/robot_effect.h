#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

class RobotEffect
{
public:
    void prepareToPlay(double sampleRate);

    void setEnabled(bool shouldBeEnabled);
    void setFrequencyHz(float hz);
    void setDepth(float amount);
    void setCrush(float amount);
    void setMix(float amount);
    void setPitchShiftSemitones(float semitones);

    float processSample(float inputSample, int channel);

private:
    float processPitchShift(float inputSample, int channel);
    float readPitchBuffer(int channel, float delaySamples) const;
    float getNextModulatorSample(int channel);
    float crushSample(float sample, int channel);

    std::atomic<bool> enabled { false };
    std::atomic<float> frequencyHz { 42.0f };
    std::atomic<float> depth { 0.0f };
    std::atomic<float> crush { 0.0f };
    std::atomic<float> mix { 0.0f };
    std::atomic<float> pitchShiftSemitones { 0.0f };

    double currentSampleRate = 44100.0;
    float phase[2] { 0.0f, 0.25f };
    float pitchPhase[2] { 0.0f, 0.5f };
    float heldSample[2] { 0.0f, 0.0f };
    int holdCounter[2] { 0, 0 };
    int pitchWriteIndex[2] { 0, 0 };
    int pitchSamplesWritten[2] { 0, 0 };
    std::vector<float> pitchBuffer[2];
};
