#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "audio_engine.h"

class MainComponent final : public juce::AudioAppComponent
{
public:
    MainComponent()
    {
        setSize(900, 680);

        setAudioChannels(1, 2);

        configureNavigationIcons();

        navigationButton.onClick = [this]
        {
            showingSettings = !showingSettings;
            updateScreenVisibility();
            resized();
            repaint();
        };

        addAndMakeVisible(navigationButton);

        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManager,
            1, 2,
            1, 2,
            true,
            false,
            true,
            false
        );

        addAndMakeVisible(*deviceSelector);

        gainSlider.setRange(0.0, 4.0, 0.01);
        gainSlider.setValue(2.0);
        gainSlider.setTextValueSuffix("x");

        gainSlider.onValueChange = [this]
        {
            audioEngine.setOutputGain(static_cast<float>(gainSlider.getValue()));
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

        bypassButton.setButtonText("bypass off");
        bypassButton.setClickingTogglesState(true);
        bypassButton.setToggleState(false, juce::dontSendNotification);
        bypassButton.onClick = [this]
        {
            auto isEnabled = bypassButton.getToggleState();

            audioEngine.setEffectsBypassed(isEnabled);
            bypassButton.setButtonText(isEnabled ? "bypass on" : "bypass off");
        };

        echoButton.setButtonText("echo off");
        echoButton.setClickingTogglesState(true);
        echoButton.setToggleState(false, juce::dontSendNotification);
        echoButton.onClick = [this]
        {
            auto isEnabled = echoButton.getToggleState();

            audioEngine.setEchoEnabled(isEnabled);
            echoButton.setButtonText(isEnabled ? "echo on" : "echo off");
        };

        delaySlider.setRange(1.0, 2000.0, 1.0);
        delaySlider.setValue(350.0);
        delaySlider.setTextValueSuffix(" ms");
        delaySlider.onValueChange = [this]
        {
            audioEngine.setEchoDelayMs(static_cast<float>(delaySlider.getValue()));
        };

        feedbackSlider.setRange(0.0, 0.95, 0.01);
        feedbackSlider.setValue(0.35);
        feedbackSlider.onValueChange = [this]
        {
            audioEngine.setEchoFeedback(static_cast<float>(feedbackSlider.getValue()));
        };

        mixSlider.setRange(0.0, 1.0, 0.01);
        mixSlider.setValue(0.35);
        mixSlider.onValueChange = [this]
        {
            audioEngine.setEchoMix(static_cast<float>(mixSlider.getValue()));
        };

        gainLabel.setText("volume", juce::dontSendNotification);
        gainLabel.attachToComponent(&gainSlider, true);

        delayLabel.setText("delay", juce::dontSendNotification);
        delayLabel.attachToComponent(&delaySlider, true);

        feedbackLabel.setText("feedback", juce::dontSendNotification);
        feedbackLabel.attachToComponent(&feedbackSlider, true);

        mixLabel.setText("mix", juce::dontSendNotification);
        mixLabel.attachToComponent(&mixSlider, true);

        addAndMakeVisible(voiceButton);
        addAndMakeVisible(bypassButton);
        addAndMakeVisible(gainSlider);
        addAndMakeVisible(gainLabel);

        addAndMakeVisible(echoButton);
        addAndMakeVisible(delaySlider);
        addAndMakeVisible(delayLabel);
        addAndMakeVisible(feedbackSlider);
        addAndMakeVisible(feedbackLabel);
        addAndMakeVisible(mixSlider);
        addAndMakeVisible(mixLabel);

        updateScreenVisibility();
        resized();
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

        if (showingSettings)
        {
            if (deviceSelector != nullptr)
                deviceSelector->setBounds(bounds);

            return;
        }

        auto controlsArea = bounds.withSizeKeepingCentre(
            juce::jmin(700, bounds.getWidth()),
            380
        );

        auto voiceArea = controlsArea.removeFromTop(44);
        voiceButton.setBounds(voiceArea.withSizeKeepingCentre(160, 40));

        controlsArea.removeFromTop(20);

        auto bypassArea = controlsArea.removeFromTop(44);
        bypassButton.setBounds(bypassArea.withSizeKeepingCentre(160, 40));

        controlsArea.removeFromTop(32);

        auto gainArea = controlsArea.removeFromTop(40);
        gainArea.removeFromLeft(120);
        gainSlider.setBounds(gainArea);

        controlsArea.removeFromTop(28);

        auto echoArea = controlsArea.removeFromTop(44);
        echoButton.setBounds(echoArea.withSizeKeepingCentre(160, 40));

        controlsArea.removeFromTop(28);

        auto delayArea = controlsArea.removeFromTop(40);
        delayArea.removeFromLeft(120);
        delaySlider.setBounds(delayArea);

        controlsArea.removeFromTop(20);

        auto feedbackArea = controlsArea.removeFromTop(40);
        feedbackArea.removeFromLeft(120);
        feedbackSlider.setBounds(feedbackArea);

        controlsArea.removeFromTop(20);

        auto mixArea = controlsArea.removeFromTop(40);
        mixArea.removeFromLeft(120);
        mixSlider.setBounds(mixArea);
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
        gainSlider.setVisible(!showingSettings);
        gainLabel.setVisible(!showingSettings);

        echoButton.setVisible(!showingSettings);
        delaySlider.setVisible(!showingSettings);
        delayLabel.setVisible(!showingSettings);
        feedbackSlider.setVisible(!showingSettings);
        feedbackLabel.setVisible(!showingSettings);
        mixSlider.setVisible(!showingSettings);
        mixLabel.setVisible(!showingSettings);

        if (deviceSelector != nullptr)
            deviceSelector->setVisible(showingSettings);
    }

private:
    bool showingSettings = false;

    AudioEngine audioEngine;

    juce::DrawableButton navigationButton {
        "navigation",
        juce::DrawableButton::ImageFitted
    };

    juce::DrawablePath settingsIcon;
    juce::DrawablePath backIcon;

    juce::TextButton voiceButton;
    juce::Slider gainSlider;
    juce::Label gainLabel;

    juce::TextButton echoButton;
    juce::TextButton bypassButton;
    juce::Slider delaySlider;
    juce::Label delayLabel;
    juce::Slider feedbackSlider;
    juce::Label feedbackLabel;
    juce::Slider mixSlider;
    juce::Label mixLabel;

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