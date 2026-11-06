#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout RoamVerbProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    StringArray names;
    for (auto& r : roomSpecs()) names.add (r.name);
    names.add ("Custom IR");

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "room", 1 }, "Room", names, 3));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "x", 1 }, "Position X",
                    NormalisableRange<float> (-1.f, 1.f, 0.001f), 0.f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "y", 1 }, "Position Depth",
                    NormalisableRange<float> (0.f, 1.f, 0.001f), 0.3f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Reverb",
                    NormalisableRange<float> (0.f, 1.f, 0.001f), 0.35f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "predelay", 1 }, "Pre-Delay",
                    NormalisableRange<float> (0.f, 200.f, 0.1f), 10.f, AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width",
                    NormalisableRange<float> (0.f, 2.f, 0.001f), 1.f));

    return { p.begin(), p.end() };
}

RoamVerbProcessor::RoamVerbProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    roomP  = apvts.getRawParameterValue ("room");
    xP     = apvts.getRawParameterValue ("x");
    yP     = apvts.getRawParameterValue ("y");
    mixP   = apvts.getRawParameterValue ("mix");
    preP   = apvts.getRawParameterValue ("predelay");
    widthP = apvts.getRawParameterValue ("width");

    apvts.addParameterListener ("room", this);
    startTimerHz (20);
}

RoamVerbProcessor::~RoamVerbProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("room", this);
}

bool RoamVerbProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet()  == AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void RoamVerbProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    dsp::ProcessSpec spec { sampleRate, (uint32) blockSize, 2 };

    convolution.prepare (spec);
    preDelay.setMaximumDelayInSamples ((int) (sampleRate * 0.5));
    preDelay.prepare (spec);
    preDelay.reset();

    for (auto& f : dryLP)
    {
        f.prepare (dsp::ProcessSpec { sampleRate, (uint32) blockSize, 1 });
        f.setType (dsp::FirstOrderTPTFilterType::lowpass);
        f.reset();
    }

    wet.setSize (2, blockSize);

    xSm.reset (sampleRate, 0.03);   xSm.setCurrentAndTargetValue (*xP);
    ySm.reset (sampleRate, 0.03);   ySm.setCurrentAndTargetValue (*yP);
    preSm.reset (sampleRate, 0.05); preSm.setCurrentAndTargetValue (*preP);
    wetGainSm.reset (sampleRate, 0.03); wetGainSm.setCurrentAndTargetValue (0.f);
    widthSm.reset (sampleRate, 0.03);   widthSm.setCurrentAndTargetValue (*widthP);

    prepared = true;
    irDirty = true;
}

void RoamVerbProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    wet.setSize (2, n, false, false, true);

    auto* l  = buffer.getWritePointer (0);
    auto* r  = buffer.getWritePointer (1);
    auto* wl = wet.getWritePointer (0);
    auto* wr = wet.getWritePointer (1);

    xSm.setTargetValue (xP->load());
    ySm.setTargetValue (yP->load());
    preSm.setTargetValue (preP->load());
    widthSm.setTargetValue (widthP->load());
    const float mix = mixP->load();
    // Farther away = more reverberant relative to the direct sound.
    wetGainSm.setTargetValue (mix * Decibels::decibelsToGain (-6.f * (1.f - ySm.getTargetValue())));

    const float maxCut = (float) (0.45 * sr);

    for (int i = 0; i < n; ++i)
    {
        const float x = xSm.getNextValue();
        const float y = ySm.getNextValue();
        const float pre = preSm.getNextValue() + 30.f * (1.f - y); // closer = bigger gap

        // Equal-power balance for the direct sound
        const float a  = (x + 1.f) * MathConstants<float>::pi * 0.25f;
        const float gL = std::cos (a) * MathConstants<float>::sqrt2;
        const float gR = std::sin (a) * MathConstants<float>::sqrt2;
        // Reverb send follows the source only partially
        const float aw  = (x * 0.5f + 1.f) * MathConstants<float>::pi * 0.25f;
        const float wgL = std::cos (aw) * MathConstants<float>::sqrt2;
        const float wgR = std::sin (aw) * MathConstants<float>::sqrt2;

        const float inL = l[i], inR = r[i];

        preDelay.setDelay (jmin ((float) (sr * 0.5) - 2.f, pre * 0.001f * (float) sr));
        preDelay.pushSample (0, inL * wgL);
        preDelay.pushSample (1, inR * wgR);
        wl[i] = preDelay.popSample (0, -1.f, true);
        wr[i] = preDelay.popSample (1, -1.f, true);

        // Distance: quieter + duller direct sound (air absorption)
        const float cutoff = jmin (maxCut, 20000.f * std::pow (0.25f, y));
        dryLP[0].setCutoffFrequency (cutoff);
        dryLP[1].setCutoffFrequency (cutoff);
        const float dg = Decibels::decibelsToGain (-18.f * y);
        l[i] = dryLP[0].processSample (0, inL * gL) * dg;
        r[i] = dryLP[1].processSample (0, inR * gR) * dg;
    }

    dsp::AudioBlock<float> block (wet);
    convolution.process (dsp::ProcessContextReplacing<float> (block));

    for (int i = 0; i < n; ++i)
    {
        const float g = wetGainSm.getNextValue();
        const float w = widthSm.getNextValue();
        const float m = 0.5f * (wl[i] + wr[i]);
        const float s = 0.5f * (wl[i] - wr[i]) * w;
        l[i] += g * (m + s);
        r[i] += g * (m - s);
    }
}

void RoamVerbProcessor::timerCallback()
{
    if (prepared && irDirty.exchange (false))
        applyIR();
}

void RoamVerbProcessor::applyIR()
{
    using C = dsp::Convolution;
    const int idx = (int) roomP->load();

    if (idx >= customRoomIndex())
    {
        File f (apvts.state.getProperty ("customIR").toString());
        if (f.existsAsFile())
            convolution.loadImpulseResponse (f, C::Stereo::yes, C::Trim::yes, 0, C::Normalise::yes);
        return;
    }

    auto buf = makeIR (roomSpecs()[(size_t) idx], 48000.0, idx + 1);
    convolution.loadImpulseResponse (std::move (buf), 48000.0,
                                     C::Stereo::yes, C::Trim::no, C::Normalise::yes);
}

void RoamVerbProcessor::loadCustomIR (const File& f)
{
    apvts.state.setProperty ("customIR", f.getFullPathName(), nullptr);
    if (auto* p = apvts.getParameter ("room"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) customRoomIndex()));
    irDirty = true;
}

void RoamVerbProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void RoamVerbProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            irDirty = true;
        }
}

AudioProcessorEditor* RoamVerbProcessor::createEditor() { return new RoamVerbEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RoamVerbProcessor(); }
