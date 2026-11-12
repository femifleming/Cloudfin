#pragma once
#include "../../Source/PluginProcessor.h"
#include "dsp/Spatial.h"

//==============================================================================
// Leitner: Cloudfin in Ambisonics. It is Cloudfin's processor (the seven effects, the drift, the
// presets) with a different output and one more stage: the stereo it makes is sent through the room
// as two lines of sound (see dsp/Spatial.h) and encoded, ACN/SN3D, 3rd to 7th order.
// It is an encoder: route its channels to a decoder of the same order.
class LeitnerProcessor : public CloudfinProcessor
{
public:
    LeitnerProcessor();

    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    const juce::String getName() const override { return "Leitner"; }
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Leitner's programs: its own (a space, with one of Cloudfin's sound programs under it), then
    // Cloudfin's sound programs alone, which leave the space as it is.
    int getNumPrograms() override;
    int getCurrentProgram() override { return program.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    juce::String getProgramGroup (int);
    static int numSpacePrograms();
    juce::String presetExtension() const override { return ".leitner"; }

    // the space's own parameters (added beside Cloudfin's)
    juce::AudioParameterChoice* order = nullptr;
    juce::AudioParameterChoice* figure = nullptr;
    juce::AudioParameterChoice *sieve = nullptr, *cell = nullptr, *jux = nullptr;
    juce::AudioParameterFloat *seconds = nullptr, *rest = nullptr, *size = nullptr, *height = nullptr, *turn = nullptr, *apart = nullptr,
                              *counter = nullptr, *legato = nullptr, *swell = nullptr, *lift = nullptr, *chance = nullptr, *walk = nullptr, *scatter = nullptr,
                              *pulse = nullptr, *pulseRate = nullptr, *pulseBend = nullptr, *pulseWidth = nullptr, *drop = nullptr, *room = nullptr, *roomDecay = nullptr, *level = nullptr;
    juce::AudioParameterInt *points = nullptr, *rotate = nullptr, *hits = nullptr, *steps = nullptr;
    juce::AudioParameterBool* hold = nullptr;
    std::vector<juce::RangedAudioParameter*> spaceParams;   // all of them, in order

    leitner::Params readSpace() const;
    leitner::Space space;
    std::atomic<int> orderNow { 3 }, channelsNow { 16 };   // what's actually coming out (a track with too few channels gets a lower order)
    std::atomic<float> outLevel { 0.0f };
    std::atomic<int> program { 0 };
    std::atomic<float> viewYaw { 0.0f }, viewPitch { 0.0f };   // how the drawing of the room is turned (kept while the window is closed)

private:
    static BusesProperties buses();
    juce::AudioBuffer<float> stereo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeitnerProcessor)
};
