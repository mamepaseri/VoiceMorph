#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterMapping.h"

namespace
{
    constexpr float minSafeFrequency = 20.0f;

    std::pair<float, float> getDetectorRange (int index, double sampleRate)
    {
        const float nyquistSafe = static_cast<float> (sampleRate * 0.45);
        switch (index)
        {
            case 0: return { 1000.0f, juce::jmin (4000.0f, nyquistSafe) };
            case 1: return { 2000.0f, juce::jmin (4000.0f, nyquistSafe) };
            case 2: return { 5000.0f, juce::jmin (10000.0f, nyquistSafe) };
            default: return { 5000.0f, juce::jmin (20000.0f, nyquistSafe) };
        }
    }
}

VoiceMorphAudioProcessor::VoiceMorphAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOICEMORPH_STATE", createParameterLayout())
{
    for (auto& sample : inputWaveform)  sample.store (0.0f);
    for (auto& sample : outputWaveform) sample.store (0.0f);
    for (auto& sample : inputPitchContour) sample.store (0.0f);
    for (auto& sample : targetPitchContour) sample.store (0.0f);
    slotA = apvts.copyState();
    slotB = slotA.createCopy();
    updateDetectorBand (0);
}

VoiceMorphAudioProcessor::APVTS::ParameterLayout VoiceMorphAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    using Float = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool = juce::AudioParameterBool;

    layout.add (std::make_unique<Float> (juce::ParameterID { "inputGain", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f));

    // Core voice-transform controls. Pitch and Formant feed the optional Signalsmith
    // backend through the voiced-periodic hybrid path. Tracking controls streaming
    // F0 / voicing response speed; preservation controls feed first-pass split gates.
    layout.add (std::make_unique<Float> (juce::ParameterID { "voiceMix", 1 }, "Voice Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "pitchSemitones", 1 }, "Pitch",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "formantSemitones", 1 }, "Formant",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "pitchDynamics", 1 }, "Pitch Dynamics",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 75.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "tracking", 1 }, "Tracking",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "breathPreserve", 1 }, "Breath Preserve",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 65.0f));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "voiceMode", 1 }, "Voice Mode",
        juce::StringArray { "Natural", "Balanced", "Precise" }, 0));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "qualityMode", 1 }, "Quality Mode",
        juce::StringArray { "Live", "Balanced", "Studio" }, 0));
    layout.add (std::make_unique<Bool> (juce::ParameterID { "consonantPreserve", 1 }, "Consonant Preserve", true));
    layout.add (std::make_unique<Bool> (juce::ParameterID { "transientPreserve", 1 }, "Transient Preserve", true));
    layout.add (std::make_unique<Bool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // De-esser controls adapted from the documented parameter concepts embedded in
    // the supplied T-De-Esser 2 VST3 metadata (not its code or assets).
    layout.add (std::make_unique<Choice> (juce::ParameterID { "deEssRange", 1 }, "De-Esser Frequency Range",
        juce::StringArray { "Low-Hi 1-4 kHz", "Mid-Hi 2-4 kHz", "High 5-10 kHz", "Hi-End 5-20 kHz" }, 2));
    layout.add (std::make_unique<Float> (juce::ParameterID { "deEssIntensity", 1 }, "De-Esser Intensity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "deEssMix", 1 }, "De-Esser Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "deEssAttack", 1 }, "De-Esser Attack",
        juce::NormalisableRange<float> (0.5f, 20.0f, 0.1f), 3.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "deEssRelease", 1 }, "De-Esser Release",
        juce::NormalisableRange<float> (5.0f, 250.0f, 1.0f), 90.0f));
    layout.add (std::make_unique<Bool> (juce::ParameterID { "deEssBypass", 1 }, "De-Esser Bypass", false));

    return layout;
}

float VoiceMorphAudioProcessor::readParam (const APVTS& state, const char* id, float fallback) noexcept
{
    if (auto* value = state.getRawParameterValue (id))
        return value->load (std::memory_order_relaxed);
    return fallback;
}

void VoiceMorphAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jlimit (8000.0, 192000.0, sampleRate);
    activeDetectorRange = -1;
    updateDetectorBand (static_cast<int> (readParam (apvts, "deEssRange", 2.0f)));
    hpPreviousInput.fill (0.0f);
    hpPreviousOutput.fill (0.0f);
    lpPreviousOutput.fill (0.0f);
    detectorEnvelope.fill (0.0f);
    speechAnalyzer.prepare (currentSampleRate);
    pitchContourMapper.prepare (currentSampleRate);
    currentPitchDynamicsOffset = 0.0f;
    periodicSplitter.prepare (currentSampleRate);

    hybridMaximumBlockSize = juce::jmax (1, samplesPerBlock);
    const auto channelCount = juce::jlimit (1, 2, getTotalNumOutputChannels());
    const bool hybridPrepared = hybridVoiceEngine.prepare (channelCount, currentSampleRate,
                                                            hybridMaximumBlockSize);
    voiceEngineAvailable.store (hybridPrepared && hybridVoiceEngine.isTransformAvailable(),
                                std::memory_order_relaxed);
    setLatencySamples (hybridPrepared ? hybridVoiceEngine.getLatencySamples() : 0);

    const auto scratchSize = static_cast<std::size_t> (hybridMaximumBlockSize);
    framePitch.assign (scratchSize, 0.0f);
    frameVoiced.assign (scratchSize, 0.0f);
    frameAperiodic.assign (scratchSize, 0.0f);
    frameInputMono.assign (scratchSize, 0.0f);

    periodicEnergyAccumulator = 0.0f;
    residualEnergyAccumulator = 0.0f;
    splitSamplesInFrame = 0;
    publishedPeriodicRms.store (0.0f, std::memory_order_relaxed);
    publishedResidualRms.store (0.0f, std::memory_order_relaxed);
    for (auto& pitch : inputPitchContour) pitch.store (0.0f, std::memory_order_relaxed);
    for (auto& pitch : targetPitchContour) pitch.store (0.0f, std::memory_order_relaxed);
    pitchContourWriteIndex.store (0, std::memory_order_relaxed);
    publishSpeechFeatures();
}

void VoiceMorphAudioProcessor::releaseResources()
{
    hybridVoiceEngine.reset();
    voiceEngineAvailable.store (false, std::memory_order_relaxed);
    setLatencySamples (0);
}

bool VoiceMorphAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void VoiceMorphAudioProcessor::updateDetectorBand (int rangeIndex) noexcept
{
    if (rangeIndex == activeDetectorRange) return;
    activeDetectorRange = rangeIndex;
    const auto range = getDetectorRange (rangeIndex, currentSampleRate);
    const float nyquistSafe = static_cast<float> (currentSampleRate * 0.45);
    const float lowHz = juce::jlimit (minSafeFrequency, nyquistSafe - 50.0f, range.first);
    const float highHz = juce::jlimit (lowHz + 50.0f, nyquistSafe, range.second);
    hpCoefficient = std::exp (-2.0f * juce::MathConstants<float>::pi * lowHz / static_cast<float> (currentSampleRate));
    lpCoefficient = std::exp (-2.0f * juce::MathConstants<float>::pi * highHz / static_cast<float> (currentSampleRate));
}

void VoiceMorphAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    if (channels <= 0 || numSamples <= 0)
        return;

    const float inputGain = juce::Decibels::decibelsToGain (readParam (apvts, "inputGain", 0.0f));
    const float outputGain = juce::Decibels::decibelsToGain (readParam (apvts, "outputGain", 0.0f));
    const float pitchSemitones = readParam (apvts, "pitchSemitones", 0.0f);
    const float pitchDynamics = readParam (apvts, "pitchDynamics", 75.0f);
    const float formantSemitones = readParam (apvts, "formantSemitones", 0.0f);
    const float voiceMix = readParam (apvts, "voiceMix", 100.0f);
    const float breathPreserve = readParam (apvts, "breathPreserve", 65.0f);
    const float tracking = readParam (apvts, "tracking", 70.0f);
    const bool consonantPreserve = readParam (apvts, "consonantPreserve", 1.0f) > 0.5f;
    const bool transientPreserve = readParam (apvts, "transientPreserve", 1.0f) > 0.5f;
    const float intensity = readParam (apvts, "deEssIntensity", 25.0f) * 0.01f;
    const float wet = readParam (apvts, "deEssMix", 100.0f) * 0.01f;
    const float attackMs = readParam (apvts, "deEssAttack", 3.0f);
    const float releaseMs = readParam (apvts, "deEssRelease", 90.0f);
    const bool deEssBypass = readParam (apvts, "deEssBypass", 0.0f) > 0.5f;
    const bool bypass = readParam (apvts, "bypass", 0.0f) > 0.5f;
    const int rangeIndex = static_cast<int> (readParam (apvts, "deEssRange", 2.0f));
    updateDetectorBand (rangeIndex);
    speechAnalyzer.setTracking (tracking);
    pitchContourMapper.setDynamicsPercent (pitchDynamics);

    const float attackCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * juce::jmax (0.0005f, attackMs * 0.001f)));
    const float releaseCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * juce::jmax (0.001f, releaseMs * 0.001f)));
    const float thresholdDb = -38.0f + (1.0f - intensity) * 18.0f;
    const float maxReductionDb = 12.0f * intensity;

    const int controlHopSamples = juce::jmax (1, static_cast<int> (std::lround (currentSampleRate * 0.010)));

    // Hosts occasionally provide a block larger than prepareToPlay() advertised.
    // Chunking at 10 ms also keeps pitch-contour control updates time-local.
    for (int offset = 0; offset < numSamples; )
    {
        const int count = juce::jmin (juce::jmin (hybridMaximumBlockSize, controlHopSamples),
                                      numSamples - offset);

        for (int sample = 0; sample < count; ++sample)
        {
            float rawMono = 0.0f;
            float analysisMono = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float raw = buffer.getSample (ch, offset + sample);
                rawMono += raw;
                const float preEngine = bypass ? raw : raw * inputGain;
                buffer.setSample (ch, offset + sample, preEngine);
                analysisMono += preEngine;
            }

            const float monoInput = analysisMono / static_cast<float> (channels);
            frameInputMono[static_cast<std::size_t> (sample)] = rawMono / static_cast<float> (channels);
            const bool newFeatureFrame = speechAnalyzer.processSample (monoInput);
            const auto& speech = speechAnalyzer.getFeatures();
            framePitch[static_cast<std::size_t> (sample)] = speech.pitchHz;
            frameVoiced[static_cast<std::size_t> (sample)] = speech.voiced;
            frameAperiodic[static_cast<std::size_t> (sample)] = speech.aperiodic;

            // Keep the UI's split meters backed by the same live analyzer.
            const float aperiodicProtection = consonantPreserve
                ? breathPreserve * 0.01f * speech.aperiodic : 0.0f;
            const auto split = periodicSplitter.processSample (monoInput, speech.pitchHz, speech.voiced,
                                                               transientPreserve, aperiodicProtection);
            periodicEnergyAccumulator += split.periodic * split.periodic;
            residualEnergyAccumulator += split.residual * split.residual;
            ++splitSamplesInFrame;

            if (newFeatureFrame)
            {
                currentPitchDynamicsOffset = pitchContourMapper.processFrame (speech.pitchHz, speech.voiced);
                const float effectivePitchSemitones = pitchSemitones + currentPitchDynamicsOffset;
                const bool hasStablePitch = speech.pitchHz >= 55.0f && speech.pitchHz <= 880.0f
                                         && speech.voiced >= 0.25f;
                const float inputF0 = hasStablePitch ? speech.pitchHz : 0.0f;
                const float targetF0 = hasStablePitch
                    ? static_cast<float> (voice_morph::parameter_mapping::shiftedFrequencyFromSemitones (
                          speech.pitchHz, effectivePitchSemitones))
                    : 0.0f;
                const size_t contourWrite = pitchContourWriteIndex.load (std::memory_order_relaxed);
                inputPitchContour[contourWrite].store (inputF0, std::memory_order_relaxed);
                targetPitchContour[contourWrite].store (targetF0, std::memory_order_relaxed);
                pitchContourWriteIndex.store ((contourWrite + 1) % waveformSize, std::memory_order_relaxed);

                publishSpeechFeatures();
                if (splitSamplesInFrame > 0)
                {
                    const float inverseCount = 1.0f / static_cast<float> (splitSamplesInFrame);
                    publishedPeriodicRms.store (std::sqrt (periodicEnergyAccumulator * inverseCount), std::memory_order_relaxed);
                    publishedResidualRms.store (std::sqrt (residualEnergyAccumulator * inverseCount), std::memory_order_relaxed);
                }
                periodicEnergyAccumulator = 0.0f;
                residualEnergyAccumulator = 0.0f;
                splitSamplesInFrame = 0;
            }
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            hybridInputPointers[static_cast<std::size_t> (ch)] = buffer.getReadPointer (ch, offset);
            hybridOutputPointers[static_cast<std::size_t> (ch)] = buffer.getWritePointer (ch, offset);
        }

        // Formant base follows the smoothed live F0 estimate, with a stable neutral
        // value during silence/unvoiced regions. The splitter itself still gates
        // those regions out of the transformed periodic stem.
        const float trackedPitch = speechAnalyzer.getFeatures().pitchHz;
        const float formantBase = trackedPitch >= 65.0f && trackedPitch <= 450.0f ? trackedPitch : 140.0f;
        hybridVoiceEngine.setParameters (pitchSemitones + currentPitchDynamicsOffset,
                                         formantSemitones, formantBase);
        const bool hybridProcessed = hybridVoiceEngine.processBlock (
            hybridInputPointers.data(), hybridOutputPointers.data(), channels, count,
            framePitch.data(), frameVoiced.data(), frameAperiodic.data(), voiceMix, bypass,
            breathPreserve, consonantPreserve, transientPreserve);

        // If an unexpected engine/host failure occurs, the pre-engine (gain-staged)
        // input is still in the buffer. It is safer to preserve that than emit a
        // partially recombined residual stem.
        (void) hybridProcessed;

        for (int sample = 0; sample < count; ++sample)
        {
            float outputMono = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
            {
                float processed = buffer.getSample (ch, offset + sample);
                if (! bypass)
                {
                    if (! deEssBypass && intensity > 0.0001f)
                    {
                        const auto channelIndex = static_cast<std::size_t> (ch);
                        const float hp = hpCoefficient * (hpPreviousOutput[channelIndex]
                            + processed - hpPreviousInput[channelIndex]);
                        hpPreviousInput[channelIndex] = processed;
                        hpPreviousOutput[channelIndex] = hp;
                        const float band = (1.0f - lpCoefficient) * hp
                            + lpCoefficient * lpPreviousOutput[channelIndex];
                        lpPreviousOutput[channelIndex] = band;

                        const float detectorLevel = std::abs (band);
                        float& envelope = detectorEnvelope[channelIndex];
                        const float coeff = detectorLevel > envelope ? attackCoeff : releaseCoeff;
                        envelope = coeff * envelope + (1.0f - coeff) * detectorLevel;
                        const float envelopeDb = juce::Decibels::gainToDecibels (envelope, -120.0f);
                        const float overThreshold = juce::jmax (0.0f, envelopeDb - thresholdDb);
                        const float reductionDb = juce::jlimit (0.0f, maxReductionDb, overThreshold * 0.70f * intensity);
                        const float reductionGain = juce::Decibels::decibelsToGain (-reductionDb);
                        processed *= (1.0f + (reductionGain - 1.0f) * wet);
                    }
                    processed *= outputGain;
                    buffer.setSample (ch, offset + sample, processed);
                }
                outputMono += processed;
            }

            if ((sample % 4) == 0)
            {
                const size_t write = waveformWriteIndex.load (std::memory_order_relaxed);
                inputWaveform[write].store (frameInputMono[static_cast<std::size_t> (sample)], std::memory_order_relaxed);
                outputWaveform[write].store (outputMono / static_cast<float> (channels), std::memory_order_relaxed);
                waveformWriteIndex.store ((write + 1) % waveformSize, std::memory_order_relaxed);
            }
        }

        offset += count;
    }
}

void VoiceMorphAudioProcessor::publishSpeechFeatures() noexcept
{
    const auto& features = speechAnalyzer.getFeatures();
    publishedRms.store (features.rms, std::memory_order_relaxed);
    publishedPitchHz.store (features.pitchHz, std::memory_order_relaxed);
    publishedPeriodicity.store (features.periodicity, std::memory_order_relaxed);
    publishedVoiced.store (features.voiced, std::memory_order_relaxed);
    publishedAperiodic.store (features.aperiodic, std::memory_order_relaxed);
    publishedHighBandRatio.store (features.highBandRatio, std::memory_order_relaxed);
    publishedTransient.store (features.transient, std::memory_order_relaxed);
}

void VoiceMorphAudioProcessor::getSpeechFeatures (SpeechFeatures& destination) const noexcept
{
    destination.rms = publishedRms.load (std::memory_order_relaxed);
    destination.pitchHz = publishedPitchHz.load (std::memory_order_relaxed);
    destination.periodicity = publishedPeriodicity.load (std::memory_order_relaxed);
    destination.voiced = publishedVoiced.load (std::memory_order_relaxed);
    destination.aperiodic = publishedAperiodic.load (std::memory_order_relaxed);
    destination.highBandRatio = publishedHighBandRatio.load (std::memory_order_relaxed);
    destination.transient = publishedTransient.load (std::memory_order_relaxed);
}

void VoiceMorphAudioProcessor::getSplitLevels (float& periodicRms, float& residualRms) const noexcept
{
    periodicRms = publishedPeriodicRms.load (std::memory_order_relaxed);
    residualRms = publishedResidualRms.load (std::memory_order_relaxed);
}

void VoiceMorphAudioProcessor::copyPitchContours (PitchContour& inputHz, PitchContour& targetHz) const noexcept
{
    const size_t start = pitchContourWriteIndex.load (std::memory_order_relaxed);
    for (size_t i = 0; i < waveformSize; ++i)
    {
        const size_t index = (start + i) % waveformSize;
        inputHz[i] = inputPitchContour[index].load (std::memory_order_relaxed);
        targetHz[i] = targetPitchContour[index].load (std::memory_order_relaxed);
    }
}

void VoiceMorphAudioProcessor::copyWaveforms (Waveform& input, Waveform& output) const noexcept
{
    const size_t start = waveformWriteIndex.load (std::memory_order_relaxed);
    for (size_t i = 0; i < waveformSize; ++i)
    {
        const size_t index = (start + i) % waveformSize;
        input[i] = inputWaveform[index].load (std::memory_order_relaxed);
        output[i] = outputWaveform[index].load (std::memory_order_relaxed);
    }
}

void VoiceMorphAudioProcessor::switchAB (bool toB)
{
    if (toB == activeBSlot) return;
    const auto current = apvts.copyState();
    if (activeBSlot) slotB = current.createCopy(); else slotA = current.createCopy();
    apvts.replaceState ((toB ? slotB : slotA).createCopy());
    activeBSlot = toB;
}

void VoiceMorphAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void VoiceMorphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        slotA = apvts.copyState();
        slotB = slotA.createCopy();
        activeBSlot = false;
    }
}

juce::AudioProcessorEditor* VoiceMorphAudioProcessor::createEditor()
{
    return new VoiceMorphAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoiceMorphAudioProcessor();
}

