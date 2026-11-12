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

//==============================================================================
namespace
{
struct SpaceValue { const char* id; float v; };
struct SpaceProgram { const char* name; const char* group; const char* sound; std::vector<SpaceValue> space; };

// figures by number, as in leitner::Figure
enum { Line = 0, Swing, Liege, Gate, Vertical, Circle, Soundcube, Spiral, Cube, Expand, Vault, Paraboloid, Terretektorh, Polytope };

const std::vector<SpaceProgram>& spacePrograms()
{
    static const std::vector<SpaceProgram> list {
        // after Leitner
        { "Ton-Linie",           "Leitner", "Cloudfin",        {} },
        { "Through the Body",    "Leitner", "Clear Water",     { { "spFigure", Line }, { "spHeight", 0.0f }, { "spApart", 0.0f }, { "spSwell", 0.8f }, { "spSeconds", 3.0f }, { "spPoints", 12 }, { "spLegato", 0.7f } } },
        { "Ton-Tor",             "Leitner", "Vowel Drift",     { { "spFigure", Gate }, { "spCounter", 1.0f }, { "spLift", 0.7f }, { "spSeconds", 5.0f }, { "spPoints", 19 }, { "spLegato", 0.3f }, { "spApart", 20.0f } } },
        { "Schwingen",           "Leitner", "Slow Tide",       { { "spFigure", Swing }, { "spPoints", 33 }, { "spSeconds", 5.0f }, { "spRoom", 0.3f }, { "spApart", 15.0f } } },
        { "Ton-Liege",           "Leitner", "Deep Hydrophone", { { "spFigure", Liege }, { "spPoints", 4 }, { "spLegato", 1.0f }, { "spSeconds", 9.0f }, { "spApart", 10.0f }, { "spSwell", 0.4f } } },
        { "Soundcube 1969",      "Leitner", "Glass Stretch",   { { "spFigure", Soundcube }, { "spPoints", 32 }, { "spLegato", 0.2f }, { "spSeconds", 14.0f }, { "spRoom", 0.1f } } },
        { "Vertical Space",      "Leitner", "Yaman Shimmer",   { { "spFigure", Vertical }, { "spCounter", 1.0f }, { "spLift", 0.6f }, { "spPoints", 6 }, { "spLegato", 0.1f }, { "spSeconds", 2.5f }, { "spApart", 45.0f } } },
        { "Widening, Narrowing", "Leitner", "Bloom Loop",      { { "spFigure", Expand }, { "spApart", 90.0f }, { "spPoints", 33 }, { "spSeconds", 7.0f }, { "spSwell", 0.3f }, { "spLevel", 0.8f } } },
        { "Vault",               "Leitner", "Underwater Room", { { "spFigure", Vault }, { "spPoints", 33 }, { "spSeconds", 20.0f }, { "spRoom", 0.4f }, { "spRoomDecay", 8.0f } } },
        // after Xenakis
        { "Ruled Surface",       "Xenakis", "Harmonic Fountain", { { "spFigure", Paraboloid }, { "spPoints", 33 }, { "spCounter", 1.0f }, { "spSeconds", 8.0f }, { "spSize", 1.0f } } },
        { "Terretektorh",        "Xenakis", "Spectral Snow",   { { "spFigure", Terretektorh }, { "spPoints", 24 }, { "spLegato", 0.6f }, { "spSeconds", 6.0f }, { "spCounter", 0.5f } } },
        { "Polytope",            "Xenakis", "Pelog Echoes",    { { "spFigure", Polytope }, { "spChance", 1.0f }, { "spScatter", 0.4f }, { "spRest", 0.2f }, { "spPoints", 8 }, { "spLegato", 1.0f }, { "spSeconds", 4.0f } } },
        { "Sieve Ring",          "Xenakis", "Maqam Mist",      { { "spFigure", Circle }, { "spPoints", 16 }, { "spLegato", 0.0f }, { "spSieve", 2 }, { "spChance", 0.6f }, { "spSeconds", 3.0f } } },
        { "Brownian Line",       "Xenakis", "Tritave Choir",   { { "spFigure", Line }, { "spWalk", 0.7f }, { "spSeconds", 6.0f }, { "spPoints", 33 }, { "spSwell", 0.5f } } },
        // pulses, after Fell, Lazar and Kindohm
        { "Walking Accent",      "pulses",  "Clear Water",     { { "spFigure", Circle }, { "spPoints", 8 }, { "spPulse", 1.0f }, { "spPulseRate", 9.0f }, { "spCell", 3 }, { "spRotate", 1 }, { "spPulseWidth", 0.3f }, { "spSeconds", 4.0f }, { "spRoom", 0.05f } } },
        { "Rhythm Into Pitch",   "pulses",  "Cloudfin",        { { "spFigure", Line }, { "spPoints", 33 }, { "spPulse", 1.0f }, { "spPulseRate", 30.0f }, { "spPulseBend", 1.0f }, { "spPulseWidth", 0.2f }, { "spSeconds", 6.0f }, { "spApart", 40.0f } } },
        { "Five in Eight",       "pulses",  "Pelog Echoes",    { { "spFigure", Cube }, { "spPoints", 8 }, { "spLegato", 0.0f }, { "spPulse", 1.0f }, { "spPulseRate", 7.0f }, { "spHits", 5 }, { "spSteps", 8 }, { "spCell", 6 }, { "spJux", 3 }, { "spSeconds", 5.0f } } },
        { "Broken Pattern",      "pulses",  "Spectral Snow",   { { "spFigure", Polytope }, { "spPulse", 1.0f }, { "spPulseRate", 11.0f }, { "spHits", 5 }, { "spSteps", 8 }, { "spDrop", 0.3f }, { "spJux", 1 }, { "spChance", 0.8f }, { "spPoints", 8 }, { "spSeconds", 3.0f } } },
    };
    return list;
}
}

int LeitnerProcessor::numSpacePrograms() { return (int) spacePrograms().size(); }
int LeitnerProcessor::getNumPrograms() { return numSpacePrograms() + CloudfinProcessor::getNumPrograms(); }

const String LeitnerProcessor::getProgramName (int i)
{
    const int n = numSpacePrograms();
    if (isPositiveAndBelow (i, n)) return spacePrograms()[(size_t) i].name;
    return CloudfinProcessor::getProgramName (i - n);
}

String LeitnerProcessor::getProgramGroup (int i)
{
    const int n = numSpacePrograms();
    if (isPositiveAndBelow (i, n)) return spacePrograms()[(size_t) i].group;
    return "sound only: " + CloudfinProcessor::getProgramGroup (i - n);
}

void LeitnerProcessor::setCurrentProgram (int index)
{
    const int n = numSpacePrograms();
    if (! isPositiveAndBelow (index, getNumPrograms())) return;
    if (index >= n)
    {
        // one of Cloudfin's: the sound changes, the space stays as you have it
        std::vector<float> keep;
        for (auto* prm : spaceParams) keep.push_back (prm->getValue());
        CloudfinProcessor::setCurrentProgram (index - n);
        for (size_t i = 0; i < spaceParams.size(); ++i) spaceParams[i]->setValueNotifyingHost (keep[i]);
        program.store (index);
        return;
    }
    const auto& pr = spacePrograms()[(size_t) index];
    const float keepOrder = static_cast<RangedAudioParameter*> (order)->getValue();   // (the order belongs to the track, not the program)
    int sound = 0;
    for (int i = 0; i < CloudfinProcessor::getNumPrograms(); ++i) if (CloudfinProcessor::getProgramName (i) == pr.sound) sound = i;
    CloudfinProcessor::setCurrentProgram (sound);     // (this also puts every space parameter back to its default)
    for (auto& v : pr.space)
        for (auto* prm : spaceParams)
            if (prm->getParameterID() == v.id) prm->setValueNotifyingHost (prm->convertTo0to1 (v.v));
    order->setValueNotifyingHost (keepOrder);
    program.store (index);
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
    apvts.state.setProperty ("ltProgram", program.load(), nullptr);
    CloudfinProcessor::getStateInformation (dest);
}

void LeitnerProcessor::setStateInformation (const void* data, int size)
{
    CloudfinProcessor::setStateInformation (data, size);
    for (auto* prm : spaceParams)
        if (apvts.state.hasProperty (prm->getParameterID()))
            prm->setValueNotifyingHost ((float) apvts.state.getProperty (prm->getParameterID()));
    program.store (jlimit (0, getNumPrograms() - 1, (int) apvts.state.getProperty ("ltProgram", 0)));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LeitnerProcessor(); }
