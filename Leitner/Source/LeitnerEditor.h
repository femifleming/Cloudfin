#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "LeitnerProcessor.h"

//==============================================================================
// The window is drawn the way Bernhard Leitner drew his Soundcube sheets in 1969: typed sheets, the
// room in perspective with its loudspeakers as small circles, the sound as a heavy black line with
// arrows, a solid black figure standing in it, and typewritten captions in English and German. Black
// on paper; the second line of sound is the one red thing, after the red rods of his Plexiglas models.
//
// It is mirror symmetrical: the room in the middle, the space's controls under it in two facing
// columns, and Cloudfin's sound program on a sheet either side, the right one the left one reversed.
// Drag the room to turn it and see where things are from any side; double-click it to face front again.
//
// Every parameter is a ruled line with a tick on it: click or drag along the line to set it,
// double-click to put it back. Alt-drag a Cloudfin parameter to set its drift (how far its own random
// voltage moves it): the red stroke under the line is the drift, the red circle where it is now.
namespace lt
{
juce::Font typed (float h, bool bold = false);
extern const juce::Colour paper, ink, faint, red;

class Rule : public juce::Component
{
public:
    Rule (juce::RangedAudioParameter& main, juce::RangedAudioParameter* drift, const juce::String& name, bool mirrored);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::atomic<float>* live = nullptr;   // where the drift has it now (0..1), if it drifts
    int nameW = 66, valueW = 56;
private:
    juce::Rectangle<int> lineArea() const;
    float along (int x) const;            // 0..1 along the line, read the way it's drawn
    void setFrom (const juce::MouseEvent&);
    juce::RangedAudioParameter& param;
    juce::RangedAudioParameter* driftParam;
    juce::ParameterAttachment att;
    std::unique_ptr<juce::ParameterAttachment> driftAtt;
    juce::String name;
    bool mirrored;
    float value = 0.0f, drift = 0.0f, downAt = 0.0f, driftAtDown = 0.0f;
    int dragging = 0;   // 1 the value, 2 the drift
};

class Choice : public juce::Component
{
public:
    Choice (juce::RangedAudioParameter&, const juce::String& name, bool mirrored);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int nameW = 66;
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment att;
    juce::String name;
    bool mirrored;
};

class Check : public juce::Component
{
public:
    Check (juce::RangedAudioParameter&, const juce::String& name, bool mirrored);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment att;
    juce::String name;
    bool mirrored, on = false;
};

// one line's pulses over a pass, as a row of strokes (a stroke as long as the pulse sounds), with a
// mark for where the pass has got to. The one on the right-hand sheet runs right to left.
class PulseStrip : public juce::Component
{
public:
    PulseStrip (LeitnerProcessor& p, int whichLine, bool isMirrored) : proc (p), line (whichLine), mirrored (isMirrored) {}
    void paint (juce::Graphics&) override;
private:
    LeitnerProcessor& proc;
    int line; bool mirrored;
};

// the room, in perspective, with the two lines of sound in it. Drag to turn it.
class Room : public juce::Component
{
public:
    explicit Room (LeitnerProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    struct View { leitner::Vec3 f, r, u; };
    View view() const;
    juce::Point<float> project (const View&, leitner::Vec3, float* scale = nullptr) const;
    LeitnerProcessor& proc;
    float yawDown = 0.0f, pitchDown = 0.0f;
};
}

class LeitnerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LeitnerEditor (LeitnerProcessor&);
    ~LeitnerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int designW = 1500, designH = 790;

private:
    struct Content : juce::Component
    {
        explicit Content (LeitnerEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintSheets (g); }
        void mouseDown (const juce::MouseEvent& e) override { owner.clickOrder (e.getPosition()); }
        LeitnerEditor& owner;
    };
    struct Heading { juce::String text; juce::Rectangle<int> area; int stage; bool mirrored; };
    struct Look : juce::LookAndFeel_V4
    {
        juce::Font getTextButtonFont (juce::TextButton&, int) override;
        juce::Font getPopupMenuFont() override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    };

    void paintSheets (juce::Graphics&);
    void clickOrder (juce::Point<int>);
    void timerCallback() override;

    LeitnerProcessor& proc;
    Look look;
    Content content { *this };
    lt::Room room { proc };
    lt::PulseStrip strip1 { proc, 0, false }, strip2 { proc, 1, true };
    juce::OwnedArray<juce::Component> parts;
    std::vector<Heading> headings;
    std::vector<juce::Rectangle<int>> orderWords;
    juce::TextButton presetBtn, randomBtn, shapeBtn;
    uint32_t seenOrder = 0;
    int seenFigure = -1, seenOut = -1;
    int seenProgram = -1;
    juce::String userShown;                      // your preset's name, while it is the one loaded
    std::unique_ptr<juce::FileChooser> chooser;
    void programMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeitnerEditor)
};
