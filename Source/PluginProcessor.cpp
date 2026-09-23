#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float minusInfinityDb = -90.0f;
constexpr int pitchFrameSize = 2048;
constexpr int pitchHopSamples = 1024;
constexpr float defaultBpm = 150.0f;
constexpr int defaultBeatsPerBar = 4;

const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

HoneyBadgerKickCartographerAudioProcessor::HoneyBadgerKickCartographerAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    resetCurrentSnapshot(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout HoneyBadgerKickCartographerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sensitivity", "Transient Sensitivity",
        juce::NormalisableRange<float>(0.25f, 2.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("minConfidence", "Minimum Pitch Confidence",
        juce::NormalisableRange<float>(0.10f, 0.90f, 0.01f), 0.34f));
    return { params.begin(), params.end() };
}

void HoneyBadgerKickCartographerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    ring.assign(static_cast<size_t>(pitchFrameSize), 0.0f);
    ringWrite = 0;
    samplesUntilPitch = pitchHopSamples;
    fallbackSamplesIntoBar = 0;
    currentHostBar = 0;
    haveHostBar = false;
    history = {};
    historyWriteIndex.store(0);
    liveHz.store(0.0f);
    liveConfidence.store(0.0f);
    livePeakDb.store(minusInfinityDb);
    liveRmsDb.store(minusInfinityDb);
    resetCurrentSnapshot(0);
}

bool HoneyBadgerKickCartographerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainIn == mainOut
        && (mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo());
}

float HoneyBadgerKickCartographerAudioProcessor::linearToDb(float value) noexcept
{
    return value > 0.000001f ? juce::Decibels::gainToDecibels(value) : minusInfinityDb;
}

float HoneyBadgerKickCartographerAudioProcessor::hzToMidi(float hz) noexcept
{
    return hz > 0.0f ? 69.0f + 12.0f * std::log2(hz / 440.0f) : 0.0f;
}

float HoneyBadgerKickCartographerAudioProcessor::midiToHz(float midi) noexcept
{
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

int HoneyBadgerKickCartographerAudioProcessor::pitchClassForHz(float hz) noexcept
{
    if (hz <= 0.0f)
        return -1;

    auto midi = static_cast<int>(std::round(hzToMidi(hz)));
    auto pc = midi % 12;
    return pc < 0 ? pc + 12 : pc;
}

juce::String HoneyBadgerKickCartographerAudioProcessor::hzToNoteName(float hz)
{
    if (hz <= 0.0f)
        return "--";

    const auto midi = static_cast<int>(std::round(hzToMidi(hz)));
    const auto pc = (midi % 12 + 12) % 12;
    const auto cents = static_cast<int>(std::round((hzToMidi(hz) - static_cast<float>(midi)) * 100.0f));
    return juce::String(noteNames[pc]) + juce::String(midi / 12 - 1) + " " + juce::String(hz, 1) + " Hz "
        + (cents >= 0 ? "+" : "") + juce::String(cents) + "c";
}

juce::String HoneyBadgerKickCartographerAudioProcessor::compatibleKeysForHz(float hz)
{
    const auto pc = pitchClassForHz(hz);
    if (pc < 0)
        return "Waiting for pitch";

    const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
    const int minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };
    juce::StringArray keys;

    for (int root = 0; root < 12; ++root)
    {
        for (auto degree : majorScale)
            if ((root + degree) % 12 == pc)
            {
                keys.add(juce::String(noteNames[root]) + " maj");
                break;
            }
    }

    for (int root = 0; root < 12; ++root)
    {
        for (auto degree : minorScale)
            if ((root + degree) % 12 == pc)
            {
                keys.add(juce::String(noteNames[root]) + " min");
                break;
            }
    }

    return keys.joinIntoString(", ");
}

void HoneyBadgerKickCartographerAudioProcessor::resetCurrentSnapshot(int barNumber)
{
    current = {};
    current.bar = barNumber;
    current.peakDb = minusInfinityDb;
    current.rmsDb = minusInfinityDb;
    shownBar.store(barNumber);
}

void HoneyBadgerKickCartographerAudioProcessor::publishCurrentSnapshot()
{
    if (current.sampleCount > 0)
        current.rmsDb = linearToDb(static_cast<float>(std::sqrt(current.sumSquares / static_cast<double>(current.sampleCount))));

    if (current.punchHz > 0.0f && current.tailHz > 0.0f)
        current.driftSemis = hzToMidi(current.tailHz) - hzToMidi(current.punchHz);

    Snapshot snapshot;
    snapshot.bar = current.bar;
    snapshot.punchHz = current.punchHz;
    snapshot.tailHz = current.tailHz;
    snapshot.driftSemis = current.driftSemis;
    snapshot.peakDb = current.peakDb;
    snapshot.rmsDb = current.rmsDb;
    snapshot.confidence = static_cast<int>(std::round(100.0f * juce::jlimit(0.0f, 1.0f, current.confidence)));

    const auto index = historyWriteIndex.load() % historySize;
    history[static_cast<size_t>(index)] = snapshot;
    historyWriteIndex.store((index + 1) % historySize);
}

HoneyBadgerKickCartographerAudioProcessor::Snapshot HoneyBadgerKickCartographerAudioProcessor::getSnapshot(int newestOffset) const noexcept
{
    const auto write = historyWriteIndex.load();
    const auto index = (write - 1 - newestOffset + historySize * 4) % historySize;
    return history[static_cast<size_t>(index)];
}

void HoneyBadgerKickCartographerAudioProcessor::updateHostBarPosition(int numSamples)
{
    bool foundHostPosition = false;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            const auto ppq = position->getPpqPosition();
            const auto timeSig = position->getTimeSignature();
            if (ppq.hasValue())
            {
                const auto beatsPerBar = timeSig.hasValue() ? juce::jmax(1, timeSig->numerator) : defaultBeatsPerBar;
                const auto bar = static_cast<int>(std::floor(*ppq / static_cast<double>(beatsPerBar))) + 1;
                foundHostPosition = true;

                if (!haveHostBar)
                {
                    currentHostBar = bar;
                    resetCurrentSnapshot(bar);
                    haveHostBar = true;
                }
                else if (bar != currentHostBar)
                {
                    publishCurrentSnapshot();
                    currentHostBar = bar;
                    resetCurrentSnapshot(bar);
                }
            }
        }
    }

    if (!foundHostPosition)
    {
        const auto samplesPerBar = static_cast<int>((60.0 / defaultBpm) * defaultBeatsPerBar * currentSampleRate);
        fallbackSamplesIntoBar += numSamples;
        if (fallbackSamplesIntoBar >= samplesPerBar)
        {
            fallbackSamplesIntoBar -= samplesPerBar;
            publishCurrentSnapshot();
            resetCurrentSnapshot(current.bar + 1);
        }
    }
}

void HoneyBadgerKickCartographerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin(2, getTotalNumInputChannels());
    const auto totalChannels = getTotalNumOutputChannels();

    float blockPeak = 0.0f;
    double sumSquares = 0.0;
    int count = 0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto left = buffer.getReadPointer(0)[sample];
        const auto right = numChannels > 1 ? buffer.getReadPointer(1)[sample] : left;
        const auto mono = 0.5f * (left + right);

        if (!ring.empty())
        {
            ring[static_cast<size_t>(ringWrite)] = mono;
            ringWrite = (ringWrite + 1) % pitchFrameSize;
        }

        blockPeak = juce::jmax(blockPeak, std::abs(left), std::abs(right));
        sumSquares += static_cast<double>(left) * left + static_cast<double>(right) * right;
        count += 2;

        if (--samplesUntilPitch <= 0)
        {
            samplesUntilPitch = pitchHopSamples;
            analyzePitchFrame(linearToDb(blockPeak));
        }
    }

    for (int channel = numChannels; channel < totalChannels; ++channel)
        buffer.clear(channel, 0, numSamples);

    current.peakDb = juce::jmax(current.peakDb, linearToDb(blockPeak));
    current.sumSquares += sumSquares;
    current.sampleCount += count;

    livePeakDb.store(linearToDb(blockPeak));
    liveRmsDb.store(count > 0 ? linearToDb(static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count)))) : minusInfinityDb);

    updateHostBarPosition(numSamples);
}

void HoneyBadgerKickCartographerAudioProcessor::analyzePitchFrame(float peakDb)
{
    float confidence = 0.0f;
    const auto hz = estimateFundamental(confidence);
    liveHz.store(hz);
    liveConfidence.store(confidence);

    const auto minConfidence = parameters.getRawParameterValue("minConfidence")->load();
    if (hz <= 0.0f || confidence < minConfidence || peakDb < -54.0f)
        return;

    const auto sensitivity = parameters.getRawParameterValue("sensitivity")->load();
    const auto loudnessScore = juce::jmap(juce::jlimit(-54.0f, -3.0f, peakDb), -54.0f, -3.0f, 0.0f, 1.0f);
    const auto pitchScore = confidence * loudnessScore * sensitivity;

    if (pitchScore > current.punchScore)
    {
        current.punchScore = pitchScore;
        current.punchHz = hz;
    }

    if (confidence > minConfidence && peakDb < current.peakDb - 5.0f && pitchScore >= current.tailScore)
    {
        current.tailScore = pitchScore;
        current.tailHz = hz;
    }
    else if (current.tailHz <= 0.0f && current.punchHz > 0.0f)
    {
        current.tailHz = hz;
    }

    current.confidence = juce::jmax(current.confidence, confidence);
}

float HoneyBadgerKickCartographerAudioProcessor::estimateFundamental(float& confidenceOut) const noexcept
{
    confidenceOut = 0.0f;
    if (ring.size() < static_cast<size_t>(pitchFrameSize) || currentSampleRate <= 0.0)
        return 0.0f;

    float frame[pitchFrameSize];
    for (int i = 0; i < pitchFrameSize; ++i)
    {
        const auto sourceIndex = (ringWrite + i) % pitchFrameSize;
        const auto window = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(pitchFrameSize - 1));
        frame[i] = ring[static_cast<size_t>(sourceIndex)] * window;
    }

    double energy = 0.0;
    for (auto sample : frame)
        energy += static_cast<double>(sample) * sample;

    if (energy < 0.000001)
        return 0.0f;

    const auto minLag = juce::jlimit(1, pitchFrameSize - 2, static_cast<int>(currentSampleRate / 260.0));
    const auto maxLag = juce::jlimit(minLag + 1, pitchFrameSize - 2, static_cast<int>(currentSampleRate / 28.0));
    float best = 0.0f;
    int bestLag = 0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double corr = 0.0;
        double lagEnergy = 0.0;
        for (int i = 0; i < pitchFrameSize - lag; ++i)
        {
            corr += static_cast<double>(frame[i]) * frame[i + lag];
            lagEnergy += static_cast<double>(frame[i + lag]) * frame[i + lag];
        }

        const auto normalized = static_cast<float>(corr / std::sqrt((energy + 0.00000001) * (lagEnergy + 0.00000001)));
        if (normalized > best)
        {
            best = normalized;
            bestLag = lag;
        }
    }

    if (bestLag <= 0 || best < 0.10f)
        return 0.0f;

    confidenceOut = juce::jlimit(0.0f, 1.0f, best);
    return static_cast<float>(currentSampleRate) / static_cast<float>(bestLag);
}

void HoneyBadgerKickCartographerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void HoneyBadgerKickCartographerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* HoneyBadgerKickCartographerAudioProcessor::createEditor()
{
    return new HoneyBadgerKickCartographerAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HoneyBadgerKickCartographerAudioProcessor();
}
