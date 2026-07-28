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

        gainLabel.setText("volume", juce::dontSendNotification);
        gainLabel.attachToComponent(&gainSlider, true);

        addAndMakeVisible(gainSlider);
        addAndMakeVisible(gainLabel);

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
        bounds.removeFromTop(56);

        auto gainArea = bounds.removeFromTop(40);
        gainArea.removeFromLeft(120);
        gainSlider.setBounds(gainArea);

        bounds.removeFromTop(24);

        if (deviceSelector != nullptr)
            deviceSelector->setBounds(bounds);
    }

private:
    std::atomic<float> outputGain { 2.0f };

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