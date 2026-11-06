#include "PluginEditor.h"

using namespace juce;

static const Colour kBg     (0xff14161a);
static const Colour kPanel  (0xff1e2127);
static const Colour kAccent (0xffff8a3d);
static const Colour kText   (0xffe8e8ea);

// ---------------------------------------------------------------- RoomPad
RoomPad::RoomPad (AudioProcessorValueTreeState& s)
    : px (s.getParameter ("x")), py (s.getParameter ("y")), pmix (s.getParameter ("mix"))
{
    startTimerHz (30);
}

void RoomPad::paint (Graphics& g)
{
    auto b = area();
    g.setColour (kPanel);
    g.fillRoundedRectangle (b, 12.f);
    g.setColour (kText.withAlpha (0.15f));
    g.drawRoundedRectangle (b, 12.f, 1.5f);

    const float margin = 26.f;
    const Point<float> listener (b.getCentreX(), b.getBottom() - margin);

    // distance rings
    for (int k = 1; k <= 5; ++k)
    {
        const float rad = (float) k * (b.getHeight() - 2 * margin) / 5.f;
        g.setColour (kText.withAlpha (0.07f));
        g.drawEllipse (listener.x - rad, listener.y - rad, rad * 2, rad * 2, 1.f);
    }
    g.setColour (kText.withAlpha (0.07f));
    g.drawVerticalLine ((int) b.getCentreX(), b.getY() + 6, b.getBottom() - 6);

    // source
    const float nx = px->getValue(), ny = py->getValue();
    const Point<float> src (b.getX() + margin + nx * (b.getWidth() - 2 * margin),
                            b.getBottom() - margin - ny * (b.getHeight() - 2 * margin));

    // reverb halo grows with distance and mix
    const float halo = 18.f + 70.f * ny * (0.3f + pmix->getValue());
    ColourGradient grad (kAccent.withAlpha (0.35f), src, kAccent.withAlpha (0.f),
                         src.translated (halo, 0), true);
    g.setGradientFill (grad);
    g.fillEllipse (src.x - halo, src.y - halo, halo * 2, halo * 2);

    g.setColour (kText.withAlpha (0.4f));
    g.drawLine (Line<float> (listener, src), 1.f);

    g.setColour (kAccent);
    g.fillEllipse (src.x - 9, src.y - 9, 18, 18);
    g.setColour (Colours::white);
    g.drawEllipse (src.x - 9, src.y - 9, 18, 18, 2.f);

    // listener
    g.setColour (kText);
    g.fillEllipse (listener.x - 6, listener.y - 6, 12, 12);
    g.setFont (Font (FontOptions (11.f)));
    g.setColour (kText.withAlpha (0.5f));
    g.drawText ("LISTENER", Rectangle<float> (listener.x - 40, listener.y + 7, 80, 14), Justification::centred);
    g.drawText ("L", Rectangle<float> (b.getX() + 8, b.getCentreY() - 8, 16, 16), Justification::centred);
    g.drawText ("R", Rectangle<float> (b.getRight() - 24, b.getCentreY() - 8, 16, 16), Justification::centred);
    g.drawText ("FAR", Rectangle<float> (b.getCentreX() - 20, b.getY() + 6, 40, 14), Justification::centred);
}

void RoomPad::setFromMouse (const MouseEvent& e)
{
    auto b = area();
    const float margin = 26.f;
    const float nx = jlimit (0.f, 1.f, (e.position.x - b.getX() - margin) / (b.getWidth() - 2 * margin));
    const float ny = jlimit (0.f, 1.f, (b.getBottom() - margin - e.position.y) / (b.getHeight() - 2 * margin));
    px->setValueNotifyingHost (nx);
    py->setValueNotifyingHost (ny);
}

void RoomPad::mouseDown (const MouseEvent& e)
{
    px->beginChangeGesture(); py->beginChangeGesture();
    setFromMouse (e);
}
void RoomPad::mouseDrag (const MouseEvent& e) { setFromMouse (e); }
void RoomPad::mouseUp (const MouseEvent&)
{
    px->endChangeGesture(); py->endChangeGesture();
}
void RoomPad::mouseDoubleClick (const MouseEvent&)
{
    px->beginChangeGesture(); py->beginChangeGesture();
    px->setValueNotifyingHost (px->getDefaultValue());
    py->setValueNotifyingHost (py->getDefaultValue());
    px->endChangeGesture(); py->endChangeGesture();
}

// ----------------------------------------------------------------- Editor
void RoamVerbEditor::setupKnob (Slider& s, Label& l, const String& text)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible (s);
    l.setText (text, dontSendNotification);
    l.setJustificationType (Justification::centred);
    addAndMakeVisible (l);
}

RoamVerbEditor::RoamVerbEditor (RoamVerbProcessor& p)
    : AudioProcessorEditor (&p), proc (p), pad (p.apvts)
{
    laf.setColour (Slider::rotarySliderFillColourId, kAccent);
    laf.setColour (Slider::rotarySliderOutlineColourId, kPanel.brighter (0.2f));
    laf.setColour (Slider::thumbColourId, kText);
    laf.setColour (Slider::textBoxTextColourId, kText);
    laf.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    laf.setColour (Label::textColourId, kText);
    laf.setColour (ComboBox::backgroundColourId, kPanel);
    laf.setColour (ComboBox::textColourId, kText);
    laf.setColour (ComboBox::outlineColourId, kText.withAlpha (0.2f));
    laf.setColour (ComboBox::arrowColourId, kAccent);
    laf.setColour (PopupMenu::backgroundColourId, kPanel);
    laf.setColour (PopupMenu::textColourId, kText);
    laf.setColour (PopupMenu::highlightedBackgroundColourId, kAccent);
    laf.setColour (TextButton::buttonColourId, kPanel);
    laf.setColour (TextButton::textColourOffId, kText);
    setLookAndFeel (&laf);

    addAndMakeVisible (pad);

    if (auto* choice = dynamic_cast<AudioParameterChoice*> (p.apvts.getParameter ("room")))
        roomBox.addItemList (choice->choices, 1);
    addAndMakeVisible (roomBox);
    roomAtt = std::make_unique<CA> (p.apvts, "room", roomBox);

    addAndMakeVisible (loadBtn);
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Choose an impulse response",
                                                 File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.loadCustomIR (f);
                              });
    };

    setupKnob (mixKnob, mixLbl, "REVERB");
    setupKnob (preKnob, preLbl, "PRE-DELAY");
    setupKnob (widthKnob, widthLbl, "WIDTH");
    mixAtt   = std::make_unique<SA> (p.apvts, "mix", mixKnob);
    preAtt   = std::make_unique<SA> (p.apvts, "predelay", preKnob);
    widthAtt = std::make_unique<SA> (p.apvts, "width", widthKnob);

    setSize (660, 420);
}

RoamVerbEditor::~RoamVerbEditor() { setLookAndFeel (nullptr); }

void RoamVerbEditor::paint (Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kText);
    g.setFont (Font (FontOptions (26.f, Font::bold)));
    g.drawText ("ROAM", 410, 14, 100, 32, Justification::centredLeft);
    g.setColour (kAccent);
    g.drawText ("VERB", 490, 14, 100, 32, Justification::centredLeft);
    g.setColour (kText.withAlpha (0.5f));
    g.setFont (Font (FontOptions (11.f)));
    g.drawText ("Drag the dot to place your sound in the room", 410, 44, 240, 16, Justification::centredLeft);
}

void RoamVerbEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    pad.setBounds (r.removeFromLeft (380));
    r.removeFromLeft (20);
    r.removeFromTop (70);

    roomBox.setBounds (r.removeFromTop (30));
    r.removeFromTop (8);
    loadBtn.setBounds (r.removeFromTop (28));
    r.removeFromTop (24);

    auto row = r.removeFromTop (120);
    const int w = row.getWidth() / 3;
    Slider* knobs[3]  = { &mixKnob, &preKnob, &widthKnob };
    Label*  labels[3] = { &mixLbl,  &preLbl,  &widthLbl };
    for (int i = 0; i < 3; ++i)
    {
        auto c = row.removeFromLeft (w);
        labels[i]->setBounds (c.removeFromTop (16));
        knobs[i]->setBounds (c);
    }
}
