#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class HoneyBadgerKickCartographerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                              private juce::Timer
{
public:
    explicit HoneyBadgerKickCartographerAudioProcessorEditor(HoneyBadgerKickCartographerAudioProcessor&);
    ~HoneyBadgerKickCartographerAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void configureReadout(juce::Label&);
    void configureSlider(juce::Slider&, juce::Label&, const juce::String&);
    void paintHistory(juce::Graphics&, juce::Rectangle<int>);
    void paintSnapshot(juce::Graphics&, juce::Rectangle<int>, const HoneyBadgerKickCartographerAudioProcessor::Snapshot&, int index);

    HoneyBadgerKickCartographerAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label liveLabel;
    juce::Label punchLabel;
    juce::Label tailLabel;
    juce::Label driftLabel;
    juce::Label keysLabel;
    juce::Label levelLabel;
    juce::Label sensitivityLabel;
    juce::Label minConfidenceLabel;
    juce::Slider sensitivitySlider;
    juce::Slider minConfidenceSlider;

    std::unique_ptr<SliderAttachment> sensitivityAttachment;
    std::unique_ptr<SliderAttachment> minConfidenceAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoneyBadgerKickCartographerAudioProcessorEditor)
};
