#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>

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
            outputGain.store(
                static_cast<float>(gainSlider.getValue()),
                std::memory_order_relaxed
            );
        };

        voiceButton.setButtonText("voice off");
        voiceButton.setClickingTogglesState(true);
        voiceButton.setToggleState(false, juce::dontSendNotification);
        voiceButton.onClick = [this]
        {
            auto isEnabled = voiceButton.getToggleState();

            voiceEnabled.store(isEnabled, std::memory_order_relaxed);
            voiceButton.setButtonText(isEnabled ? "voice on" : "voice off");
        };

        gainLabel.setText("volume", juce::dontSendNotification);
        gainLabel.attachToComponent(&gainSlider, true);

        addAndMakeVisible(voiceButton);
        addAndMakeVisible(gainSlider);
        addAndMakeVisible(gainLabel);

        updateScreenVisibility();
        resized();
    }

    ~MainComponent() override
    {
        shutdownAudio();
    }

    void prepareToPlay(int, double) override
    {
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* device = deviceManager.getCurrentAudioDevice();

        if (device == nullptr)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        if (!voiceEnabled.load(std::memory_order_relaxed))
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        auto activeInputChannels = device->getActiveInputChannels();
        auto activeOutputChannels = device->getActiveOutputChannels();

        auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
        auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;

        for (int channel = 0; channel < maxOutputChannels; ++channel)
        {
            if (!activeOutputChannels[channel])
                continue;

            auto* outputData = bufferToFill.buffer->getWritePointer(
                channel,
                bufferToFill.startSample
            );

            if (channel < maxInputChannels && activeInputChannels[channel])
            {
                auto* inputData = bufferToFill.buffer->getReadPointer(
                    channel,
                    bufferToFill.startSample
                );

                auto currentGain = outputGain.load(std::memory_order_relaxed);

                for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
                {
                    auto boostedSample = inputData[sample] * currentGain;

                    outputData[sample] = juce::jlimit(
                        -1.0f,
                        1.0f,
                        boostedSample
                    );
                }
            }
            else
            {
                juce::FloatVectorOperations::clear(
                    outputData,
                    bufferToFill.numSamples
                );
            }
        }
    }

    void releaseResources() override
    {
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

        if (showingSettings)
        {
            if (deviceSelector != nullptr)
                deviceSelector->setBounds(bounds);

            return;
        }

        auto controlsArea = bounds.withSizeKeepingCentre(
            juce::jmin(700, bounds.getWidth()),
            128
        );

        auto voiceArea = controlsArea.removeFromTop(44);
        voiceButton.setBounds(voiceArea.withSizeKeepingCentre(160, 40));

        controlsArea.removeFromTop(32);

        auto gainArea = controlsArea.removeFromTop(40);
        gainArea.removeFromLeft(120);
        gainSlider.setBounds(gainArea);
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
        gainSlider.setVisible(!showingSettings);
        gainLabel.setVisible(!showingSettings);

        if (deviceSelector != nullptr)
            deviceSelector->setVisible(showingSettings);
    }

private:
    bool showingSettings = false;

    std::atomic<float> outputGain { 2.0f };
    std::atomic<bool> voiceEnabled { false };

    juce::DrawableButton navigationButton {
        "navigation",
        juce::DrawableButton::ImageFitted
    };

    juce::DrawablePath settingsIcon;
    juce::DrawablePath backIcon;

    juce::TextButton voiceButton;
    juce::Slider gainSlider;
    juce::Label gainLabel;

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