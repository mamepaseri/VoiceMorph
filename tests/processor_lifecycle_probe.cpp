#include "PluginProcessor.h"

#include <cmath>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VoiceMorphAudioProcessor processor;
    std::cout << "PASS: processor constructed\n" << std::flush;

    processor.prepareToPlay (48000.0, 2048);
#if VOICEMORPH_HAS_SIGNALSMITH
    if (! processor.isVoiceEngineConnected())
    {
        std::cerr << "FAIL: Signalsmith engine unavailable after prepareToPlay\n";
        return 1;
    }
#endif
    std::cout << "PASS: processor prepared at 48 kHz / 2048 samples\n" << std::flush;

    juce::AudioBuffer<float> buffer (2, 2048);
    juce::MidiBuffer midi;
    for (int block = 0; block < 64; ++block)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const double time = static_cast<double> (block * buffer.getNumSamples() + sample) / 48000.0;
            const float value = static_cast<float> (0.2 * std::sin (2.0 * 3.14159265358979323846 * 132.0 * time));
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, value * 0.9f);
        }
        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                if (! std::isfinite (buffer.getSample (channel, sample)))
                {
                    std::cerr << "FAIL: non-finite output during processBlock\n";
                    return 1;
                }
    }
    std::cout << "PASS: 64 stereo audio blocks processed\n" << std::flush;

    const int reportedLatency = processor.getLatencySamples();
    for (int cycle = 0; cycle < 4; ++cycle)
    {
        processor.releaseResources();
        if (processor.getLatencySamples() != reportedLatency)
        {
            std::cerr << "FAIL: reported latency changed when resources were released\n";
            return 1;
        }
        processor.prepareToPlay (48000.0, 2048);
        const bool engineAvailabilityMatchesBuild =
#if VOICEMORPH_HAS_SIGNALSMITH
            processor.isVoiceEngineConnected();
#else
            ! processor.isVoiceEngineConnected();
#endif
        if (! engineAvailabilityMatchesBuild || processor.getLatencySamples() != reportedLatency)
        {
            std::cerr << "FAIL: engine or reported latency changed during prepare/release cycle\n";
            return 1;
        }
    }
    std::cout << "PASS: host latency stayed at " << reportedLatency
              << " samples across four prepare/release cycles\n" << std::flush;

    std::cout << "START: createEditor\n" << std::flush;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    std::cout << "PASS: editor constructed\n" << std::flush;
    if (editor == nullptr)
    {
        std::cerr << "FAIL: createEditor returned null\n";
        return 1;
    }
    editor->setSize (1220, 810);
    std::cout << "PASS: editor resized\n" << std::flush;
    juce::Thread::sleep (80);
    std::cout << "START: pending timers\n" << std::flush;
    juce::Timer::callPendingTimersSynchronously();
    std::cout << "PASS: timer ticked\n" << std::flush;

    juce::Button* advancedButton = nullptr;
    for (int index = 0; index < editor->getNumChildComponents(); ++index)
        if (auto* button = dynamic_cast<juce::Button*> (editor->getChildComponent (index)))
            if (button->getButtonText() == "ADVANCED")
                advancedButton = button;
    if (advancedButton == nullptr)
    {
        std::cerr << "FAIL: advanced settings button was not created\n";
        return 1;
    }
    advancedButton->triggerClick();
    std::cout << "PASS: advanced settings window opened\n" << std::flush;

    editor.reset();
    processor.releaseResources();
    std::cout << "PASS: editor and processor released\n" << std::flush;
    return 0;
}
