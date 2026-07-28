#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <array>

#include "audio_engine.h"
#include "voice_profile.h"

class ProfileCardButton final : public juce::Button
{
public:
    explicit ProfileCardButton(const VoiceProfile& profileToUse)
        : juce::Button(profileToUse.name), profile(profileToUse)
    {
    }

    VoiceProfileId getProfileId() const
    {
        return profile.id;
    }

    void paintButton(
        juce::Graphics& g,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown
    ) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto selected = getToggleState();
        auto baseColour = selected
            ? juce::Colour::fromRGB(42, 62, 66)
            : juce::Colour::fromRGB(31, 44, 49);

        if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.08f);

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.08f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 7.0f);

        g.setColour(selected ? juce::Colour::fromRGB(119, 185, 198)
                             : juce::Colour::fromRGB(118, 135, 141));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 7.0f, selected ? 2.0f : 1.0f);

        auto imageArea = getLocalBounds().reduced(12).removeFromTop(
            juce::jmax(48, getHeight() - 50)
        );

        g.setColour(juce::Colour::fromRGB(23, 32, 36));
        g.fillRoundedRectangle(imageArea.toFloat(), 6.0f);

        g.setColour(juce::Colour::fromRGB(70, 86, 92));
        g.drawRoundedRectangle(imageArea.toFloat(), 6.0f, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.setFont(18.0f);
        g.drawText(
            profile.name,
            getLocalBounds().removeFromBottom(38).reduced(8, 0),
            juce::Justification::centred
        );
    }

private:
    const VoiceProfile& profile;
};

class ProfileDrawerBackground final : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.setColour(juce::Colour::fromRGB(21, 25, 29));
        g.fillRect(bounds);

        g.setColour(juce::Colour::fromRGB(56, 67, 72));
        g.drawVerticalLine(0, 0.0f, static_cast<float>(bounds.getHeight()));

        g.setColour(juce::Colour::fromRGB(30, 40, 45));
        g.fillRoundedRectangle(imageArea.toFloat(), 8.0f);

        g.setColour(juce::Colour::fromRGB(75, 91, 98));
        g.drawRoundedRectangle(imageArea.toFloat(), 8.0f, 1.0f);
    }

    void setImageArea(juce::Rectangle<int> newImageArea)
    {
        imageArea = newImageArea;
        repaint();
    }

private:
    juce::Rectangle<int> imageArea;
};

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer
{
public:
    MainComponent()
    {
        setSize(900, 680);

        setAudioChannels(1, 2);
        configureSettings();
        initialiseProfileControlValues();
        loadSettings();

        configureNavigationIcons();

        navigationButton.onClick = [this]
        {
            showingSettings = !showingSettings;
            updateScreenVisibility();
            resized();
            repaint();
        };

        addAndMakeVisible(navigationButton);
        addAndMakeVisible(contentViewport);
        contentViewport.setViewedComponent(&contentComponent, false);
        contentViewport.setScrollBarsShown(true, false);

        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManager,
            1, 2,
            1, 2,
            true,
            false,
            true,
            false
        );

        contentComponent.addAndMakeVisible(*deviceSelector);

        inputGainSlider.setRange(0.0, 4.0, 0.01);
        inputGainSlider.setValue(inputGain, juce::dontSendNotification);
        inputGainSlider.setTextValueSuffix("x");
        inputGainSlider.onValueChange = [this]
        {
            inputGain = static_cast<float>(inputGainSlider.getValue());
            audioEngine.setInputGain(inputGain);
            saveGlobalSettings();
        };

        gainSlider.setRange(0.0, 4.0, 0.01);
        gainSlider.setValue(outputGain, juce::dontSendNotification);
        gainSlider.setTextValueSuffix("x");

        gainSlider.onValueChange = [this]
        {
            outputGain = static_cast<float>(gainSlider.getValue());
            audioEngine.setOutputGain(outputGain);
            saveGlobalSettings();
        };

        voiceButton.setButtonText("voice off");
        voiceButton.setClickingTogglesState(true);
        voiceButton.setToggleState(false, juce::dontSendNotification);
        voiceButton.onClick = [this]
        {
            auto isEnabled = voiceButton.getToggleState();

            audioEngine.setVoiceEnabled(isEnabled);
            voiceButton.setButtonText(isEnabled ? "voice on" : "voice off");
        };

        bypassButton.setButtonText("effects on");
        bypassButton.setClickingTogglesState(true);
        bypassButton.setToggleState(false, juce::dontSendNotification);
        bypassButton.onClick = [this]
        {
            auto isEnabled = bypassButton.getToggleState();

            audioEngine.setEffectsBypassed(isEnabled);
            bypassButton.setButtonText(isEnabled ? "effects off" : "effects on");
        };

        profileLabel.setText("profiles", juce::dontSendNotification);
        profileLabel.setJustificationType(juce::Justification::centredLeft);

        profileDrawerTitle.setJustificationType(juce::Justification::centredLeft);
        profileDrawerTitle.setFont(juce::Font(24.0f, juce::Font::bold));
        profileDrawerDescription.setJustificationType(juce::Justification::topLeft);
        profileDrawerDescription.setColour(
            juce::Label::textColourId,
            juce::Colours::white.withAlpha(0.78f)
        );

        closeProfileDrawerButton.setButtonText("x");
        closeProfileDrawerButton.onClick = [this]
        {
            profileDrawerOpen = false;
            updateProfilePanel();
            resized();
            repaint();
        };

        configureProfileButtons();

        resetProfileButton.setButtonText("reset profile");
        resetProfileButton.onClick = [this]
        {
            resetCurrentProfileToDefaults();
        };

        for (auto index = 0; index < static_cast<int>(controlSliders.size()); ++index)
        {
            auto& slider = controlSliders[static_cast<size_t>(index)];
            auto& label = controlLabels[static_cast<size_t>(index)];

            slider.setRange(0.0, 1.0, 0.01);
            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 58, 24);
            slider.setTextValueSuffix("");
            slider.onValueChange = [this, index]
            {
                updateCurrentControlValue(index, static_cast<float>(
                    controlSliders[static_cast<size_t>(index)].getValue()
                ));
            };

            label.setJustificationType(juce::Justification::centredLeft);

            addAndMakeVisible(slider);
            addAndMakeVisible(label);
        }

        gainLabel.setText("output", juce::dontSendNotification);
        gainLabel.attachToComponent(&gainSlider, true);

        inputGainLabel.setText("input", juce::dontSendNotification);
        inputGainLabel.attachToComponent(&inputGainSlider, true);

        inputLevelLabel.setText("input level", juce::dontSendNotification);
        outputLevelLabel.setText("output level", juce::dontSendNotification);

        inputLevelMeter.setPercentageDisplay(false);
        outputLevelMeter.setPercentageDisplay(false);

        contentComponent.addAndMakeVisible(voiceButton);
        contentComponent.addAndMakeVisible(bypassButton);
        contentComponent.addAndMakeVisible(profileLabel);
        contentComponent.addAndMakeVisible(inputGainSlider);
        contentComponent.addAndMakeVisible(inputGainLabel);
        contentComponent.addAndMakeVisible(gainSlider);
        contentComponent.addAndMakeVisible(gainLabel);
        contentComponent.addAndMakeVisible(inputLevelLabel);
        contentComponent.addAndMakeVisible(inputLevelMeter);
        contentComponent.addAndMakeVisible(outputLevelLabel);
        contentComponent.addAndMakeVisible(outputLevelMeter);

        addAndMakeVisible(profileDrawerBackground);
        addAndMakeVisible(profileDrawerTitle);
        addAndMakeVisible(profileDrawerDescription);
        addAndMakeVisible(closeProfileDrawerButton);
        addAndMakeVisible(resetProfileButton);

        audioEngine.setInputGain(inputGain);
        audioEngine.setOutputGain(outputGain);
        applySelectedVoiceProfile();
        updateScreenVisibility();
        resized();
        startTimerHz(30);
    }

    ~MainComponent() override
    {
        shutdownAudio();
    }

    void prepareToPlay(int, double sampleRate) override
    {
        audioEngine.prepareToPlay(sampleRate);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* device = deviceManager.getCurrentAudioDevice();

        if (device == nullptr)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        audioEngine.processBlock(bufferToFill, *device);
    }

    void releaseResources() override {}

    void timerCallback() override
    {
        inputLevelValue = audioEngine.getInputLevel();
        outputLevelValue = audioEngine.getOutputLevel();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour::fromRGB(18, 18, 22));

        g.setColour(juce::Colours::white);
        g.setFont(24.0f);
        g.drawText("alterecho", getLocalBounds().removeFromTop(56),
                   juce::Justification::centred);

    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(24);

        auto headerArea = bounds.removeFromTop(56);
        navigationButton.setBounds(headerArea.removeFromLeft(40).reduced(6));

        contentViewport.setBounds(bounds);
        contentViewport.setScrollBarsShown(!showingSettings, false);

        auto contentHeight = showingSettings
            ? juce::jmax(1, contentViewport.getMaximumVisibleHeight())
            : juce::jmax(1180, contentViewport.getMaximumVisibleHeight());
        contentComponent.setSize(
            juce::jmax(1, contentViewport.getMaximumVisibleWidth()),
            contentHeight
        );

        if (showingSettings)
        {
            if (deviceSelector != nullptr)
                deviceSelector->setBounds(contentComponent.getLocalBounds());

            return;
        }

        auto controlsArea = contentComponent.getLocalBounds().withSizeKeepingCentre(
            juce::jmin(760, contentComponent.getWidth()),
            1120
        );

        auto voiceArea = controlsArea.removeFromTop(44);
        voiceButton.setBounds(voiceArea.withSizeKeepingCentre(160, 40));

        controlsArea.removeFromTop(20);

        auto bypassArea = controlsArea.removeFromTop(44);
        bypassButton.setBounds(bypassArea.withSizeKeepingCentre(160, 40));

        controlsArea.removeFromTop(18);

        auto profileHeaderArea = controlsArea.removeFromTop(30);
        profileLabel.setBounds(profileHeaderArea.removeFromLeft(120));

        controlsArea.removeFromTop(8);

        auto profileGridArea = controlsArea.removeFromTop(696);
        layoutProfileButtons(profileGridArea);

        controlsArea.removeFromTop(18);

        auto inputGainArea = controlsArea.removeFromTop(36);
        inputGainArea.removeFromLeft(120);
        inputGainSlider.setBounds(inputGainArea);

        controlsArea.removeFromTop(12);

        auto gainArea = controlsArea.removeFromTop(36);
        gainArea.removeFromLeft(120);
        gainSlider.setBounds(gainArea);

        controlsArea.removeFromTop(14);

        auto inputMeterArea = controlsArea.removeFromTop(22);
        inputLevelLabel.setBounds(inputMeterArea.removeFromLeft(120));
        inputLevelMeter.setBounds(inputMeterArea);

        controlsArea.removeFromTop(8);

        auto outputMeterArea = controlsArea.removeFromTop(22);
        outputLevelLabel.setBounds(outputMeterArea.removeFromLeft(120));
        outputLevelMeter.setBounds(outputMeterArea);

        layoutProfileDrawer();
    }

    void configureSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "alterecho";
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Application Support";

        applicationProperties.setStorageParameters(options);
    }

    void initialiseProfileControlValues()
    {
        profileControlValues.clear();

        for (const auto& profile : getVoiceProfiles())
        {
            std::vector<float> values;

            for (const auto& control : profile.controls)
                values.push_back(control.defaultValue);

            profileControlValues.push_back(values);
        }
    }

    void loadSettings()
    {
        auto* settings = applicationProperties.getUserSettings();

        if (settings == nullptr)
            return;

        selectedProfileId = getVoiceProfileIdFromKey(
            settings->getValue("selectedProfile", getVoiceProfileKey(VoiceProfileId::echo))
        );
        inputGain = static_cast<float>(settings->getDoubleValue("inputGain", 1.0));
        outputGain = static_cast<float>(settings->getDoubleValue("outputGain", 1.0));

        for (const auto& profile : getVoiceProfiles())
        {
            auto profileIndex = getProfileIndex(profile.id);

            if (profileIndex < 0)
                continue;

            auto& values = profileControlValues[static_cast<size_t>(profileIndex)];

            for (auto controlIndex = 0; controlIndex < static_cast<int>(profile.controls.size()); ++controlIndex)
            {
                auto key = getSettingKey(profile, profile.controls[static_cast<size_t>(controlIndex)]);
                values[static_cast<size_t>(controlIndex)] = juce::jlimit(
                    0.0f,
                    1.0f,
                    static_cast<float>(settings->getDoubleValue(
                        key,
                        profile.controls[static_cast<size_t>(controlIndex)].defaultValue
                    ))
                );
            }
        }
    }

    void saveGlobalSettings()
    {
        auto* settings = applicationProperties.getUserSettings();

        if (settings == nullptr)
            return;

        settings->setValue("selectedProfile", getVoiceProfileKey(selectedProfileId));
        settings->setValue("inputGain", static_cast<double>(inputGain));
        settings->setValue("outputGain", static_cast<double>(outputGain));
        settings->saveIfNeeded();
    }

    void saveCurrentProfileSettings()
    {
        auto* settings = applicationProperties.getUserSettings();

        if (settings == nullptr)
            return;

        auto profileIndex = getProfileIndex(selectedProfileId);

        if (profileIndex < 0)
            return;

        const auto& profile = getVoiceProfile(selectedProfileId);
        const auto& values = profileControlValues[static_cast<size_t>(profileIndex)];

        for (auto controlIndex = 0; controlIndex < static_cast<int>(profile.controls.size()); ++controlIndex)
        {
            settings->setValue(
                getSettingKey(profile, profile.controls[static_cast<size_t>(controlIndex)]),
                static_cast<double>(values[static_cast<size_t>(controlIndex)])
            );
        }

        settings->saveIfNeeded();
    }

    juce::String getSettingKey(const VoiceProfile& profile, const VoiceControl& control) const
    {
        return "profile." + getVoiceProfileKey(profile.id) + "." + getVoiceControlKey(control.id);
    }

    int getProfileIndex(VoiceProfileId id) const
    {
        const auto& profiles = getVoiceProfiles();

        for (auto index = 0; index < static_cast<int>(profiles.size()); ++index)
        {
            if (profiles[static_cast<size_t>(index)].id == id)
                return index;
        }

        return -1;
    }

    void configureProfileButtons()
    {
        constexpr auto profileRadioGroupId = 1001;

        for (const auto& profile : getVoiceProfiles())
        {
            auto button = std::make_unique<ProfileCardButton>(profile);
            button->setRadioGroupId(profileRadioGroupId);
            button->setClickingTogglesState(true);
            button->setToggleState(profile.id == selectedProfileId, juce::dontSendNotification);
            button->onClick = [this, profileId = profile.id]
            {
                selectProfile(profileId);
            };

            contentComponent.addAndMakeVisible(*button);
            profileButtons.push_back(std::move(button));
        }
    }

    void selectProfile(VoiceProfileId profileId)
    {
        selectedProfileId = profileId;
        profileDrawerOpen = true;

        for (auto index = 0; index < static_cast<int>(getVoiceProfiles().size()); ++index)
        {
            auto shouldBeSelected = getVoiceProfiles()[static_cast<size_t>(index)].id == selectedProfileId;
            profileButtons[static_cast<size_t>(index)]->setToggleState(
                shouldBeSelected,
                juce::dontSendNotification
            );
        }

        applySelectedVoiceProfile();
        saveGlobalSettings();
        resized();
        repaint();
    }

    void updateCurrentControlValue(int controlIndex, float value)
    {
        auto profileIndex = getProfileIndex(selectedProfileId);

        if (profileIndex < 0)
            return;

        auto& values = profileControlValues[static_cast<size_t>(profileIndex)];

        if (controlIndex < 0 || controlIndex >= static_cast<int>(values.size()))
            return;

        values[static_cast<size_t>(controlIndex)] = juce::jlimit(0.0f, 1.0f, value);
        applySelectedVoiceProfile();
        saveCurrentProfileSettings();
    }

    void resetCurrentProfileToDefaults()
    {
        auto profileIndex = getProfileIndex(selectedProfileId);

        if (profileIndex < 0)
            return;

        const auto& profile = getVoiceProfile(selectedProfileId);
        auto& values = profileControlValues[static_cast<size_t>(profileIndex)];
        values.clear();

        for (const auto& control : profile.controls)
            values.push_back(control.defaultValue);

        applySelectedVoiceProfile();
        saveCurrentProfileSettings();
    }

    void applySelectedVoiceProfile()
    {
        auto profileIndex = getProfileIndex(selectedProfileId);

        if (profileIndex < 0)
            profileIndex = 0;

        const auto& profile = getVoiceProfile(selectedProfileId);
        const auto& values = profileControlValues[static_cast<size_t>(profileIndex)];

        audioEngine.setVoiceProfileParameters(
            mapVoiceProfileToTechnicalParameters(profile, values)
        );

        updateProfilePanel();
    }

    void updateProfilePanel()
    {
        const auto& profile = getVoiceProfile(selectedProfileId);
        auto profileIndex = getProfileIndex(selectedProfileId);

        profileDrawerTitle.setText(profile.name, juce::dontSendNotification);
        profileDrawerDescription.setText(profile.description, juce::dontSendNotification);

        if (profileIndex < 0)
            return;

        const auto& values = profileControlValues[static_cast<size_t>(profileIndex)];

        for (auto index = 0; index < static_cast<int>(controlSliders.size()); ++index)
        {
            auto hasControl = index < static_cast<int>(profile.controls.size());
            auto& slider = controlSliders[static_cast<size_t>(index)];
            auto& label = controlLabels[static_cast<size_t>(index)];

            slider.setVisible(!showingSettings && profileDrawerOpen && hasControl);
            label.setVisible(!showingSettings && profileDrawerOpen && hasControl);

            if (!hasControl)
                continue;

            label.setText(profile.controls[static_cast<size_t>(index)].name, juce::dontSendNotification);
            slider.setValue(values[static_cast<size_t>(index)], juce::dontSendNotification);
        }

        auto drawerVisible = !showingSettings && profileDrawerOpen;
        profileDrawerBackground.setVisible(drawerVisible);
        profileDrawerTitle.setVisible(drawerVisible);
        profileDrawerDescription.setVisible(drawerVisible);
        closeProfileDrawerButton.setVisible(drawerVisible);
        resetProfileButton.setVisible(drawerVisible && !profile.controls.empty());
    }

    void layoutProfileDrawer()
    {
        if (!profileDrawerOpen || showingSettings)
        {
            profileDrawerBackground.setBounds({});
            profileDrawerTitle.setBounds({});
            profileDrawerDescription.setBounds({});
            closeProfileDrawerButton.setBounds({});
            resetProfileButton.setBounds({});
            layoutProfileControls({});
            return;
        }

        auto drawerWidth = getProfileDrawerWidth();
        auto drawerBounds = getLocalBounds().removeFromRight(drawerWidth);
        profileDrawerBackground.setBounds(drawerBounds);

        auto drawerArea = drawerBounds.reduced(22, 74);
        auto headerArea = drawerArea.removeFromTop(36);

        closeProfileDrawerButton.setBounds(headerArea.removeFromRight(34));
        profileDrawerTitle.setBounds(headerArea);

        drawerArea.removeFromTop(14);

        auto imageSlot = drawerArea.removeFromTop(230);
        profileImagePlaceholder = imageSlot.withSizeKeepingCentre(
            juce::jmin(180, imageSlot.getWidth()),
            imageSlot.getHeight()
        );
        profileDrawerBackground.setImageArea(
            profileImagePlaceholder.translated(-drawerBounds.getX(), -drawerBounds.getY())
        );
        drawerArea.removeFromTop(10);

        profileDrawerDescription.setBounds(drawerArea.removeFromTop(42));
        drawerArea.removeFromTop(12);

        layoutProfileControls(drawerArea.removeFromTop(166));
        drawerArea.removeFromTop(12);

        resetProfileButton.setBounds(
            drawerArea.removeFromTop(42).withSizeKeepingCentre(190, 38)
        );

        profileDrawerBackground.toBack();
        profileDrawerBackground.toFront(false);
        profileDrawerTitle.toFront(false);
        profileDrawerDescription.toFront(false);
        closeProfileDrawerButton.toFront(false);
        resetProfileButton.toFront(false);

        for (auto& slider : controlSliders)
            slider.toFront(false);

        for (auto& label : controlLabels)
            label.toFront(false);
    }

    int getProfileDrawerWidth() const
    {
        return juce::jmin(330, juce::jmax(280, getWidth() / 3));
    }

    void layoutProfileButtons(juce::Rectangle<int> area)
    {
        constexpr auto columns = 4;
        constexpr auto buttonHeight = 220;
        constexpr auto gap = 12;
        auto buttonWidth = (area.getWidth() - gap * (columns - 1)) / columns;

        for (auto index = 0; index < static_cast<int>(profileButtons.size()); ++index)
        {
            auto row = index / columns;
            auto column = index % columns;
            auto buttonArea = juce::Rectangle<int>(
                area.getX() + column * (buttonWidth + gap),
                area.getY() + row * (buttonHeight + gap),
                buttonWidth,
                buttonHeight
            );

            profileButtons[static_cast<size_t>(index)]->setBounds(buttonArea);
        }
    }

    void layoutProfileControls(juce::Rectangle<int> area)
    {
        for (auto index = 0; index < static_cast<int>(controlSliders.size()); ++index)
        {
            auto row = area.removeFromTop(32);
            controlLabels[static_cast<size_t>(index)].setBounds(row.removeFromLeft(82));
            row.removeFromLeft(8);
            controlSliders[static_cast<size_t>(index)].setBounds(row);
            area.removeFromTop(9);
        }
    }

    void configureNavigationIcons()
    {
        juce::Path settingsPath;
        settingsPath.addRoundedRectangle(6.0f, 8.0f, 28.0f, 4.0f, 2.0f);
        settingsPath.addRoundedRectangle(6.0f, 18.0f, 28.0f, 4.0f, 2.0f);
        settingsPath.addRoundedRectangle(6.0f, 28.0f, 28.0f, 4.0f, 2.0f);
        settingsPath.addEllipse(12.0f, 5.0f, 10.0f, 10.0f);
        settingsPath.addEllipse(22.0f, 15.0f, 10.0f, 10.0f);
        settingsPath.addEllipse(10.0f, 25.0f, 10.0f, 10.0f);

        settingsIcon.setPath(settingsPath);
        settingsIcon.setFill(juce::Colours::white);

        juce::Path backPath;
        backPath.startNewSubPath(28.0f, 8.0f);
        backPath.lineTo(12.0f, 20.0f);
        backPath.lineTo(28.0f, 32.0f);
        backPath.lineTo(28.0f, 24.0f);
        backPath.lineTo(36.0f, 24.0f);
        backPath.lineTo(36.0f, 16.0f);
        backPath.lineTo(28.0f, 16.0f);
        backPath.closeSubPath();

        backIcon.setPath(backPath);
        backIcon.setFill(juce::Colours::white);

        navigationButton.setColour(
            juce::DrawableButton::backgroundColourId,
            juce::Colours::transparentBlack
        );

        navigationButton.setColour(
            juce::DrawableButton::backgroundOnColourId,
            juce::Colours::transparentBlack
        );
    }

    void updateScreenVisibility()
    {
        navigationButton.setImages(showingSettings ? &backIcon : &settingsIcon);

        voiceButton.setVisible(!showingSettings);
        bypassButton.setVisible(!showingSettings);
        profileLabel.setVisible(!showingSettings);
        inputGainSlider.setVisible(!showingSettings);
        inputGainLabel.setVisible(!showingSettings);
        gainSlider.setVisible(!showingSettings);
        gainLabel.setVisible(!showingSettings);
        inputLevelLabel.setVisible(!showingSettings);
        inputLevelMeter.setVisible(!showingSettings);
        outputLevelLabel.setVisible(!showingSettings);
        outputLevelMeter.setVisible(!showingSettings);

        for (const auto& button : profileButtons)
            button->setVisible(!showingSettings);

        updateProfilePanel();

        if (deviceSelector != nullptr)
            deviceSelector->setVisible(showingSettings);
    }

private:
    bool showingSettings = false;
    bool profileDrawerOpen = false;
    float inputGain = 1.0f;
    float outputGain = 1.0f;

    AudioEngine audioEngine;
    juce::ApplicationProperties applicationProperties;
    VoiceProfileId selectedProfileId = VoiceProfileId::echo;
    std::vector<std::vector<float>> profileControlValues;

    juce::DrawableButton navigationButton {
        "navigation",
        juce::DrawableButton::ImageFitted
    };

    juce::DrawablePath settingsIcon;
    juce::DrawablePath backIcon;

    juce::Viewport contentViewport;
    juce::Component contentComponent;

    juce::TextButton voiceButton;
    juce::Slider inputGainSlider;
    juce::Label inputGainLabel;
    juce::Slider gainSlider;
    juce::Label gainLabel;
    juce::Label inputLevelLabel;
    juce::Label outputLevelLabel;
    double inputLevelValue = 0.0;
    double outputLevelValue = 0.0;
    juce::ProgressBar inputLevelMeter { inputLevelValue };
    juce::ProgressBar outputLevelMeter { outputLevelValue };

    juce::TextButton bypassButton;
    juce::Label profileLabel;
    ProfileDrawerBackground profileDrawerBackground;
    juce::Label profileDrawerTitle;
    juce::Label profileDrawerDescription;
    juce::TextButton closeProfileDrawerButton;
    juce::TextButton resetProfileButton;
    juce::Rectangle<int> profileImagePlaceholder;
    std::vector<std::unique_ptr<ProfileCardButton>> profileButtons;
    std::array<juce::Slider, 5> controlSliders;
    std::array<juce::Label, 5> controlLabels;

    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow(juce::String name)
        : DocumentWindow(
            name,
            juce::Colours::black,
            DocumentWindow::allButtons
        )
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class AlterechoApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "alterecho";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(AlterechoApplication)
