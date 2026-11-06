#pragma once
#include "PluginProcessor.h"

// Interactive room: drag the dot to place the sound. Left/right = pan,
// up/down = distance from the listener (bottom centre).
class RoomPad : public juce::Component, private juce::Timer
{
public:
    explicit RoomPad (juce::AudioProcessorValueTreeState& s);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void setFromMouse (const juce::MouseEvent&);
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (6.f); }

    juce::RangedAudioParameter *px, *py, *pmix;
};

class RoamVerbEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoamVerbEditor (RoamVerbProcessor&);
    ~RoamVerbEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void setupKnob (juce::Slider&, juce::Label&, const juce::String& text);

    RoamVerbProcessor& proc;
    juce::LookAndFeel_V4 laf;
    RoomPad pad;
    juce::ComboBox roomBox;
    juce::TextButton loadBtn { "Load IR..." };
    juce::Slider mixKnob, preKnob, widthKnob;
    juce::Label mixLbl, preLbl, widthLbl;
    std::unique_ptr<CA> roomAtt;
    std::unique_ptr<SA> mixAtt, preAtt, widthAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoamVerbEditor)
};
