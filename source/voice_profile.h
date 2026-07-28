#pragma once

#include <juce_core/juce_core.h>

#include <vector>

enum class VoiceProfileId
{
    echo,
    radio,
    female1,
    female2,
    female3,
    male1,
    male2,
    male3,
    narrator,
    robot,
    monster,
    alien
};

enum class VoiceControlId
{
    tone,
    depth,
    brightness,
    amount,
    mix
};

struct VoiceControl
{
    VoiceControlId id;
    juce::String name;
    float defaultValue = 0.5f;
};

struct TechnicalVoiceParameters
{
    bool echoEnabled = false;
    float echoDelayMs = 350.0f;
    float echoFeedback = 0.0f;
    float echoMix = 0.0f;
};

struct VoiceProfile
{
    VoiceProfileId id;
    juce::String name;
    juce::String description;
    std::vector<VoiceControl> controls;
};

juce::String getVoiceProfileKey(VoiceProfileId id);
juce::String getVoiceControlKey(VoiceControlId id);

const std::vector<VoiceProfile>& getVoiceProfiles();
const VoiceProfile& getVoiceProfile(VoiceProfileId id);
VoiceProfileId getVoiceProfileIdFromKey(const juce::String& key);
TechnicalVoiceParameters mapVoiceProfileToTechnicalParameters(
    const VoiceProfile& profile,
    const std::vector<float>& controlValues
);
