#include "PluginProcessor.h"
#ifndef CLOUDFIN_AS_BASE   // (defined when another plug-in builds on this processor and brings its own editor)
 #include "PluginEditor.h"
#endif
#include <BinaryData.h>

using namespace juce;
using EP = fp::EngineParams;

//==============================================================================
const std::vector<ModTarget>& modTargets()
{
    static const std::vector<ModTarget> t {
        { ids::sgCutoff,   &EP::sgCutoffDb },  { ids::sgAttack,  &EP::sgAttackMs }, { ids::sgSmear,   &EP::sgSmearS },
        { ids::sgKnee,     &EP::sgKnee },      { ids::sgBlur,    &EP::sgBlur },     { ids::sgScatter, &EP::sgScatter },
        { ids::sgTilt,     &EP::sgTilt },      { ids::sgMix,     &EP::sgMix },
        { ids::psShift,    &EP::psShiftSemis },{ ids::psGrain,   &EP::psGrainMs },  { ids::psFeedback, &EP::psFeedback },
        { ids::psMix,      &EP::psMix },
        { ids::fmVowel,    &EP::fmVowel },     { ids::fmShift,   &EP::fmShift },    { ids::fmRes,     &EP::fmRes },
        { ids::fmMix,      &EP::fmMix },
        { ids::grSize,     &EP::grSizeMs },    { ids::grDensity, &EP::grDensity },  { ids::grPosition, &EP::grPosition },
        { ids::grSpray,    &EP::grSpray },     { ids::grWidth,   &EP::grWidth },    { ids::grReverse, &EP::grReverse },
        { ids::grOctave,   &EP::grOctave },
        { ids::grFade,     &EP::grFade },      { ids::grMix,     &EP::grMix },
        { ids::dlTime,     &EP::dlTimeMs },    { ids::dlFeedback, &EP::dlFeedback },{ ids::dlTone,    &EP::dlTone },
        { ids::dlMix,      &EP::dlMix },
        { ids::rvDecay,    &EP::rvDecay },     { ids::rvTone,    &EP::rvTone },     { ids::rvMix,     &EP::rvMix },
        { ids::cpThresh,   &EP::cpThreshDb },  { ids::cpRatio,   &EP::cpRatio },    { ids::cpBelow,   &EP::cpBelow },
        { ids::cpAttack,   &EP::cpAttackMs },  { ids::cpRelease, &EP::cpReleaseMs },{ ids::cpMakeup,  &EP::cpMakeupDb },
        { ids::cpMix,      &EP::cpMix },
        { ids::masterFb,   &EP::masterFb },
    };
    return t;
}

int modIndexOf (const String& id)
{
    const auto& t = modTargets();
    for (int i = 0; i < (int) t.size(); ++i)
        if (id == t[(size_t) i].id) return i;
    return -1;
}

//==============================================================================
namespace
{
    String pct (float v, int)  { return String (roundToInt (v * 100.0f)) + " %"; }
    String ms (float v, int)   { return v < 1000.0f ? String (v, v < 10.0f ? 1 : 0) + " ms" : String (v * 0.001f, 2) + " s"; }
    String secs (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, v < 10.0f ? 2 : 1) + " s"; }
    String hz (float v, int)   { return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v * 0.001f, 1) + " kHz"; }
    String dbs (float v, int)  { return String (v, 1) + " dB"; }
    String semis (float v, int){ return (v > 0 ? "+" : "") + String (v, 1) + " st"; }

    NormalisableRange<float> logRange (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
}

AudioProcessorValueTreeState::ParameterLayout CloudfinProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    using F = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;

    auto add = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                    std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<F> (ParameterID { id, 1 }, name, range, def, A().withStringFromValueFunction (std::move (fmt))));
    };
    auto unit = [&] (const char* id, const char* name, float def) { add (id, name, { 0.0f, 1.0f }, def, pct); };

    // Spectral gate / smear
    add (ids::sgCutoff,  "Gate Cutoff",  { -60.0f, 0.0f }, -24.0f, dbs);
    add (ids::sgAttack,  "Gate Attack",  logRange (1.0f, 500.0f, 40.0f), 45.0f, ms);
    add (ids::sgSmear,   "Gate Smear",   logRange (0.05f, 20.0f, 1.5f), 1.8f, secs);
    add (ids::sgKnee,    "Gate Knee",    { 1.0f, 8.0f }, 4.0f, [] (float v, int) { return String (v, 1) + " : 1"; });
    unit (ids::sgBlur,    "Gate Blur", 0.0f);
    unit (ids::sgScatter, "Gate Scatter", 0.0f);
    add (ids::sgTilt,    "Gate Tilt",    { -6.0f, 6.0f }, 0.0f, [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB/oct"; });
    unit (ids::sgMix,     "Gate Mix", 0.35f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::sgFreeze, 1 }, "Spectral Freeze", false));

    // Pitch shifter
    StringArray scaleNames;
    for (auto& s : fp::getScales()) scaleNames.add (s.name);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::psScale, 1 }, "Scale", scaleNames, 0));
    add (ids::psShift,    "Pitch Shift",    { -24.0f, 24.0f }, 7.0f, semis);
    add (ids::psGrain,    "Pitch Grain",    logRange (4.0f, 400.0f, 60.0f), 65.0f, ms);
    add (ids::psFeedback, "Pitch Feedback", { 0.0f, 0.95f }, 0.35f, pct);
    unit (ids::psMix,     "Pitch Mix", 0.3f);

    // Formant
    add (ids::fmVowel, "Formant Vowel", { 0.0f, 4.0f }, 0.0f, [] (float v, int)
    {
        const int i = jlimit (0, 4, (int) v);
        const float t = v - (float) i;
        if (t < 0.05f || i == 4) return String (fp::FormantFilter::vowelName (i));
        if (t > 0.95f) return String (fp::FormantFilter::vowelName (i + 1));
        return String (fp::FormantFilter::vowelName (i)) + String (CharPointer_UTF8 (" \xe2\x86\x92 ")) + String (fp::FormantFilter::vowelName (i + 1));
    });
    add (ids::fmShift, "Formant Shift",     { -12.0f, 12.0f }, 0.0f, semis);
    add (ids::fmRes,   "Formant Resonance", logRange (0.25f, 4.0f, 1.0f), 1.0f, [] (float v, int) { return "x" + String (v, 2); });
    unit (ids::fmMix,  "Formant Mix", 0.2f);

    // Grains
    add (ids::grSize,     "Grain Size",     logRange (10.0f, 1500.0f, 150.0f), 90.0f, ms);
    add (ids::grDensity,  "Grain Density",  logRange (1.0f, 24.0f, 6.0f), 7.0f, [] (float v, int) { return String (v, 1) + "x"; });
    unit (ids::grPosition,"Grain Position", 0.25f);
    unit (ids::grSpray,   "Grain Spray", 0.1f);
    unit (ids::grWidth,   "Grain Random Pan", 0.45f);
    unit (ids::grReverse, "Grain Reverse", 0.0f);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::grOctave, 1 }, "Grain Octave", -2, 2, 0,
                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                         { return (v > 0 ? "+" : "") + String (v) + " oct"; })));
    add (ids::grFade, "Freeze Fade", NormalisableRange<float> (0.01f, 10.0f, 0.0f, 0.35f), 1.0f, secs);
    unit (ids::grMix, "Grain Mix", 0.4f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::grRandOct, 1 }, "Random Octave", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::grFreeze, 1 }, "Freeze", false));

    // Stereo JI delay
    add (ids::dlTime,     "Delay Time",     logRange (20.0f, 2000.0f, 350.0f), 380.0f, ms);
    add (ids::dlFeedback, "Delay Feedback", { 0.0f, 0.95f }, 0.35f, pct);
    add (ids::dlTone,     "Delay Tone",     logRange (300.0f, 18000.0f, 3000.0f), 5000.0f, hz);
    unit (ids::dlMix,     "Delay Mix", 0.2f);

    // Reverb
    add (ids::rvDecay, "Reverb Decay", NormalisableRange<float> (0.2f, 20.0f, 0.0f, 0.4f), 4.0f,
         [] (float v, int) { return String (v, 1) + " s"; });
    add (ids::rvTone, "Reverb Tone", { -1.0f, 1.0f }, 0.0f, [] (float v, int)
    {
        auto hz = [] (float f) { return f < 1000.0f ? String (roundToInt (f)) : String (f * 0.001f, 1) + "k"; };
        if (std::abs (v) < 0.01f) return String ("flat");
        return v < 0 ? "lp " + hz (fp::FDNReverb::lowpassHz (v)) : "hp " + hz (fp::FDNReverb::highpassHz (v));
    });
    unit (ids::rvMix, "Reverb Mix", 0.3f);

    // Compander
    add (ids::cpThresh,  "Comp Threshold", { -60.0f, 0.0f }, -18.0f, dbs);
    add (ids::cpRatio,   "Comp Ratio",     logRange (1.0f, 20.0f, 4.0f), 2.0f, [] (float v, int) { return String (v, 1) + " : 1"; });
    add (ids::cpBelow,   "Comp Below",     { -1.0f, 1.0f }, 0.0f, [] (float v, int)
    {
        if (std::abs (v) < 0.005f) return String ("off");
        return (v < 0 ? String ("lift ") : String ("sink ")) + String (roundToInt (std::abs (v) * 100.0f)) + " %";
    });
    add (ids::cpAttack,  "Comp Attack",  logRange (0.5f, 200.0f, 15.0f), 10.0f, ms);
    add (ids::cpRelease, "Comp Release", logRange (10.0f, 2000.0f, 250.0f), 250.0f, ms);
    add (ids::cpMakeup,  "Comp Makeup",  { -12.0f, 18.0f }, 0.0f, dbs);
    unit (ids::cpMix,    "Comp Mix", 1.0f);

    // Main feedback
    unit (ids::masterFb, "Main Feedback", 0.0f);

    // the gravity toy, and the hydrophone texture
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::gravity, 1 }, "Gravity", false));
    unit (ids::hydroLevel, "Hydrophone", 0.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::hydroInto, 1 }, "Hydrophone Into", StringArray { "output", "input" }, 0));

    // Random sources
    add (ids::modRate, "Drift Rate", logRange (0.02f, 12.0f, 0.5f), 0.35f,
         [] (float v, int) { return v < 1.0f ? String (v, 2) + " Hz" : String (v, 1) + " Hz"; });
    unit (ids::modSlew, "Drift Slew", 0.7f);

    // Attenuverters: -100 % .. +100 % of the parameter's range per unit of random voltage
    for (auto& m : modTargets())
    {
        float def = 0.0f;
        const String id (m.id);
        if (id == ids::grSize) def = 0.35f;       // grains keep their old wandering by default
        if (id == ids::grPosition) def = 0.4f;
        layout.add (std::make_unique<F> (ParameterID { ids::att (m.id), 1 }, String (m.id) + " Random",
                                         NormalisableRange<float> (-1.0f, 1.0f), def,
                                         A().withStringFromValueFunction ([] (float v, int)
                                         {
                                             if (std::abs (v) < 0.005f) return String ("0");
                                             return (v > 0 ? String ("+") : String (CharPointer_UTF8 ("\xe2\x88\x92")))
                                                    + String (roundToInt (std::abs (v) * 100.0f)) + " %";
                                         })));
    }
    return layout;
}

//==============================================================================
CloudfinProcessor::CloudfinProcessor()
    : CloudfinProcessor (BusesProperties()
                             .withInput  ("Input",  AudioChannelSet::stereo(), true)
                             .withOutput ("Output", AudioChannelSet::stereo(), true))
{
}

CloudfinProcessor::CloudfinProcessor (const BusesProperties& buses)
    : AudioProcessor (buses),
      apvts (*this, nullptr, "Cloudfin", createLayout())
{
    const auto& t = modTargets();
    mods.resize (t.size());
    for (size_t i = 0; i < t.size(); ++i)
    {
        mods[i].param = apvts.getParameter (t[i].id);
        mods[i].base  = apvts.getRawParameterValue (t[i].id);
        mods[i].att   = apvts.getRawParameterValue (ids::att (t[i].id));
        mods[i].walk.seed (0xF00D5EEDull + 0x9E3779B97F4A7C15ull * (uint64_t) (i + 1));
        jassert (mods[i].param != nullptr && mods[i].att != nullptr);
        modNorm.push_back (std::make_unique<std::atomic<float>> (mods[i].param->getValue()));
    }
    p_psScale   = apvts.getRawParameterValue (ids::psScale);
    p_sgFreeze  = apvts.getRawParameterValue (ids::sgFreeze);
    p_grRandOct = apvts.getRawParameterValue (ids::grRandOct);
    p_grFreeze  = apvts.getRawParameterValue (ids::grFreeze);
    p_modRate   = apvts.getRawParameterValue (ids::modRate);
    p_modSlew   = apvts.getRawParameterValue (ids::modSlew);
    p_gravity = apvts.getRawParameterValue (ids::gravity);
    p_masterFb = apvts.getRawParameterValue (ids::masterFb);
    p_hydroLevel = apvts.getRawParameterValue (ids::hydroLevel);
    p_hydroInto = apvts.getRawParameterValue (ids::hydroInto);

    fp::GrainShifter::makeShape (fp::GrainShifter::saw, shapeShared.data());
}

bool CloudfinProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

//==============================================================================
uint32_t CloudfinProcessor::packOrder (const fp::StageOrder& o) noexcept
{
    uint32_t v = 0;
    for (int i = 0; i < fp::numStages; ++i) v |= (uint32_t) o[(size_t) i] << (3 * i);
    return v;
}

fp::StageOrder CloudfinProcessor::unpackOrder (uint32_t v) noexcept
{
    fp::StageOrder o {};
    for (int i = 0; i < fp::numStages; ++i) o[(size_t) i] = (int) ((v >> (3 * i)) & 7u);
    return fp::isValidOrder (o) ? o : fp::defaultOrder();
}

void CloudfinProcessor::setOrder (const fp::StageOrder& o)
{
    if (fp::isValidOrder (o)) orderPacked.store (packOrder (o));
}

fp::StageOrder CloudfinProcessor::getOrder() const noexcept { return unpackOrder (orderPacked.load()); }

//==============================================================================
void CloudfinProcessor::setGrainShape (const float* table)
{
    {
        const SpinLock::ScopedLockType sl (shapeLock);
        for (int i = 0; i < kShapeN; ++i) shapeShared[(size_t) i] = jlimit (0.0f, 1.0f, table[i]);
    }
    ++shapeVersion;
}

void CloudfinProcessor::getGrainShape (float* table) const
{
    const SpinLock::ScopedLockType sl (shapeLock);
    std::copy (shapeShared.begin(), shapeShared.end(), table);
}

void CloudfinProcessor::pullGrainShape() noexcept
{
    const uint32_t v = shapeVersion.load();
    if (v == shapeSeen) return;
    const SpinLock::ScopedTryLockType tl (shapeLock); // never wait on the audio thread
    if (! tl.isLocked()) return;
    engine.setGrainShape (shapeShared.data());
    shapeSeen = v;
}

//==============================================================================
void CloudfinProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    hydro.prepare (sr);
    slosh.prepare (sr);
    limiter.prepare (sr);
    inScratch.setSize (2, jmax (1, samplesPerBlock), false, true, true);
    shapeSeen = 0;
    pullGrainShape();

    // seed the engine with the un-modulated values so nothing ramps in from defaults
    fp::EngineParams p;
    for (size_t i = 0; i < mods.size(); ++i)
        p.*(modTargets()[i].field) = mods[i].base->load();
    p.psScale   = (int) p_psScale->load();
    p.grRandOct = p_grRandOct->load() > 0.5f;
    p.grFreeze  = p_grFreeze->load() > 0.5f;
    p.sgFreeze  = p_sgFreeze->load() > 0.5f;
    engine.setParams (p);
    engine.setOrder (getOrder());
    engine.prepare (sr);
    scratch.setSize (2, std::max (1, samplesPerBlock), false, true, true);
    setLatencySamples (0);
}

void CloudfinProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (n == 0) return;

    if (scratch.getNumSamples() < n) // hosts shouldn't do this, but stay safe
        scratch.setSize (2, n, false, true, true);

    pullGrainShape();
    engine.setOrder (getOrder());

    // the input, plus the hydrophone if it's going in there
    if (inScratch.getNumSamples() < n) inScratch.setSize (2, n, false, true, true);
    inScratch.copyFrom (0, 0, buffer, 0, 0, n);
    inScratch.copyFrom (1, 0, buffer, numIn > 1 ? 1 : 0, 0, n);
    const bool hydroIn = p_hydroInto->load() > 0.5f;

    // the main loop catches its own hiss, like analogue feedback: while it's turned up and everything's
    // gone quiet, a whisper of noise (~ -70 dBFS) goes in so it can start ringing even with nothing playing.
    // Once it's ringing (or with the feedback down) nothing is added.
    {
        const float fb = p_masterFb->load();
        if (fb > 0.02f && lastOutPeak < 1.0e-3f)
        {
            const float amp = 3.0e-4f * std::min (1.0f, fb * 2.0f);
            for (int c = 0; c < 2; ++c)
            {
                float* d = inScratch.getWritePointer (c);
                for (int i = 0; i < n; ++i) d[i] += amp * seedNoise.bi();
            }
        }
    }
    hydro.addTo (inScratch.getWritePointer (0), inScratch.getWritePointer (1), n, hydroIn ? p_hydroLevel->load() : 0.0f);
    const float* inL = inScratch.getReadPointer (0);
    const float* inR = inScratch.getReadPointer (1);
    float* oL = scratch.getWritePointer (0);
    float* oR = scratch.getWritePointer (1);

    const float rate = p_modRate->load(), slew = p_modSlew->load();
    const auto& targets = modTargets();

    // control rate: every 32 samples the random voltages move and the params are re-read
    constexpr int kChunk = 32;
    for (int off = 0; off < n; off += kChunk)
    {
        const int len = std::min (kChunk, n - off);
        fp::EngineParams p;
        for (size_t i = 0; i < mods.size(); ++i)
        {
            auto& m = mods[i];
            const float v = m.walk.advance (len, sampleRate, rate, slew);
            const float a = m.att->load();
            float norm = m.param->convertTo0to1 (m.base->load());
            if (a != 0.0f) norm = jlimit (0.0f, 1.0f, norm + a * v);
            modNorm[i]->store (norm, std::memory_order_relaxed);
            p.*(targets[i].field) = m.param->convertFrom0to1 (norm);
        }
        p.psScale   = (int) p_psScale->load();
        p.grRandOct = p_grRandOct->load() > 0.5f;
        p.grFreeze  = p_grFreeze->load() > 0.5f;
        p.sgFreeze  = p_sgFreeze->load() > 0.5f;
        engine.setParams (p);
        engine.process (inL + off, inR + off, oL + off, oR + off, len);
    }

    // on the master: the hydrophone (if it's going there), then the slosh while gravity's on
    if (! hydroIn) hydro.addTo (oL, oR, n, p_hydroLevel->load());
    slosh.process (oL, oR, n, p_gravity->load() > 0.5f, sloshEnergy.load (std::memory_order_relaxed));
    sloshAmount.store (slosh.amount());

    // never clips: a peak limiter with its ceiling at -1 dBFS is the very last stage
    limiter.process (oL, oR, n);

    if (numOut > 1)
    {
        buffer.copyFrom (0, 0, oL, n);
        buffer.copyFrom (1, 0, oR, n);
    }
    else
    {
        buffer.copyFrom (0, 0, oL, n, 0.5f);
        buffer.addFrom  (0, 0, oR, n, 0.5f);
    }
    lastOutPeak = jmax (buffer.getMagnitude (0, 0, n), numOut > 1 ? buffer.getMagnitude (1, 0, n) : 0.0f);
    freezeAmount.store (engine.getFreezeAmount());
    compGainDb.store (engine.getCompGainDb());
    spectralFreezeAmount.store (engine.getSpectralFreezeAmount());
}

#ifndef CLOUDFIN_AS_BASE
AudioProcessorEditor* CloudfinProcessor::createEditor() { return new CloudfinEditor (*this); }
#else
AudioProcessorEditor* CloudfinProcessor::createEditor() { return nullptr; }
#endif

//==============================================================================

//==============================================================================
// The hydrophone recording, decoded in the background the first time a Cloudfin opens
fp::HydrophoneData::HydrophoneData()
{
    loader = std::thread ([this]
    {
        OggVorbisAudioFormat ogg;
        std::unique_ptr<AudioFormatReader> r (ogg.createReaderFor (new MemoryInputStream (BinaryData::hydrophone_ogg, BinaryData::hydrophone_oggSize, false), true));
        if (r == nullptr || r->lengthInSamples < 1000) return;
        audio.setSize ((int) jmax (1u, r->numChannels), (int) r->lengthInSamples);
        r->read (&audio, 0, (int) r->lengthInSamples, 0, true, true);
        fileRate = r->sampleRate;
        ready.store (true, std::memory_order_release);
    });
}

//==============================================================================
// Presets. Each starts from the defaults (gravity is left as it is), then sets its own values.
namespace
{
    struct PV { String id; float v; };
    struct CPreset { const char* name; const char* group; const char* scale; std::vector<PV> values; };

    const std::vector<CPreset>& cloudfinPresets()
    {
        static const std::vector<CPreset> l {
            { "Cloudfin", "start", nullptr, {} },
            // ---- feedback: the loop pushed until it sings on its own
            { "Endless Slide", "feedback", "The Well-Tuned Piano (La Monte Young)",
              { { ids::masterFb, 0.9f }, { ids::psShift, 7.0f }, { ids::psFeedback, 0.6f }, { ids::psMix, 0.4f },
                { ids::rvDecay, 10.0f }, { ids::rvMix, 0.45f }, { ids::dlMix, 0.3f }, { ids::dlFeedback, 0.45f } } },
            { "Bloom Loop", "feedback", nullptr,
              { { ids::masterFb, 0.8f }, { ids::cpBelow, -0.35f }, { ids::cpRatio, 4.0f }, { ids::cpMakeup, -6.0f }, { ids::fmMix, 0.4f }, { ids::fmVowel, 2.0f },
                { ids::rvMix, 0.5f }, { ids::rvDecay, 14.0f }, { ids::psMix, 0.2f } } },
            { "Harmonic Fountain", "feedback", "Harmonic Series 8-16",
              { { ids::masterFb, 0.85f }, { ids::psShift, 12.0f }, { ids::psFeedback, 0.8f }, { ids::psMix, 0.55f },
                { ids::dlMix, 0.25f }, { ids::rvDecay, 8.0f }, { ids::rvMix, 0.4f } } },
            { "Tritave Choir", "feedback", "Bohlen-Pierce (tritave)",
              { { ids::masterFb, 0.88f }, { ids::psShift, 19.0f }, { ids::psFeedback, 0.5f }, { ids::psMix, 0.4f },
                { ids::fmMix, 0.6f }, { ids::fmVowel, 3.0f }, { ids::rvMix, 0.5f }, { ids::rvDecay, 9.0f } } },
            // ---- stretch: long, sparse grains
            { "Slow Tide", "stretch", nullptr,
              { { ids::grSize, 1500.0f }, { ids::grDensity, 1.5f }, { ids::grMix, 1.0f }, { ids::grSpray, 0.5f }, { ids::grWidth, 0.8f },
                { ids::sgMix, 0.5f }, { ids::sgSmear, 6.0f }, { ids::rvDecay, 12.0f }, { ids::rvMix, 0.4f }, { ids::psMix, 0.0f } } },
            { "Glass Stretch", "stretch", nullptr,
              { { ids::grSize, 1200.0f }, { ids::grDensity, 2.0f }, { ids::grMix, 0.95f }, { ids::grPosition, 0.6f }, { ids::grReverse, 0.3f },
                { ids::sgMix, 0.6f }, { ids::sgSmear, 10.0f }, { ids::sgBlur, 0.4f }, { ids::rvMix, 0.45f }, { ids::psMix, 0.0f } } },
            { "Deep Hydrophone", "stretch", nullptr,
              { { ids::grSize, 1000.0f }, { ids::grDensity, 1.8f }, { ids::grOctave, -1.0f }, { ids::grMix, 0.9f },
                { ids::hydroLevel, 0.35f }, { ids::hydroInto, 1.0f }, { ids::rvDecay, 16.0f }, { ids::rvMix, 0.5f }, { ids::sgMix, 0.4f },
                { ids::psMix, 0.0f } } },
            // ---- useful
            { "Clear Water", "useful", nullptr,
              { { ids::sgMix, 0.15f }, { ids::psMix, 0.1f }, { ids::rvMix, 0.2f }, { ids::rvDecay, 3.0f }, { ids::dlMix, 0.15f } } },
            { "Pelog Echoes", "useful", "Pelog",
              { { ids::dlMix, 0.45f }, { ids::dlFeedback, 0.6f }, { ids::dlTime, 420.0f }, { ids::psMix, 0.25f }, { ids::psShift, 5.0f },
                { ids::rvMix, 0.3f } } },
            { "Yaman Shimmer", "useful", "Raga Yaman",
              { { ids::psShift, 12.0f }, { ids::psFeedback, 0.55f }, { ids::psMix, 0.35f }, { ids::rvDecay, 9.0f }, { ids::rvMix, 0.45f } } },
            { "Vowel Drift", "useful", nullptr,
              { { ids::fmMix, 0.7f }, { ids::fmRes, 2.5f }, { ids::fmVowel, 2.0f }, { ids::att (ids::fmVowel), 0.6f }, { ids::att (ids::fmShift), 0.3f },
                { ids::modRate, 0.2f }, { ids::modSlew, 0.9f }, { ids::rvMix, 0.3f } } },
            { "Spectral Snow", "useful", nullptr,
              { { ids::sgMix, 0.8f }, { ids::sgCutoff, -40.0f }, { ids::sgSmear, 8.0f }, { ids::sgBlur, 0.5f }, { ids::sgScatter, 0.4f },
                { ids::rvDecay, 10.0f }, { ids::rvMix, 0.35f } } },
            { "Underwater Room", "useful", nullptr,
              { { ids::hydroLevel, 0.5f }, { ids::hydroInto, 0.0f }, { ids::cpThresh, -30.0f }, { ids::cpRatio, 3.0f }, { ids::rvTone, -0.6f },
                { ids::rvDecay, 6.0f }, { ids::rvMix, 0.4f }, { ids::dlTone, 1500.0f }, { ids::dlMix, 0.2f } } },
            { "Maqam Mist", "useful", "Maqam Bayati",
              { { ids::psShift, -5.0f }, { ids::psMix, 0.3f }, { ids::psFeedback, 0.45f }, { ids::grMix, 0.5f }, { ids::grSize, 400.0f },
                { ids::rvMix, 0.4f } } },
        };
        return l;
    }
}

// a random patch: every sound control somewhere new (gravity and the hydrophone are left alone,
// the main feedback stays below 0.9 and the makeup gain within +-6 dB, the drift depths stay gentle)
void CloudfinProcessor::randomize()
{
    Random r;
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<RangedAudioParameter*> (prm);
        if (rp == nullptr) continue;
        const String id = rp->getParameterID();
        if (id == ids::gravity || id == ids::hydroLevel || id == ids::hydroInto || id == ids::sgFreeze || id == ids::grFreeze) continue;
        float norm = r.nextFloat();
        if (id.endsWith ("_att"))  norm = 0.5f + (r.nextFloat() - 0.5f) * 0.5f;    // -25 % .. +25 %
        if (id == ids::masterFb)   norm = r.nextFloat() * 0.9f;
        if (id == ids::cpMakeup)   norm = rp->convertTo0to1 (-6.0f + 12.0f * r.nextFloat());
        rp->setValueNotifyingHost (norm);
    }
}

int CloudfinProcessor::getNumPrograms() { return (int) cloudfinPresets().size(); }
const String CloudfinProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, getNumPrograms()) ? String (cloudfinPresets()[(size_t) i].name) : String();
}
String CloudfinProcessor::getProgramGroup (int i)
{
    return isPositiveAndBelow (i, getNumPrograms()) ? String (cloudfinPresets()[(size_t) i].group) : String();
}

void CloudfinProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, getNumPrograms())) return;
    const auto& pr = cloudfinPresets()[(size_t) index];
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            if (r->getParameterID() != ids::gravity)
                r->setValueNotifyingHost (r->getDefaultValue());
    auto set = [this] (const String& id, float real)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (real));
    };
    if (pr.scale != nullptr)
        for (int i = 0; i < (int) fp::getScales().size(); ++i)
            if (fp::getScales()[(size_t) i].name == pr.scale) set (ids::psScale, (float) i);
    for (auto& v : pr.values) set (v.id, v.v);
    currentPreset.store (index);
}

void CloudfinProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    float t[kShapeN];
    getGrainShape (t);
    String s;
    for (int i = 0; i < kShapeN; ++i) s << String (t[i], 4) << (i + 1 < kShapeN ? "," : "");
    state.setProperty ("grainShape", s, nullptr);
    state.setProperty ("order", (int) orderPacked.load(), nullptr);
    state.setProperty ("theme", themeMode.load(), nullptr);
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CloudfinProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            const auto shapeStr = tree.getProperty ("grainShape").toString();
            if (tree.hasProperty ("order"))
                orderPacked.store (packOrder (unpackOrder ((uint32_t) (int) tree.getProperty ("order"))));
            currentPreset.store ((int) tree.getProperty ("preset", 0));
            if (tree.hasProperty ("theme"))
                themeMode.store (jlimit (0, 2, (int) tree.getProperty ("theme")));
            else if (tree.hasProperty ("night"))            // sessions saved before night mode existed
                themeMode.store ((bool) tree.getProperty ("night") ? 1 : 0);
            apvts.replaceState (tree);

            float t[kShapeN];
            StringArray parts;
            parts.addTokens (shapeStr, ",", {});
            if (parts.size() == kShapeN)
                for (int i = 0; i < kShapeN; ++i) t[i] = parts[i].getFloatValue();
            else
                fp::GrainShifter::makeShape (fp::GrainShifter::saw, t);
            setGrainShape (t);
        }
}

#ifndef CLOUDFIN_AS_BASE
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CloudfinProcessor(); }
#endif
