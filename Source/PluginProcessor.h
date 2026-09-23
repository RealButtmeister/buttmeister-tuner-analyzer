#pragma once

#include <JuceHeader.h>

class HoneyBadgerKickCartographerAudioProcessor final : public juce::AudioProcessor
{
public:
    HoneyBadgerKickCartographerAudioProcessor();
    ~HoneyBadgerKickCartographerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

    struct Snapshot
    {
        int bar = 0;
        float punchHz = 0.0f;
        float tailHz = 0.0f;
        float driftSemis = 0.0f;
        float peakDb = -90.0f;
        float rmsDb = -90.0f;
        int confidence = 0;
    };

    Snapshot getSnapshot(int newestOffset) const noexcept;
    int getHistorySize() const noexcept { return historySize; }
    int getWriteIndex() const noexcept { return historyWriteIndex.load(); }
    int getCurrentBar() const noexcept { return shownBar.load(); }
    float getLiveHz() const noexcept { return liveHz.load(); }
    float getLiveConfidence() const noexcept { return liveConfidence.load(); }
    float getLivePeakDb() const noexcept { return livePeakDb.load(); }
    float getLiveRmsDb() const noexcept { return liveRmsDb.load(); }

    static juce::String hzToNoteName(float hz);
    static juce::String compatibleKeysForHz(float hz);
    static float hzToMidi(float hz) noexcept;

private:
    struct MutableSnapshot
    {
        int bar = 0;
        float punchHz = 0.0f;
        float tailHz = 0.0f;
        float driftSemis = 0.0f;
        float peakDb = -90.0f;
        float rmsDb = -90.0f;
        float confidence = 0.0f;
        float punchScore = 0.0f;
        float tailScore = 0.0f;
        double sumSquares = 0.0;
        int sampleCount = 0;
    };

    static constexpr int historySize = 16;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float linearToDb(float value) noexcept;
    static float midiToHz(float midi) noexcept;
    static int pitchClassForHz(float hz) noexcept;

    void resetCurrentSnapshot(int barNumber);
    void publishCurrentSnapshot();
    void analyzePitchFrame(float peakDb);
    float estimateFundamental(float& confidenceOut) const noexcept;
    void updateHostBarPosition(int numSamples);

    juce::AudioProcessorValueTreeState parameters;

    double currentSampleRate = 44100.0;
    std::vector<float> ring;
    int ringWrite = 0;
    int samplesUntilPitch = 0;
    int fallbackSamplesIntoBar = 0;
    int currentHostBar = 0;
    bool haveHostBar = false;
    MutableSnapshot current;
    std::array<Snapshot, historySize> history {};

    std::atomic<int> historyWriteIndex { 0 };
    std::atomic<int> shownBar { 0 };
    std::atomic<float> liveHz { 0.0f };
    std::atomic<float> liveConfidence { 0.0f };
    std::atomic<float> livePeakDb { -90.0f };
    std::atomic<float> liveRmsDb { -90.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoneyBadgerKickCartographerAudioProcessor)
};
