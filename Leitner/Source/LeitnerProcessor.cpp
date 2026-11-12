#include "LeitnerProcessor.h"
#include "LeitnerEditor.h"

using namespace juce;

// Stereo in; out, Ambisonics. As an Audio Unit and in the desktop app 3rd order (16 channels) is
// published; everywhere else up to 7th (64), so a track with that many has room for it. The order
// setting says how many are used.
AudioProcessor::BusesProperties LeitnerProcessor::buses()
{
    const auto t = PluginHostType::getPluginLoadedAs();
    const bool third = t == AudioProcessor::wrapperType_AudioUnit || t == AudioProcessor::wrapperType_Standalone;
    return BusesProperties()
        .withInput  ("Input",  AudioChannelSet::stereo(), true)
        .withOutput ("Output", AudioChannelSet::ambisonic (third ? 3 : 7), true);
}

LeitnerProcessor::LeitnerProcessor() : CloudfinProcessor (buses())
{
    auto secs = [] (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; };
    auto pct  = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto deg  = [] (float v, int) { return String (roundToInt (v)) + String::fromUTF8 ("\xc2\xb0"); };
    using A = AudioParameterFloatAttributes;
    auto f = [this] (AudioParameterFloat*& slot, const char* id, const char* name, NormalisableRange<float> r, float def, A attr)
    {
        auto p = std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, attr);
        slot = p.get(); spaceParams.push_back (slot); addParameter (p.release());
    };
    auto skew = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    {
        auto p = std::make_unique<AudioParameterChoice> (ParameterID { "spOrder", 1 }, "Ambisonic Order", StringArray { "3rd", "4th", "5th", "6th", "7th" }, 0);
        order = p.get(); spaceParams.push_back (order); addParameter (p.release());
    }
    {
        StringArray names;
        for (int i = 0; i < leitner::numFigures; ++i) names.add (String::fromUTF8 (leitner::figureNames()[i]));
        auto p = std::make_unique<AudioParameterChoice> (ParameterID { "spFigure", 1 }, "Figure", names, 0);
        figure = p.get(); spaceParams.push_back (figure); addParameter (p.release());
    }
    f (seconds, "spSeconds", "Pass Time", skew (0.05f, 60.0f, 4.0f), 4.0f, A().withStringFromValueFunction (secs));
    f (rest,    "spRest",    "Rest",     { 0.0f, 0.9f }, 0.0f, A().withStringFromValueFunction (pct));
    f (size,    "spSize",    "Size",     { 0.1f, 1.0f }, 0.9f, A().withStringFromValueFunction (pct));
    f (height,  "spHeight",  "Height",   { -1.0f, 1.0f }, 0.0f, A().withStringFromValueFunction ([] (float v, int) { return (v > 0.004f ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; }));
    f (turn,    "spTurn",    "Turn",     { -180.0f, 180.0f }, 0.0f, A().withStringFromValueFunction (deg));
    f (apart,   "spApart",   "Apart",    { 0.0f, 90.0f }, 30.0f, A().withStringFromValueFunction (deg));
    f (counter, "spCounter", "Counter",  { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    {
        auto p = std::make_unique<AudioParameterInt> (ParameterID { "spPoints", 1 }, "Points", 2, leitner::kMaxPoints, 8,
                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v >= leitner::kMaxPoints ? String ("continuous") : String (v); }));
        points = p.get(); spaceParams.push_back (points); addParameter (p.release());
    }
    f (legato,  "spLegato",  "Legato",   { 0.0f, 1.0f }, 0.5f, A().withStringFromValueFunction (pct));
    f (swell,   "spSwell",   "Swell",    { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (lift,    "spLift",    "Lift",     { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (chance,  "spChance",  "Chance",   { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (walk,    "spWalk",    "Walk",     { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (scatter, "spScatter", "Scatter",  { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    {
        StringArray names;
        for (int i = 0; i < leitner::kNumSieves; ++i) names.add (leitner::sieveNames()[i]);
        auto p = std::make_unique<AudioParameterChoice> (ParameterID { "spSieve", 1 }, "Sieve", names, 0);
        sieve = p.get(); spaceParams.push_back (sieve); addParameter (p.release());
    }
    f (pulse,      "spPulse",      "Pulse",       { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (pulseRate,  "spPulseRate",  "Pulse Rate",  skew (0.5f, 400.0f, 12.0f), 8.0f, A().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " Hz"; }));
    f (pulseBend,  "spPulseBend",  "Pulse Bend",  { -1.0f, 1.0f }, 0.0f, A().withStringFromValueFunction ([] (float v, int) { return (v > 0.004f ? "+" : "") + String (v * 4.0f, 1) + " oct"; }));
    f (pulseWidth, "spPulseWidth", "Pulse Width", { 0.02f, 1.0f }, 0.5f, A().withStringFromValueFunction (pct));
    f (drop,       "spDrop",       "Drop",        { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    auto ch = [this] (AudioParameterChoice*& slot, const char* id, const char* name, const char* const* names, int n)
    {
        StringArray list; for (int i = 0; i < n; ++i) list.add (names[i]);
        auto p = std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, list, 0);
        slot = p.get(); spaceParams.push_back (slot); addParameter (p.release());
    };
    auto in = [this] (AudioParameterInt*& slot, const char* id, const char* name, int lo, int hi, int def)
    {
        auto p = std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def);
        slot = p.get(); spaceParams.push_back (slot); addParameter (p.release());
    };
    ch (cell, "spCell", "Cell", leitner::cellNames(), leitner::kNumCells);
    ch (jux,  "spJux",  "Jux",  leitner::juxNames(), leitner::kNumJux);
    in (rotate, "spRotate", "Rotate", 0, 4, 0);
    in (hits,   "spHits",   "Hits",   1, 16, 8);
    in (steps,  "spSteps",  "Steps",  1, 16, 8);
    f (room,    "spRoom",    "Room",     { 0.0f, 1.0f }, 0.15f, A().withStringFromValueFunction (pct));
    f (roomDecay, "spRoomDecay", "Room Decay", skew (0.3f, 20.0f, 3.0f), 3.0f, A().withStringFromValueFunction (secs));
    f (level,   "spLevel",   "Level",    { 0.0f, 1.5f }, 1.0f, A().withStringFromValueFunction (pct));
    {
        auto p = std::make_unique<AudioParameterBool> (ParameterID { "spHold", 1 }, "Hold", false);
        hold = p.get(); spaceParams.push_back (hold); addParameter (p.release());
    }
}

bool LeitnerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    // any order from 3rd to 7th (and the lower ones, for a host that insists: the highest that fits comes out),
    // the same counts of plain channels, or stereo (a plain left/right fold of the scene, for checking)
    bool ok = out == AudioChannelSet::stereo();
    for (int o = 1; o <= fp::ambi::kMaxOrder; ++o)
    {
        ok = ok || out == AudioChannelSet::ambisonic (o);
        ok = ok || (out == AudioChannelSet::discreteChannels (fp::ambi::chFor (o)) && wrapperType != wrapperType_AudioUnit);
    }
    if (! ok) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

leitner::Params LeitnerProcessor::readSpace() const
{
    leitner::Params p;
    p.order = 3 + order->getIndex();
    p.figure = figure->getIndex();
    p.seconds = seconds->get(); p.rest = rest->get(); p.size = size->get(); p.height = height->get();
    p.turn = turn->get(); p.apart = apart->get(); p.counter = counter->get();
    p.points = points->get(); p.legato = legato->get(); p.swell = swell->get(); p.lift = lift->get();
    p.chance = chance->get(); p.walk = walk->get(); p.scatter = scatter->get(); p.sieve = sieve->getIndex();
    p.pulse = pulse->get(); p.pulseRate = pulseRate->get(); p.pulseBend = pulseBend->get(); p.pulseWidth = pulseWidth->get(); p.drop = drop->get();
    p.cell = cell->getIndex(); p.jux = jux->getIndex(); p.rotate = rotate->get(); p.hits = hits->get(); p.steps = steps->get();
    p.room = room->get(); p.roomDecay = roomDecay->get(); p.level = level->get();
    p.hold = hold->get();
    return p;
}

void LeitnerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    CloudfinProcessor::prepareToPlay (sampleRate, samplesPerBlock);
    space.prepare (sampleRate);
    stereo.setSize (2, jmax (1, samplesPerBlock), false, true, true);
}

void LeitnerProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = buffer.getNumChannels();
    if (n == 0 || numOut == 0) return;
    if (stereo.getNumSamples() < n) stereo.setSize (2, n, false, true, true);

    // Cloudfin itself, in stereo
    stereo.copyFrom (0, 0, buffer, 0, 0, n);
    stereo.copyFrom (1, 0, buffer, numIn > 1 && buffer.getNumChannels() > 1 ? 1 : 0, 0, n);
    {
        AudioBuffer<float> two (stereo.getArrayOfWritePointers(), 2, n);
        CloudfinProcessor::processBlock (two, midi);
    }

    auto p = readSpace();

    float* outs[fp::ambi::kCh];
    if (numOut >= 4)
    {
        const int nch = jmin (numOut, fp::ambi::kCh);
        for (int c = 0; c < nch; ++c) outs[c] = buffer.getWritePointer (c);
        for (int c = nch; c < numOut; ++c) buffer.clear (c, 0, n);
        space.process (stereo.getReadPointer (0), stereo.getReadPointer (1), outs, nch, n, p);
        const int o = jmin (p.order, fp::ambi::orderFor (nch));
        for (int c = fp::ambi::chFor (o); c < nch; ++c) buffer.clear (c, 0, n);
        orderNow.store (o); channelsNow.store (fp::ambi::chFor (o));
    }
    else
    {
        // a stereo (or mono) output: encode at 1st order and fold it to left and right, W + Y and W - Y,
        // so there's something to hear without a decoder
        static thread_local std::vector<float> scratch[4];
        for (auto& s : scratch) if ((int) s.size() < n) s.resize ((size_t) n);
        for (int c = 0; c < 4; ++c) outs[c] = scratch[c].data();
        p.order = 1;
        space.process (stereo.getReadPointer (0), stereo.getReadPointer (1), outs, 4, n, p);
        float* L = buffer.getWritePointer (0);
        float* R = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float w = outs[0][i], y = outs[1][i];
            if (R != nullptr) { L[i] = 0.7071f * (w + y); R[i] = 0.7071f * (w - y); } else L[i] = w;
        }
        orderNow.store (0); channelsNow.store (numOut);
    }
    outLevel.store (buffer.getMagnitude (0, 0, n));
}

AudioProcessorEditor* LeitnerProcessor::createEditor() { return new LeitnerEditor (*this); }

// the space's parameters travel as properties on Cloudfin's state
void LeitnerProcessor::getStateInformation (MemoryBlock& dest)
{
    for (auto* prm : spaceParams) apvts.state.setProperty (prm->getParameterID(), prm->getValue(), nullptr);
    CloudfinProcessor::getStateInformation (dest);
}

void LeitnerProcessor::setStateInformation (const void* data, int size)
{
    CloudfinProcessor::setStateInformation (data, size);
    for (auto* prm : spaceParams)
        if (apvts.state.hasProperty (prm->getParameterID()))
            prm->setValueNotifyingHost ((float) apvts.state.getProperty (prm->getParameterID()));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LeitnerProcessor(); }
