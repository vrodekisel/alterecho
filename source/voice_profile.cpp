#include "voice_profile.h"

namespace
{
VoiceControl makeControl(VoiceControlId id, const juce::String& name, float defaultValue)
{
    return VoiceControl { id, name, juce::jlimit(0.0f, 1.0f, defaultValue) };
}

TechnicalVoiceParameters makeInactiveEffectParameters()
{
    return {};
}

TechnicalVoiceParameters makeEchoParameters(const std::vector<float>& controlValues)
{
    auto tone = controlValues.size() > 0 ? controlValues[0] : 0.5f;
    auto depth = controlValues.size() > 1 ? controlValues[1] : 0.5f;
    auto amount = controlValues.size() > 2 ? controlValues[2] : 0.45f;
    auto mix = controlValues.size() > 3 ? controlValues[3] : 0.35f;

    TechnicalVoiceParameters parameters;
    parameters.echoEnabled = mix > 0.01f && amount > 0.01f;
    parameters.echoDelayMs = juce::jmap(tone, 90.0f, 680.0f);
    parameters.echoFeedback = juce::jlimit(0.0f, 0.72f, juce::jmap(depth, 0.08f, 0.64f) * amount);
    parameters.echoMix = juce::jlimit(0.0f, 0.62f, juce::jmap(mix, 0.0f, 0.62f));
    return parameters;
}
TechnicalVoiceParameters makeRadioParameters(const std::vector<float>& controlValues)
{
    auto tone = controlValues.size() > 0 ? controlValues[0] : 0.5f;
    auto brightness = controlValues.size() > 1 ? controlValues[1] : 0.58f;
    auto amount = controlValues.size() > 2 ? controlValues[2] : 0.5f;
    auto mix = controlValues.size() > 3 ? controlValues[3] : 0.35f;

    TechnicalVoiceParameters parameters;
    parameters.toneEnabled = amount > 0.01f;

    parameters.highPassHz = juce::jmap(tone, 240.0f, 860.0f);
    parameters.lowPassHz = juce::jmap(brightness, 1450.0f, 4300.0f);
    parameters.drive = juce::jlimit(0.0f, 0.72f, amount * 0.72f);
    parameters.compression = juce::jlimit(0.0f, 0.86f, amount * 0.86f);
    parameters.noise = juce::jlimit(0.0f, 0.92f, mix * amount * 1.28f);

    return parameters;
}

}

juce::String getVoiceProfileKey(VoiceProfileId id)
{
    switch (id)
    {
        case VoiceProfileId::echo: return "echo";
        case VoiceProfileId::radio: return "radio";
        case VoiceProfileId::female1: return "female1";
        case VoiceProfileId::female2: return "female2";
        case VoiceProfileId::female3: return "female3";
        case VoiceProfileId::male1: return "male1";
        case VoiceProfileId::male2: return "male2";
        case VoiceProfileId::male3: return "male3";
        case VoiceProfileId::narrator: return "narrator";
        case VoiceProfileId::robot: return "robot";
        case VoiceProfileId::monster: return "monster";
        case VoiceProfileId::alien: return "alien";
    }

    return "echo";
}

juce::String getVoiceControlKey(VoiceControlId id)
{
    switch (id)
    {
        case VoiceControlId::tone: return "tone";
        case VoiceControlId::depth: return "depth";
        case VoiceControlId::brightness: return "brightness";
        case VoiceControlId::amount: return "amount";
        case VoiceControlId::mix: return "mix";
    }

    return "amount";
}

const std::vector<VoiceProfile>& getVoiceProfiles()
{
    static const std::vector<VoiceProfile> profiles {
        {
            VoiceProfileId::echo,
            "Echo",
            "Space and repeats",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.45f),
                makeControl(VoiceControlId::depth, "Depth", 0.45f),
                makeControl(VoiceControlId::amount, "Amount", 0.65f),
                makeControl(VoiceControlId::mix, "Mix", 0.35f)
            }
        },
        {
            VoiceProfileId::radio,
            "Radio",
            "Tight broadcast color",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.5f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.58f),
                makeControl(VoiceControlId::amount, "Amount", 0.72f),
                makeControl(VoiceControlId::mix, "Mix", 0.48f)
            }
        },
        {
            VoiceProfileId::female1,
            "Female 1",
            "High female voice",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.68f),
                makeControl(VoiceControlId::depth, "Depth", 0.36f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.62f),
                makeControl(VoiceControlId::amount, "Amount", 0.56f)
            }
        },
        {
            VoiceProfileId::female2,
            "Female 2",
            "Neutral female voice",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.58f),
                makeControl(VoiceControlId::depth, "Depth", 0.46f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.56f),
                makeControl(VoiceControlId::amount, "Amount", 0.5f)
            }
        },
        {
            VoiceProfileId::female3,
            "Female 3",
            "Deep female voice",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.48f),
                makeControl(VoiceControlId::depth, "Depth", 0.58f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.48f),
                makeControl(VoiceControlId::amount, "Amount", 0.52f)
            }
        },
        {
            VoiceProfileId::male1,
            "Male 1",
            "Alt tenor voice",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.48f),
                makeControl(VoiceControlId::depth, "Depth", 0.46f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.54f),
                makeControl(VoiceControlId::amount, "Amount", 0.48f)
            }
        },
        {
            VoiceProfileId::male2,
            "Male 2",
            "Neutral male voice",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.42f),
                makeControl(VoiceControlId::depth, "Depth", 0.56f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.48f),
                makeControl(VoiceControlId::amount, "Amount", 0.5f)
            }
        },
        {
            VoiceProfileId::male3,
            "Male 3",
            "Deep male voice",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.34f),
                makeControl(VoiceControlId::depth, "Depth", 0.68f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.42f),
                makeControl(VoiceControlId::amount, "Amount", 0.52f)
            }
        },
        {
            VoiceProfileId::narrator,
            "Narrator",
            "Natural narration",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.45f),
                makeControl(VoiceControlId::depth, "Depth", 0.52f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.52f),
                makeControl(VoiceControlId::amount, "Amount", 0.42f)
            }
        },
        {
            VoiceProfileId::robot,
            "Robot",
            "Mechanical character",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.5f),
                makeControl(VoiceControlId::depth, "Depth", 0.72f),
                makeControl(VoiceControlId::amount, "Amount", 0.72f),
                makeControl(VoiceControlId::mix, "Mix", 0.58f)
            }
        },
        {
            VoiceProfileId::monster,
            "Monster",
            "Large distorted voice",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.24f),
                makeControl(VoiceControlId::depth, "Depth", 0.86f),
                makeControl(VoiceControlId::amount, "Amount", 0.74f),
                makeControl(VoiceControlId::mix, "Mix", 0.38f)
            }
        },
        {
            VoiceProfileId::alien,
            "Alien",
            "Unusual animated voice",
            {
                makeControl(VoiceControlId::tone, "Tone", 0.62f),
                makeControl(VoiceControlId::depth, "Depth", 0.64f),
                makeControl(VoiceControlId::brightness, "Brightness", 0.66f),
                makeControl(VoiceControlId::amount, "Amount", 0.68f),
                makeControl(VoiceControlId::mix, "Mix", 0.45f)
            }
        }
    };

    return profiles;
}

const VoiceProfile& getVoiceProfile(VoiceProfileId id)
{
    for (const auto& profile : getVoiceProfiles())
    {
        if (profile.id == id)
            return profile;
    }

    return getVoiceProfiles().front();
}

VoiceProfileId getVoiceProfileIdFromKey(const juce::String& key)
{
    for (const auto& profile : getVoiceProfiles())
    {
        if (getVoiceProfileKey(profile.id) == key)
            return profile.id;
    }

    return VoiceProfileId::echo;
}

TechnicalVoiceParameters mapVoiceProfileToTechnicalParameters(
    const VoiceProfile& profile,
    const std::vector<float>& controlValues
)
{
    if (profile.id == VoiceProfileId::echo)
        return makeEchoParameters(controlValues);

    if (profile.id == VoiceProfileId::radio)
        return makeRadioParameters(controlValues);

    return makeInactiveEffectParameters();
}
