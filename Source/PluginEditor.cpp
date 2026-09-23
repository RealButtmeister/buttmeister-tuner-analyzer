#include "PluginEditor.h"

HoneyBadgerKickCartographerAudioProcessorEditor::HoneyBadgerKickCartographerAudioProcessorEditor(HoneyBadgerKickCartographerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(920, 560);

    titleLabel.setText("Buttmeister Tuner / Analyzer", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    for (auto* label : { &liveLabel, &punchLabel, &tailLabel, &driftLabel, &keysLabel, &levelLabel })
        configureReadout(*label);

    configureSlider(sensitivitySlider, sensitivityLabel, "Transient Sensitivity");
    configureSlider(minConfidenceSlider, minConfidenceLabel, "Minimum Confidence");

    auto& state = audioProcessor.getValueTreeState();
    sensitivityAttachment = std::make_unique<SliderAttachment>(state, "sensitivity", sensitivitySlider);
    minConfidenceAttachment = std::make_unique<SliderAttachment>(state, "minConfidence", minConfidenceSlider);

    startTimerHz(24);
}

void HoneyBadgerKickCartographerAudioProcessorEditor::configureReadout(juce::Label& label)
{
    label.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);
}

void HoneyBadgerKickCartographerAudioProcessorEditor::configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& name)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff22c7a9));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    label.setText(name, juce::dontSendNotification);
    label.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(label);
    addAndMakeVisible(slider);
}

void HoneyBadgerKickCartographerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff080b0d));

    auto bounds = getLocalBounds().toFloat().reduced(16.0f);
    g.setColour(juce::Colour(0xff10181b));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff22c7a9));
    g.drawRoundedRectangle(bounds, 8.0f, 1.3f);

    auto livePanel = juce::Rectangle<float>(34.0f, 82.0f, static_cast<float>(getWidth() - 68), 136.0f);
    g.setColour(juce::Colour(0xff061112));
    g.fillRoundedRectangle(livePanel, 7.0f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(livePanel, 7.0f, 1.0f);

    paintHistory(g, juce::Rectangle<int>(34, 256, getWidth() - 68, 190));
}

void HoneyBadgerKickCartographerAudioProcessorEditor::paintHistory(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("Recent bar snapshots", area.removeFromTop(24), juce::Justification::centredLeft);

    const auto count = audioProcessor.getHistorySize();
    const auto gap = 6;
    const auto tileWidth = (area.getWidth() - gap * (count - 1)) / count;

    for (int i = count - 1; i >= 0; --i)
    {
        auto tile = area.removeFromLeft(tileWidth);
        paintSnapshot(g, tile, audioProcessor.getSnapshot(i), i);
        area.removeFromLeft(gap);
    }
}

void HoneyBadgerKickCartographerAudioProcessorEditor::paintSnapshot(juce::Graphics& g,
                                                                    juce::Rectangle<int> tile,
                                                                    const HoneyBadgerKickCartographerAudioProcessor::Snapshot& snapshot,
                                                                    int index)
{
    juce::ignoreUnused(index);

    const auto hasPitch = snapshot.punchHz > 0.0f || snapshot.tailHz > 0.0f;
    g.setColour(hasPitch ? juce::Colour(0xff132a2b) : juce::Colour(0xff111619));
    g.fillRoundedRectangle(tile.toFloat(), 6.0f);
    g.setColour(hasPitch ? juce::Colour(0xff22c7a9).withAlpha(0.50f) : juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(tile.toFloat(), 6.0f, 1.0f);

    auto inner = tile.reduced(6);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(juce::Colours::white.withAlpha(0.70f));
    g.drawText(snapshot.bar > 0 ? "Bar " + juce::String(snapshot.bar) : "--", inner.removeFromTop(20), juce::Justification::centred);

    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawText(HoneyBadgerKickCartographerAudioProcessor::hzToNoteName(snapshot.punchHz), inner.removeFromTop(36), juce::Justification::centred);

    g.setColour(juce::Colour(0xffffd166));
    g.drawText(snapshot.tailHz > 0.0f ? HoneyBadgerKickCartographerAudioProcessor::hzToNoteName(snapshot.tailHz) : "--", inner.removeFromTop(34), juce::Justification::centred);

    g.setColour(juce::Colour(0xff9bd8ff));
    g.drawText(snapshot.punchHz > 0.0f && snapshot.tailHz > 0.0f ? juce::String(snapshot.driftSemis, 1) + " st" : "--", inner.removeFromTop(24), juce::Justification::centred);

    g.setColour(juce::Colours::white.withAlpha(0.60f));
    g.drawText(snapshot.confidence > 0 ? juce::String(snapshot.confidence) + "%" : "--", inner, juce::Justification::centred);
}

void HoneyBadgerKickCartographerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(34);
    titleLabel.setBounds(area.removeFromTop(36));
    area.removeFromTop(18);

    auto liveArea = area.removeFromTop(136).reduced(14, 8);
    auto left = liveArea.removeFromLeft(360);
    liveLabel.setBounds(left.removeFromTop(34));
    punchLabel.setBounds(left.removeFromTop(34));
    tailLabel.setBounds(left.removeFromTop(34));

    auto right = liveArea;
    driftLabel.setBounds(right.removeFromTop(34));
    levelLabel.setBounds(right.removeFromTop(34));
    keysLabel.setBounds(right.removeFromTop(52));

    area.removeFromTop(218);

    auto sliderRow = area.removeFromTop(68);
    auto sens = sliderRow.removeFromLeft(getWidth() / 2 - 40).reduced(2, 0);
    sensitivityLabel.setBounds(sens.removeFromTop(20));
    sensitivitySlider.setBounds(sens.removeFromTop(32));

    auto conf = sliderRow.reduced(2, 0);
    minConfidenceLabel.setBounds(conf.removeFromTop(20));
    minConfidenceSlider.setBounds(conf.removeFromTop(32));
}

void HoneyBadgerKickCartographerAudioProcessorEditor::timerCallback()
{
    const auto latest = audioProcessor.getSnapshot(0);
    const auto liveHz = audioProcessor.getLiveHz();

    liveLabel.setText("Live: " + HoneyBadgerKickCartographerAudioProcessor::hzToNoteName(liveHz)
        + "  confidence " + juce::String(audioProcessor.getLiveConfidence() * 100.0f, 0) + "%", juce::dontSendNotification);
    punchLabel.setText("Punch: " + HoneyBadgerKickCartographerAudioProcessor::hzToNoteName(latest.punchHz), juce::dontSendNotification);
    tailLabel.setText("Tail: " + HoneyBadgerKickCartographerAudioProcessor::hzToNoteName(latest.tailHz), juce::dontSendNotification);
    driftLabel.setText("Drift: " + (latest.punchHz > 0.0f && latest.tailHz > 0.0f ? juce::String(latest.driftSemis, 2) + " semitones" : "--"), juce::dontSendNotification);
    levelLabel.setText("Peak " + juce::String(audioProcessor.getLivePeakDb(), 1) + " dBFS / RMS "
        + juce::String(audioProcessor.getLiveRmsDb(), 1) + " dBFS", juce::dontSendNotification);
    keysLabel.setText("Keys: " + HoneyBadgerKickCartographerAudioProcessor::compatibleKeysForHz(latest.tailHz > 0.0f ? latest.tailHz : latest.punchHz), juce::dontSendNotification);

    repaint();
}
