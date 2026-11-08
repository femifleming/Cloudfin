#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
// Frutiger Aero, after a recurring dream: an endless waterslide, light blue light
// shining through mostly opaque white plastic.
//   day   : bright sky, cool blue light through the tube
//   dusk  : a water-aero sunset, a little darker, warm sunlight breaking through
//           here and there (the default)
//   night : deep blue under the stars and a moon; the slide glows from inside
struct Theme
{
    int mode = 1;                   // 0 day, 1 dusk, 2 night
    bool dusk = false, dark = false;
    juce::Colour skyTop, skyMid, skyWarm, skyLow;
    juce::Colour deep, mist;        // text on the glass panels
    juce::Colour ink;               // text on light pills (chips, buttons, menus)
    juce::Colour titleShadow;
    juce::Colour ocean, aqua, aquaLight, glint, halo;   // water
    juce::Colour violet, lime, warm;
    juce::Colour tubeRim, tube[6], tubeGlowA, tubeGlowB, seam;
    juce::Colour pulseA, pulseB;    // light travelling along the slide
    juce::Colour panelTop, panelBottom, panelGlow, panelEdge, panelSlab;
    float gloss = 0.6f;
    juce::Colour rimA, rimB, rimC;  // soap-film iridescence
    juce::Colour wordA, wordB, wordC, wordExtrude;
    juce::Colour pillTop, pillBottom;

    static const Theme& dayTheme();
    static const Theme& duskTheme();
    static const Theme& nightTheme();
    static const Theme& forMode (int m);
};

// colour per module, shared by the order strip chips and the panel badges
juce::Colour stageColour (int stage);
const char* stageName (int stage);

//==============================================================================
class AeroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AeroLookAndFeel();
    void setTheme (const Theme&);
    const Theme& theme() const noexcept { return *th; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;

    static juce::Font uiFont (float h, bool bold = false);
    static juce::Font titleFont (float h);

    // a glossy soap/water bubble, filled to 'level' (0..1) with sloshing water
    void drawWaterBubble (juce::Graphics&, juce::Rectangle<float> r, float level, float phase,
                          bool hot, float ghostLevel = -1.0f) const;
    void drawGlass (juce::Graphics&, juce::Rectangle<float> r, bool hot) const;
    void drawGloss (juce::Graphics&, juce::Rectangle<float> r) const;

    float time = 0.0f; // animation clock, advanced by the editor

private:
    const Theme* th = &Theme::duskTheme();
};

//==============================================================================
// A knob that floats: name, a water-filled bubble, its value, and (optionally)
// the attenuverter bubble that scales the random voltage into it.
struct FloatKnob : public juce::Component
{
    static inline float cacheScale = 1.0f;   // the window's pixel scale (set by the editor as it paints)
    static inline bool justMoving = false;
    static inline bool fastBlit = false;     // while they tumble under gravity: draw their pictures without smoothing   // set while the editor moves a knob: moving it doesn't change its picture
    FloatKnob (CloudfinProcessor&, const char* paramId, const juce::String& title, bool withAttenuverter, juce::Component* popupParent);
    void resized() override;
    void animate (float t, float dt);   // updates bob offset + modulation ghost
    void applyTheme (const Theme&);

    CloudfinProcessor& proc;
    juce::Label name;
    juce::Slider main, att;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mainAtt, attAtt;
    int modIndex = -1;
    bool hasAtt = true;
    float ph1 = 0, ph2 = 0, ph3 = 0, settle = 1.0f, lastMod = -1.0f;
    int frame = 0;   // it's cached as a picture; the water in it is redrawn every third frame (or right away when touched)
    juce::Point<float> bob;
};

struct BubbleToggle : public juce::ToggleButton
{
    BubbleToggle (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& text);
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

//==============================================================================
// Draw the grain envelope with the mouse. Presets underneath.
class ShapeEditor : public juce::Component
{
public:
    explicit ShapeEditor (CloudfinProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void poll (float t, float grainSeconds);

private:
    void drawAt (juce::Point<float>);
    void push();
    void applyPreset (int which);

    CloudfinProcessor& proc;
    std::array<float, CloudfinProcessor::kShapeN> table {};
    juce::Rectangle<float> plot;
    juce::Point<float> last;
    bool drawing = false;
    uint32_t seenVersion = 0;
    float time = 0.0f, riderPos = 0.0f;
    juce::OwnedArray<juce::TextButton> presets;
};

//==============================================================================
// The module order, as a row of chips. Drag a chip to move that module in the
// signal chain; double-click the strip to go back to the default order.
class ChainStrip : public juce::Component
{
public:
    explicit ChainStrip (CloudfinProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void poll (float dt);
    void shuffle();

    static constexpr float chipW = 84.0f, gap = 20.0f;

private:
    float slotX (int i) const noexcept { return (float) i * (chipW + gap); }
    int stageAt (juce::Point<float>) const;

    CloudfinProcessor& proc;
    fp::StageOrder order = fp::defaultOrder();
    float chipX[fp::numStages] {};
    int dragStage = -1;
    float dragX = 0.0f, grabOffset = 0.0f;
    juce::Random rnd;
};

//==============================================================================
// The hydrophone: a little water-filled capsule (drag sideways for the level, double-click
// for silence) and a pill that says where it goes: into the input, or onto the output.
class HydroControl : public juce::Component
{
public:
    explicit HydroControl (CloudfinProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    const Theme* theme = nullptr;
    float t = 0.0f;
private:
    juce::Rectangle<float> capsule() const;
    juce::Rectangle<float> pill() const;
    CloudfinProcessor& proc;
    juce::ParameterAttachment levelAtt, intoAtt;
    float level = 0.0f, into = 0.0f;
    bool dragging = false;
};

//==============================================================================
// Day / dusk / night: a little pill with a sun, a setting sun and a moon.
class ThemeSwitch : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setMode (int m, bool animate);
    void poll (float dt);
    std::function<void (int)> onChange;
    int mode = 1;

private:
    float pos = 0.5f; // 0 day .. 0.5 dusk .. 1 night
};

//==============================================================================
class CloudfinEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CloudfinEditor (CloudfinProcessor&);
    ~CloudfinEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 1500, designH = 790;

private:
    struct Content : public juce::Component
    {
        explicit Content (CloudfinEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        void paintOverChildren (juce::Graphics& g) override { owner.paintForeground (g); }
        CloudfinEditor& owner;
    };

    struct Panel
    {
        juce::Rectangle<int> r;
        juce::String title, sub;
        std::vector<FloatKnob*> knobs;
        int stage = -1; // which module, for the order badge
    };

    struct AmbientBubble { float x, speed, size, phase, offset; };

    // gravity: every control is a rigid disc that can fall, roll and pile up
    struct Body
    {
        juce::Component* comp = nullptr;
        FloatKnob* knob = nullptr;
        juce::Point<float> home, pos, vel;
        float r = 40.0f, angle = 0.0f, spin = 0.0f;
    };

    FloatKnob* knob (const char* id, const juce::String& title, bool withAtt = true);
    void paintContent (juce::Graphics&);
    void layoutContent();
    void timerCallback() override;
    void buildTube();
    void renderStatic (float scale);
    void drawBackScene (juce::Graphics&);
    void drawFrontScene (juce::Graphics&, bool panelsPart);   // the wordmark, or the glass frames
    void drawPanelGlass (juce::Graphics&, const Panel&);
    void drawTubeLights (juce::Graphics&, float alpha);
    void drawAmbient (juce::Graphics&);
    void drawStars (juce::Graphics&);
    void drawBokehLayer (juce::Graphics&, bool foreground);
    void paintForeground (juce::Graphics&);
    void drawDynamic (juce::Graphics&);
    float modValue (const char* id) const;
    void setThemeMode (int mode);
    void setGravity (bool on);
    void stepPhysics (float dt);

    CloudfinProcessor& proc;
    AeroLookAndFeel lnf;
    Content content { *this };

    juce::OwnedArray<FloatKnob> knobs;
    std::vector<Panel> panels;
    Panel *sgP = nullptr, *psP = nullptr, *fmP = nullptr, *grP = nullptr, *dlP = nullptr,
          *rvP = nullptr, *cpP = nullptr, *fbP = nullptr, *drP = nullptr;

    juce::ComboBox scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAtt;
    BubbleToggle freeze, randOct, specFreeze;
    ShapeEditor shapeEditor { proc };
    ChainStrip chain { proc };
    juce::TextButton shuffleBtn { "shuffle" };
    juce::TextButton randomBtn { "randomize" };
    ThemeSwitch themeSwitch;
    juce::ToggleButton gravityBtn { "gravity" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gravityAtt;
    juce::ComboBox presetBox;
    int shownPreset = -1;
    HydroControl hydroCtl { proc };

    // the water along the bottom: a strip when things are calm, a pool when gravity's on
    std::vector<float> waveH, waveV;          // surface displacement per column, and its speed
    std::vector<float> lastDepth;             // per body: how deep it was last step (for splashes)
    struct Droplet { juce::Point<float> p, v; float life; };
    std::vector<Droplet> droplets;
    float waterLevel = 18.0f, gravVis = 0.0f, splashEnergy = 0.0f;
    static constexpr float calmLevel = 18.0f, poolLevel = 190.0f;
    float surfaceY (float x) const noexcept;
    void stepWater (float dt);
    void drawWater (juce::Graphics&);
    void splash (float x, float strength);
    juce::Rectangle<int> meterArea;

    // the waterslide
    juce::Path tube, tubeOutline, farTube;
    std::vector<juce::Point<float>> tubePts, tubeTan; // evenly spaced samples along the centre line
    float tubeWidth = 128.0f, tubeStep = 4.0f;
    std::vector<AmbientBubble> ambient;

    // two cached layers: the sky + slide behind (it drifts with the mouse: parallax),
    // and the glass panels in front
    juce::Image backLayer, frontLayer, panelLayer;   // panelLayer: the glass frames (they go when gravity's on)
    juce::Image farLayer;                            // on retina: the far layers composed at 1x each frame
    bool composeFarAt1x = true;
    int farFrame = 0;
    float pixelScale = 1.0f;
    float staticScale = 0.0f;
    static constexpr int parallaxMargin = 28;
    juce::Point<float> parallax;

    std::vector<Body> bodies;
    bool gravityOn = false;
    juce::Point<int> lastScreenPos;
    bool haveScreenPos = false;

    double lastTick = 0.0;
    float t = 0.0f;
    float lastFreeze = -1.0f;

    friend struct CloudfinTestAccess; // headless test drives the timer / gravity
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloudfinEditor)
};
