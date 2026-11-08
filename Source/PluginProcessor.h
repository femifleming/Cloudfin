#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"
#include "dsp/Hydrophone.h"
#include "dsp/Limiter.h"

namespace ids
{
    // spectral gate / smear
    inline constexpr const char* sgCutoff   = "sgCutoff";
    inline constexpr const char* sgAttack   = "sgAttack";
    inline constexpr const char* sgSmear    = "sgSmear";
    inline constexpr const char* sgKnee     = "sgKnee";
    inline constexpr const char* sgBlur     = "sgBlur";
    inline constexpr const char* sgScatter  = "sgScatter";
    inline constexpr const char* sgTilt     = "sgTilt";
    inline constexpr const char* sgMix      = "sgMix";
    inline constexpr const char* sgFreeze   = "sgFreeze";
    // pitch
    inline constexpr const char* psShift    = "psShift";
    inline constexpr const char* psGrain    = "psGrain";
    inline constexpr const char* psFeedback = "psFeedback";
    inline constexpr const char* psMix      = "psMix";
    inline constexpr const char* psScale    = "psScale";
    // formant
    inline constexpr const char* fmVowel    = "fmVowel";
    inline constexpr const char* fmShift    = "fmShift";
    inline constexpr const char* fmRes      = "fmRes";
    inline constexpr const char* fmMix      = "fmMix";
    // grains
    inline constexpr const char* grSize     = "grSize";
    inline constexpr const char* grDensity  = "grDensity";
    inline constexpr const char* grPosition = "grPosition";
    inline constexpr const char* grSpray    = "grSpray";
    inline constexpr const char* grWidth    = "grWidth";
    inline constexpr const char* grReverse  = "grReverse";
    inline constexpr const char* grOctave   = "grOctave";
    inline constexpr const char* grFade     = "grFade";
    inline constexpr const char* grMix      = "grMix";
    inline constexpr const char* grRandOct  = "grRandOct";
    inline constexpr const char* grFreeze   = "grFreeze";
    // delay
    inline constexpr const char* dlTime     = "dlTime";
    inline constexpr const char* dlFeedback = "dlFeedback";
    inline constexpr const char* dlTone     = "dlTone";
    inline constexpr const char* dlMix      = "dlMix";
    // reverb
    inline constexpr const char* rvDecay    = "rvDecay";
    inline constexpr const char* rvTone     = "rvTone";
    inline constexpr const char* rvMix      = "rvMix";
    // compander
    inline constexpr const char* cpThresh   = "cpThresh";
    inline constexpr const char* cpRatio    = "cpRatio";
    inline constexpr const char* cpBelow    = "cpBelow";
    inline constexpr const char* cpAttack   = "cpAttack";
    inline constexpr const char* cpRelease  = "cpRelease";
    inline constexpr const char* cpMakeup   = "cpMakeup";
    inline constexpr const char* cpMix      = "cpMix";
    // main loop + random sources
    inline constexpr const char* masterFb   = "masterFb";
    inline constexpr const char* modRate    = "modRate";
    inline constexpr const char* modSlew    = "modSlew";
    inline constexpr const char* gravity    = "gravity";      // the gravity toy (and the slosh it brings)
    inline constexpr const char* hydroLevel = "hydroLevel";   // the hydrophone texture
    inline constexpr const char* hydroInto  = "hydroInto";    // 0: into the master output, 1: into the input

    // every modulatable parameter has an attenuverter called "<id>_att"
    inline juce::String att (const char* id) { return juce::String (id) + "_att"; }
}

// One entry per modulatable parameter: its id and where it lands in the engine.
struct ModTarget
{
    const char* id;
    float fp::EngineParams::* field;
};
const std::vector<ModTarget>& modTargets();
int modIndexOf (const juce::String& id); // -1 if not modulatable

class CloudfinProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kShapeN = fp::GrainShifter::kShapeN;

    CloudfinProcessor();
    ~CloudfinProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cloudfin"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    juce::String getProgramGroup (int);
    void randomize();   // a random patch (message thread)
    std::atomic<int> currentPreset { 0 };
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // module order (any thread). Applied click-free by the engine.
    void setOrder (const fp::StageOrder& o);
    fp::StageOrder getOrder() const noexcept;
    static uint32_t packOrder (const fp::StageOrder& o) noexcept;
    static fp::StageOrder unpackOrder (uint32_t v) noexcept;

    // UI look, saved with the session: 0 day, 1 dusk (default), 2 night
    std::atomic<int> themeMode { 1 };

    // grain envelope (message thread). The audio thread picks it up lock-free.
    void setGrainShape (const float* table);
    void getGrainShape (float* table) const;
    uint32_t getGrainShapeVersion() const noexcept { return shapeVersion.load(); }

    juce::AudioProcessorValueTreeState apvts;

    // read by the editor
    std::atomic<float> freezeAmount { 0.0f }, compGainDb { 0.0f }, spectralFreezeAmount { 0.0f };
    std::atomic<float> sloshEnergy { 0.0f };   // how hard things are splashing (set by the editor's water)
    std::atomic<float> sloshAmount { 0.0f };   // how far the slosh has faded in (for the editor and tests)
    bool hydrophoneReady() const noexcept { return hydro.isReady(); }
    std::vector<std::unique_ptr<std::atomic<float>>> modNorm; // live (modulated) 0..1 value per mod target

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void pullGrainShape() noexcept;

    fp::Engine engine;
    juce::AudioBuffer<float> scratch, inScratch;
    fp::HydrophonePlayer hydro;
    fp::Slosh slosh;
    fp::PeakLimiter limiter;
    std::atomic<float>* p_gravity = nullptr;
    std::atomic<float>* p_masterFb = nullptr;
    float lastOutPeak = 0.0f;   // the last block's output peak (for the feedback seed)
    fp::Rng seedNoise;
    std::atomic<float>* p_hydroLevel = nullptr;
    std::atomic<float>* p_hydroInto = nullptr;
    double sampleRate = 48000.0;

    struct Mod
    {
        juce::RangedAudioParameter* param = nullptr;
        std::atomic<float>* base = nullptr;
        std::atomic<float>* att = nullptr;
        fp::SlewedRandom walk;
    };
    std::vector<Mod> mods;
    std::atomic<float>* p_psScale = nullptr;
    std::atomic<float>* p_sgFreeze = nullptr;
    std::atomic<uint32_t> orderPacked { packOrder (fp::defaultOrder()) };
    std::atomic<float>* p_grRandOct = nullptr;
    std::atomic<float>* p_grFreeze = nullptr;
    std::atomic<float>* p_modRate = nullptr;
    std::atomic<float>* p_modSlew = nullptr;

    std::array<float, kShapeN> shapeShared {};
    mutable juce::SpinLock shapeLock;
    std::atomic<uint32_t> shapeVersion { 1 };
    uint32_t shapeSeen = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloudfinProcessor)
};
