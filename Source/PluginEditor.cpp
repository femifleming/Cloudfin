#include "PluginEditor.h"

using namespace juce;

namespace
{
    inline float frac (float x) { return x - std::floor (x); }
    const String sep (CharPointer_UTF8 ("  \xc2\xb7  "));
    const String loopGlyph (CharPointer_UTF8 ("\xe2\x86\xba"));
    const String minusGlyph (CharPointer_UTF8 ("\xe2\x88\x92"));
    const String chevGlyph (CharPointer_UTF8 ("\xe2\x80\xba"));
}

//==============================================================================
// Themes
// How the type is set: 30 % bigger than it was first drawn and heavier, and kept out of the dither
// (it is drawn in one exact colour that the screen passes through untouched, so the smallest values
// stay solid). The panels are packed tight: each knob's attenuverter sits in its bottom corner.
namespace look
{
    struct Tune { float type = 1.3f; bool heavy = true, crisp = true, compact = true; };
    static const Tune& tune() { static const Tune t; return t; }
    // small type: at full strength, in the theme's one text colour, when it is to stay out of the dither
    static Colour text (const Theme& t, Colour asDrawn) { return tune().crisp ? t.deep : asDrawn; }
    static int rows (int h) { return roundToInt ((float) h * (tune().type > 1.0f ? 0.5f + 0.5f * tune().type + 0.12f : 1.0f)); }
}

const Theme& Theme::dayTheme()
{
    static const Theme t = []
    {
        Theme x;
        x.mode = 0;
        x.skyTop = Colour (0xff8fcdf4); x.skyMid = Colour (0xffdcf2ff); x.skyWarm = Colour (0xffd2efff); x.skyLow = Colour (0xffb4e4f5);
        x.deep = Colour (0xff0c4770); x.mist = Colour (0xff4f7f9f); x.ink = x.deep; x.titleShadow = Colours::white.withAlpha (0.95f);
        x.ocean = Colour (0xff1479b8); x.aqua = Colour (0xff35c3f2); x.aquaLight = Colour (0xffa6e9ff); x.glint = Colours::white;
        x.halo = Colours::transparentBlack;
        x.violet = Colour (0xff9886ff); x.lime = Colour (0xff86dc5e); x.warm = Colour (0xffffd27a);
        x.cloudShade = Colour (0xff9fc3dd);
        const uint32 tc[6] = { 0xffcadff0, 0xffd9eaf7, 0xffe7f2fb, 0xfff2f8fd, 0xfffafdff, 0xffffffff };
        for (int i = 0; i < 6; ++i) x.cloud[i] = Colour (tc[i]);
        x.cloudGlowA = Colour (0xff62c6ff); x.cloudGlowB = Colour (0xff8fd9ff);
        x.glintA = Colour (0xff4fc3ff); x.glintB = Colour (0xff58ccff);
        x.panelTop = Colours::white.withAlpha (0.52f); x.panelBottom = Colour (0xffe2f4ff).withAlpha (0.32f);
        x.panelGlow = Colour (0xff35c3f2).withAlpha (0.18f); x.panelEdge = Colours::white.withAlpha (0.95f);
        x.panelSlab = Colour (0xff8dbadb).withAlpha (0.45f); x.gloss = 0.6f;
        x.rimA = Colour (0xff86e4ff); x.rimB = Colour (0xfffff0b0); x.rimC = Colour (0xffcdb6ff);
        x.wordA = Colour (0xffa6e9ff); x.wordB = Colour (0xff35c3f2); x.wordC = Colour (0xff1479b8); x.wordExtrude = Colour (0xff0d5a92);
        x.pillTop = Colours::white; x.pillBottom = Colour (0xffbfe7fa);
        return x;
    }();
    return t;
}

const Theme& Theme::duskTheme()
{
    static const Theme t = []
    {
        Theme x;
        x.mode = 1; x.dusk = true;
        x.skyTop = Colour (0xff4a6aa8); x.skyMid = Colour (0xff97a6d6); x.skyWarm = Colour (0xfff3c3a8); x.skyLow = Colour (0xffe7a189);
        x.deep = Colour (0xff16294a); x.mist = Colour (0xff4d5a84); x.ink = x.deep; x.titleShadow = Colours::white.withAlpha (0.95f);
        x.ocean = Colour (0xff1b5ea8); x.aqua = Colour (0xff3fa6e6); x.aquaLight = Colour (0xffa8d4ff); x.glint = Colour (0xffffd9b0);
        x.halo = Colours::transparentBlack;
        x.violet = Colour (0xffa08cff); x.lime = Colour (0xff9be07a); x.warm = Colour (0xffffb86b);
        x.cloudShade = Colour (0xff9097c2);
        const uint32 tc[6] = { 0xffc6c8e3, 0xffd5d4ec, 0xffe4def0, 0xfff0e7f1, 0xfff8eeee, 0xfffff6ee };
        for (int i = 0; i < 6; ++i) x.cloud[i] = Colour (tc[i]);
        x.cloudGlowA = Colour (0xffff9e6b); x.cloudGlowB = Colour (0xff8fb2ff);
        x.glintA = Colour (0xffffa65e); x.glintB = Colour (0xff6f9dff);
        x.panelTop = Colours::white.withAlpha (0.42f); x.panelBottom = Colour (0xffffe4d6).withAlpha (0.26f);
        x.panelGlow = Colour (0xffffb86b).withAlpha (0.22f); x.panelEdge = Colours::white.withAlpha (0.95f);
        x.panelSlab = Colour (0xff9a7fb0).withAlpha (0.45f); x.gloss = 0.6f;
        x.rimA = Colour (0xff9fd8ff); x.rimB = Colour (0xffffd59a); x.rimC = Colour (0xffd0b6ff);
        x.wordA = Colour (0xfffff2e0); x.wordB = Colour (0xffffb88a); x.wordC = Colour (0xff6f86d6); x.wordExtrude = Colour (0xff6a4f8f);
        x.pillTop = Colours::white; x.pillBottom = Colour (0xffffdccb);
        return x;
    }();
    return t;
}

const Theme& Theme::nightTheme()
{
    static const Theme t = []
    {
        Theme x;
        x.mode = 2; x.dark = true;
        x.skyTop = Colour (0xff050a22); x.skyMid = Colour (0xff0d1a46); x.skyWarm = Colour (0xff16306a); x.skyLow = Colour (0xff1a4470);
        x.deep = Colour (0xffe6eeff); x.mist = Colour (0xffa3b6e0); x.ink = Colour (0xff10204a); x.titleShadow = Colours::black.withAlpha (0.55f);
        x.ocean = Colour (0xff1665c8); x.aqua = Colour (0xff3fd2ff); x.aquaLight = Colour (0xffaaf2ff); x.glint = Colour (0xffd8f6ff);
        x.halo = Colour (0xff3fd2ff).withAlpha (0.26f);
        x.violet = Colour (0xffa58cff); x.lime = Colour (0xff9cf07a); x.warm = Colour (0xffffe2b0);
        x.cloudShade = Colour (0xff22356a);
        const uint32 tc[6] = { 0xff2e4280, 0xff3a5292, 0xff4864a6, 0xff5a7bba, 0xff7096cf, 0xff94b8e6 };
        for (int i = 0; i < 6; ++i) x.cloud[i] = Colour (tc[i]);
        x.cloudGlowA = Colour (0xff3fd2ff); x.cloudGlowB = Colour (0xff8fefff);
        x.glintA = Colour (0xff42e0ff); x.glintB = Colour (0xff9a86ff);
        x.panelTop = Colour (0xff1d2f66).withAlpha (0.60f); x.panelBottom = Colour (0xff0a1332).withAlpha (0.66f);
        x.panelGlow = Colour (0xff3fd2ff).withAlpha (0.20f); x.panelEdge = Colour (0xffbfe6ff).withAlpha (0.5f);
        x.panelSlab = Colour (0xff02061a).withAlpha (0.6f); x.gloss = 0.14f;
        x.rimA = Colour (0xff7fe3ff); x.rimB = Colour (0xffb9a8ff); x.rimC = Colour (0xff6fb2ff);
        x.wordA = Colour (0xffeaf8ff); x.wordB = Colour (0xff7fd8ff); x.wordC = Colour (0xff3b6fd6); x.wordExtrude = Colour (0xff0a1846);
        x.pillTop = Colour (0xffeef6ff); x.pillBottom = Colour (0xff9fc6f0);
        return x;
    }();
    return t;
}

const Theme& Theme::forMode (int m)
{
    return m <= 0 ? dayTheme() : (m == 1 ? duskTheme() : nightTheme());
}

Colour stageColour (int stage)
{
    static const uint32 c[fp::numStages] = { 0xff5ac8fa, 0xff7b8cff, 0xffa98bff, 0xff3fd0c0, 0xff62aaff, 0xff8fd9ff, 0xff7fd96a };
    return Colour (c[(size_t) jlimit (0, fp::numStages - 1, stage)]);
}

const char* stageName (int stage)
{
    static const char* n[fp::numStages] = { "smear", "pitch", "formant", "grains", "delay", "reverb", "comp" };
    return n[(size_t) jlimit (0, fp::numStages - 1, stage)];
}

//==============================================================================
// LookAndFeel
AeroLookAndFeel::AeroLookAndFeel() { setTheme (Theme::duskTheme()); }

void AeroLookAndFeel::setTheme (const Theme& t)
{
    th = &t;
    setColour (Slider::textBoxTextColourId, look::text (t, t.deep.withAlpha (0.8f)));
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, t.aqua.withAlpha (0.4f));
    setColour (Label::textColourId, t.deep);
    setColour (TextEditor::textColourId, t.deep);
    setColour (TextEditor::backgroundColourId, Colours::white.withAlpha (0.92f));
    setColour (TextEditor::highlightColourId, t.aqua.withAlpha (0.4f));
    setColour (CaretComponent::caretColourId, t.ocean);
    setColour (ComboBox::textColourId, t.ink);
    setColour (ComboBox::arrowColourId, t.ocean);
    setColour (PopupMenu::backgroundColourId, t.dusk ? Colour (0xf8fff6f0) : (t.dark ? Colour (0xf6eaf2ff) : Colour (0xf6f6fbff)));
    setColour (PopupMenu::textColourId, t.ink);
    setColour (PopupMenu::highlightedBackgroundColourId, (t.dusk ? t.warm : t.aqua).withAlpha (0.85f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ToggleButton::textColourId, t.deep);
    setColour (TooltipWindow::textColourId, t.ink);
    setColour (BubbleComponent::backgroundColourId, Colours::white);
    setColour (BubbleComponent::outlineColourId, t.aqua);
}

Font AeroLookAndFeel::uiFont (float h, bool bold)
{
    const auto& t = look::tune();
    if (t.heavy) return Font (FontOptions ("Avenir Next", bold ? "Heavy" : "Demi Bold", h * t.type));
    return Font (FontOptions ("Avenir Next", h * t.type, bold ? Font::bold : Font::plain));
}

Font AeroLookAndFeel::titleFont (float h)
{
    return Font (FontOptions ("Arial Rounded MT Bold", h, Font::plain));
}

Font AeroLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (13.0f, true); }
Font AeroLookAndFeel::getPopupMenuFont() { return uiFont (14.0f); }
Font AeroLookAndFeel::getSliderPopupFont (Slider&) { return uiFont (12.5f, true); }

Label* AeroLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (11.5f));
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::textColourId, look::text (*th, th->deep.withAlpha (0.8f)));
    return l;
}

void AeroLookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    const float rad = body.getHeight() * 0.5f;
    g.setColour (Colour (0x30206a9a));
    g.fillRoundedRectangle (body.translated (0, 2), rad);
    g.setGradientFill (ColourGradient (Colours::white, body.getCentreX(), body.getY(),
                                       th->aquaLight.interpolatedWith (Colours::white, 0.5f), body.getCentreX(), body.getBottom(), false));
    g.fillRoundedRectangle (body, rad);
    g.setColour (th->aqua.withAlpha (0.8f));
    g.drawRoundedRectangle (body.reduced (0.5f), rad, 1.0f);
}

//------------------------------------------------------------------------------
void AeroLookAndFeel::drawGlass (Graphics& g, Rectangle<float> r, bool hot) const
{
    const auto c = r.getCentre();
    const float R = r.getWidth() * 0.5f;

    // thin soap film: clear in the middle, tinted toward the rim
    ColourGradient body (Colours::white.withAlpha (0.12f), c.x, c.y,
                         (th->dusk ? Colour (0xff8fb0ff) : Colour (0xff6fcaff)).withAlpha (0.42f), c.x + R, c.y, true);
    body.addColour (0.72, Colours::white.withAlpha (0.20f));
    g.setGradientFill (body);
    g.fillEllipse (r);

    // iridescent rim
    ColourGradient irid (th->rimA, r.getX(), r.getY(), th->rimC, r.getRight(), r.getBottom(), false);
    irid.addColour (0.5, th->rimB);
    g.setGradientFill (irid);
    g.drawEllipse (r.reduced (0.9f), hot ? 2.3f : 1.6f);
    g.setColour (Colours::white.withAlpha (hot ? 0.98f : 0.82f));
    g.drawEllipse (r.reduced (0.2f), 0.9f);
}

void AeroLookAndFeel::drawGloss (Graphics& g, Rectangle<float> r) const
{
    const float d = r.getWidth();
    const auto c = r.getCentre();

    auto gl = Rectangle<float> (d * 0.66f, d * 0.40f).withCentre ({ c.x, r.getY() + d * 0.24f });
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.92f), gl.getCentreX(), gl.getY(),
                                       Colours::white.withAlpha (0.04f), gl.getCentreX(), gl.getBottom(), false));
    g.fillEllipse (gl);

    Path crescent;
    crescent.addCentredArc (c.x, c.y, d * 0.40f, d * 0.40f, 0.0f,
                            MathConstants<float>::pi * 0.62f, MathConstants<float>::pi * 0.98f, true);
    g.setColour ((th->dusk ? th->glint : Colours::white).withAlpha (0.6f));
    g.strokePath (crescent, PathStrokeType (jmax (1.0f, d * 0.03f), PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (Colours::white.withAlpha (0.85f));
    g.fillEllipse (Rectangle<float> (d * 0.075f, d * 0.075f).withCentre ({ c.x + d * 0.27f, c.y + d * 0.30f }));
}

void AeroLookAndFeel::drawWaterBubble (Graphics& g, Rectangle<float> r, float level, float phase, bool hot, float ghost) const
{
    const Theme& T = *th;
    const float d = r.getWidth(), R = d * 0.5f;
    const auto c = r.getCentre();
    Path circle;
    circle.addEllipse (r);

    // its shadow on the air below
    {
        auto sh = Rectangle<float> (d * 0.62f, d * 0.11f).withCentre ({ c.x, r.getBottom() + d * 0.10f });
        g.setGradientFill (ColourGradient (Colour (0x38206a9a), sh.getCentreX(), sh.getCentreY(),
                                           Colour (0x00206a9a), sh.getRight(), sh.getCentreY(), true));
        g.fillEllipse (sh);
    }
    if (hot)
        DropShadow ((T.dusk ? T.warm : T.aqua).withAlpha (0.55f), (int) (d * 0.30f), {}).drawForPath (g, circle);

    drawGlass (g, r, hot);

    level = jlimit (0.0f, 1.0f, level);
    if (level > 0.001f)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (circle);

        const float top = r.getY(), bottom = r.getBottom();
        const float y = bottom - level * (bottom - top);
        const float amp = d * 0.032f * (0.35f + 0.65f * std::sin (level * MathConstants<float>::pi));
        const float wob = time * 2.2f + phase * 6.0f;
        auto surf = [&] (float u)
        {
            return y + amp * std::sin (u * 6.2832f * 1.1f + wob) + amp * 0.45f * std::sin (u * 6.2832f * 2.3f - wob * 1.4f);
        };

        constexpr int steps = 18;
        Path water, surface;
        water.startNewSubPath (r.getX() - 2.0f, surf (0.0f));
        surface.startNewSubPath (r.getX() - 2.0f, surf (0.0f));
        for (int i = 1; i <= steps; ++i)
        {
            const float u = (float) i / steps;
            const float xx = r.getX() - 2.0f + u * (d + 4.0f);
            water.lineTo (xx, surf (u));
            surface.lineTo (xx, surf (u));
        }
        water.lineTo (r.getRight() + 2.0f, bottom + 2.0f);
        water.lineTo (r.getX() - 2.0f, bottom + 2.0f);
        water.closeSubPath();

        ColourGradient wg (T.aquaLight.withAlpha (0.93f), c.x, y, T.ocean.withAlpha (0.95f), c.x, bottom, false);
        wg.addColour (0.45, T.aqua.withAlpha (0.93f));
        g.setGradientFill (wg);
        g.fillPath (water);

        // light shining up through the water
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.38f), c.x - R * 0.2f, bottom - R * 0.3f,
                                           Colours::transparentWhite, c.x + R * 0.5f, bottom, true));
        g.fillPath (water);

        // sunset on the surface
        if (T.dusk)
        {
            g.setGradientFill (ColourGradient (T.glint.withAlpha (0.55f), c.x, y, T.glint.withAlpha (0.0f), c.x, y + R * 0.45f, false));
            g.fillPath (water);
        }

        // little bubbles rising inside
        const float depth = bottom - y;
        for (int k = 0; k < 3; ++k)
        {
            const float u = frac (time * (0.22f + 0.07f * (float) k) + (float) k * 0.37f + phase);
            const float by = bottom - R * 0.15f - u * (depth - R * 0.05f);
            if (by <= y + 2.0f) continue;
            const float bx = c.x + (float) (k - 1) * R * 0.34f + std::sin (time * 1.6f + (float) k * 2.1f + phase * 5.0f) * R * 0.12f;
            const float br = R * (0.05f + 0.022f * (float) k);
            g.setColour (Colours::white.withAlpha (0.6f * (1.0f - u * 0.6f)));
            g.drawEllipse (bx - br, by - br, 2.0f * br, 2.0f * br, 0.9f);
        }

        g.setColour (T.glint.withAlpha (0.8f));
        g.strokePath (surface, PathStrokeType (1.2f));
    }

    // where the random voltage is carrying the parameter right now
    if (ghost >= 0.0f)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (circle);
        const float gy = r.getBottom() - jlimit (0.0f, 1.0f, ghost) * d;
        g.setColour (T.lime.withAlpha (0.45f));
        g.fillRect (r.getX(), gy - 2.5f, d, 5.0f);
        g.setColour (Colours::white.withAlpha (0.95f));
        g.fillRect (r.getX(), gy - 0.75f, d, 1.5f);
    }

    drawGloss (g, r);
}

//------------------------------------------------------------------------------
void AeroLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float, float, Slider& s)
{
    const Theme& T = *th;
    auto& props = s.getProperties();
    const bool hot = s.isMouseOverOrDragging() && s.isEnabled();
    const float phase = (float) props.getWithDefault ("phase", 0.0f);
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float d = jmin (area.getWidth(), area.getHeight());
    auto r = Rectangle<float> (d, d).withCentre (area.getCentre());

    if (! props.contains ("atten"))
    {
        const bool modOn = (bool) props.getWithDefault ("modOn", false);
        const float ghost = modOn ? (float) props.getWithDefault ("mod", -1.0f) : -1.0f;
        drawWaterBubble (g, r, pos, phase, hot, ghost);
        return;
    }

    // attenuverter: water rises from the middle (+) or sinks from it (-)
    const float v = pos * 2.0f - 1.0f;
    const auto c = r.getCentre();
    const float R = d * 0.5f;
    Path circle;
    circle.addEllipse (r);
    if (hot)
        DropShadow (T.aqua.withAlpha (0.5f), (int) (d * 0.35f), {}).drawForPath (g, circle);
    drawGlass (g, r, hot);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (circle);
        if (std::abs (v) > 0.004f)
        {
            const float edge = c.y - v * R;
            const float amp = d * 0.035f * std::min (1.0f, std::abs (v) * 3.0f);
            const float wob = time * 2.6f + phase * 4.0f;
            Path wp;
            wp.startNewSubPath (r.getX() - 1.0f, c.y);
            constexpr int steps = 10;
            for (int i = 0; i <= steps; ++i)
            {
                const float u = (float) i / steps;
                wp.lineTo (r.getX() - 1.0f + u * (d + 2.0f), edge + amp * std::sin (u * 6.2832f + wob));
            }
            wp.lineTo (r.getRight() + 1.0f, c.y);
            wp.closeSubPath();
            const auto top = v > 0 ? T.aquaLight : Colour (0xffd9d2ff);
            const auto bot = v > 0 ? T.ocean : T.violet.darker (0.25f);
            g.setGradientFill (ColourGradient (top.withAlpha (0.95f), c.x, r.getY(), bot.withAlpha (0.95f), c.x, r.getBottom(), false));
            g.fillPath (wp);
        }
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillRect (r.getX(), c.y - 0.5f, d, 1.0f);
    }
    drawGloss (g, r);

    g.setFont (uiFont (9.0f, true));
    g.setColour (T.deep.withAlpha (0.55f));
    g.drawText ("+", Rectangle<float> (12, 10).withCentre ({ r.getRight() + 5.0f, r.getY() + 4.0f }), Justification::centred);
    g.drawText (minusGlyph, Rectangle<float> (12, 10).withCentre ({ r.getRight() + 5.0f, r.getBottom() - 4.0f }), Justification::centred);
}

void AeroLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool big = r.getHeight() > 50.0f;
    const float d = big ? jmin (r.getHeight() - 24.0f, r.getWidth()) : r.getHeight() - 4.0f;
    auto bubble = big ? Rectangle<float> (d, d).withCentre ({ r.getCentreX(), r.getY() + 2.0f + d * 0.5f })
                      : Rectangle<float> (d, d).withPosition (r.getX() + 2.0f, r.getCentreY() - d * 0.5f);

    const bool on = b.getToggleState();
    const float fill = b.getProperties().contains ("fill") ? (float) b.getProperties()["fill"] : (on ? 0.85f : 0.0f);
    drawWaterBubble (g, bubble, fill, big ? 0.3f : 0.7f, on || highlighted);

    g.setColour (look::text (*th, th->deep.withAlpha (b.isEnabled() ? 0.95f : 0.4f)));
    if (big)
    {
        g.setFont (uiFont (13.5f, true));
        g.drawText (b.getButtonText(), r.withTop (bubble.getBottom() + 4.0f), Justification::centredTop);
    }
    else
    {
        g.setFont (uiFont (12.5f, true));
        g.drawText (b.getButtonText(), r.withLeft (bubble.getRight() + 6.0f), Justification::centredLeft);
    }
}

void AeroLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const Theme& T = *th;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    r = r.withTrimmedBottom (2.5f);
    g.setColour (Colour (0x30102a50));
    g.fillRoundedRectangle (r.translated (0, 3.5f), rad);
    g.setColour (T.pillBottom.darker (0.35f));            // the pill's thickness
    g.fillRoundedRectangle (r.translated (0, 2.5f), rad);
    g.setGradientFill (ColourGradient (T.pillTop, 0, r.getY(), T.pillBottom, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.85f), 0, r.getY(), Colours::white.withAlpha (0.1f), 0, r.getCentreY(), false));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f).reduced (3.0f, 1.0f), rad * 0.6f);
    g.setColour (box.isMouseOver() ? T.aqua : Colours::white);
    g.drawRoundedRectangle (r, rad, 1.3f);

    auto bub = Rectangle<float> (r.getHeight() - 6.0f, r.getHeight() - 6.0f).withCentre ({ r.getRight() - rad, r.getCentreY() });
    g.setGradientFill (ColourGradient (T.aquaLight, bub.getCentreX(), bub.getY(), T.ocean, bub.getCentreX(), bub.getBottom(), false));
    g.fillEllipse (bub);
    g.setColour (Colours::white.withAlpha (0.9f));
    g.drawEllipse (bub, 1.0f);
    Path ch;
    const auto cc = bub.getCentre();
    const float s = bub.getWidth() * 0.2f;
    ch.startNewSubPath (cc.x - s, cc.y - s * 0.4f);
    ch.lineTo (cc.x, cc.y + s * 0.6f);
    ch.lineTo (cc.x + s, cc.y - s * 0.4f);
    g.strokePath (ch, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

void AeroLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (12, 1, box.getWidth() - box.getHeight() - 14, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void AeroLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const Theme& T = *th;
    auto full = b.getLocalBounds().toFloat().reduced (1.0f);
    const float depth = 2.5f;
    auto r = full.withTrimmedBottom (depth);
    const float rad = r.getHeight() * 0.5f;
    const bool on = down || b.getToggleState();
    g.setColour (Colour (0x30102a50));
    g.fillRoundedRectangle (r.translated (0, depth + 1.0f), rad);
    g.setColour ((on ? T.ocean : T.pillBottom).darker (0.4f));    // thickness
    g.fillRoundedRectangle (r.translated (0, depth), rad);
    if (down) r = r.translated (0, depth * 0.8f);                // pressed in
    g.setGradientFill (on ? ColourGradient (T.aquaLight, 0, r.getY(), T.ocean, 0, r.getBottom(), false)
                          : ColourGradient (T.pillTop.withAlpha (0.95f), 0, r.getY(), T.pillBottom, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.75f), 0, r.getY(), Colours::white.withAlpha (0.05f), 0, r.getCentreY(), false));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f).reduced (2.0f, 0.5f), rad * 0.6f);
    g.setColour (highlighted ? T.aqua : Colours::white);
    g.drawRoundedRectangle (r, rad, 1.0f);
}

void AeroLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    g.setFont (uiFont (b.getHeight() > 22 ? 12.5f : 9.0f, true));
    g.setColour (down ? Colours::white : th->ink);
    g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedBottom (3).translated (0, down ? 2 : 0), Justification::centred);
}

//==============================================================================
FloatKnob::FloatKnob (CloudfinProcessor& p, const char* paramId, const String& title, bool withAttenuverter, Component* popupParent)
    : proc (p), hasAtt (withAttenuverter)
{
    modIndex = modIndexOf (paramId);
    Random rnd (String (paramId).hashCode64());
    ph1 = rnd.nextFloat() * 6.2832f; ph2 = rnd.nextFloat() * 6.2832f; ph3 = rnd.nextFloat() * 6.2832f;
    // drawn once into a picture, redrawn only when something in it changes. JUCE's own cache picks its
    // resolution from the current transform, which wobbles as a ball rolls over (so it would redraw every
    // frame under gravity); this one always uses the window's real pixel scale.
    struct Cache : public CachedComponentImage
    {
        explicit Cache (Component& c) : owner (c) {}
        void paint (Graphics& g) override
        {
            const float scale = jmax (1.0f, FloatKnob::cacheScale);
            const auto bounds = owner.getLocalBounds();
            const auto ib = (bounds.toFloat() * scale).getSmallestIntegerContainer();
            if (image.isNull() || image.getBounds() != ib) { image = Image (Image::ARGB, jmax (1, ib.getWidth()), jmax (1, ib.getHeight()), true); valid = false; }
            if (! valid)
            {
                image.clear (image.getBounds());
                Graphics ig (image);
                ig.addTransform (AffineTransform::scale (scale));
                owner.paintEntireComponent (ig, true);
                valid = true;
            }
            g.setColour (Colours::black.withAlpha (owner.getAlpha()));
            if (FloatKnob::fastBlit) g.setImageResamplingQuality (Graphics::lowResamplingQuality);
            g.drawImageTransformed (image, AffineTransform::scale ((float) bounds.getWidth() / (float) ib.getWidth(),
                                                                   (float) bounds.getHeight() / (float) ib.getHeight()), false);
        }
        bool invalidateAll() override { if (! FloatKnob::justMoving) valid = false; return true; }
        bool invalidate (const Rectangle<int>&) override { if (! FloatKnob::justMoving) valid = false; return true; }
        void releaseResources() override { image = {}; }
        Component& owner;
        Image image;
        bool valid = false;
    };
    setCachedComponentImage (new Cache (*this));

    name.setText (title, dontSendNotification);
    name.setJustificationType (Justification::centred);
    name.setFont (AeroLookAndFeel::uiFont (12.5f, true));
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    main.setSliderStyle (Slider::RotaryVerticalDrag);
    main.setTextBoxStyle (Slider::TextBoxBelow, false, 84, look::rows (15));
    main.setMouseDragSensitivity (220);
    main.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    main.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    main.getProperties().set ("phase", ph1 / 6.2832f);
    addAndMakeVisible (main);
    mainAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, main);
    if (auto* prm = p.apvts.getParameter (paramId))
        main.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));

    if (hasAtt)
    {
        att.setSliderStyle (Slider::RotaryVerticalDrag);
        att.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        att.setMouseDragSensitivity (160);
        att.getProperties().set ("atten", true);
        att.getProperties().set ("phase", ph2 / 6.2832f);
        att.setPopupDisplayEnabled (true, true, popupParent, 1500);
        addAndMakeVisible (att);
        attAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids::att (paramId), att);
        att.setDoubleClickReturnValue (true, 0.0);
    }
    applyTheme (Theme::duskTheme());
}

void FloatKnob::applyTheme (const Theme& t)
{
    name.setColour (Label::textColourId, t.deep);
    main.setColour (Slider::textBoxTextColourId, look::text (t, t.deep.withAlpha (0.8f)));
}

void FloatKnob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (look::rows (15)));
    if (look::tune().compact)
    {
        // packed tight: the attenuverter sits in the bottom corner of the knob's own space instead of under it
        if (hasAtt) att.setBounds (r.getRight() - 26, r.getBottom() - look::rows (15) - 24, 26, 26);
        main.setBounds (r);
        att.toFront (false);
        return;
    }
    if (hasAtt)
        att.setBounds (r.removeFromBottom (32).withSizeKeepingCentre (30, 30));
    main.setBounds (r);
}

void FloatKnob::animate (float t, float dt)
{
    // drift in the air; settle down while the mouse is near so it's easy to grab
    const bool engaged = isMouseOverOrDragging (true);
    settle = jlimit (0.0f, 1.0f, settle + (engaged ? -3.0f : 0.7f) * dt);
    const float e = fp::smoothstep (settle);
    bob = { e * (2.4f * std::sin (t * 0.41f + ph1) + 1.0f * std::sin (t * 1.03f + ph3)),
            e * (4.2f * std::sin (t * 0.63f + ph2) + 1.2f * std::sin (t * 1.27f + ph1)) };

    float modNow = 0.0f;
    if (modIndex >= 0)
    {
        const bool on = hasAtt && std::abs ((float) att.getValue()) > 0.004f;
        modNow = on ? proc.modNorm[(size_t) modIndex]->load() : -1.0f;
        main.getProperties().set ("modOn", on);
        main.getProperties().set ("mod", proc.modNorm[(size_t) modIndex]->load());
    }
    // the water sloshing inside moves slowly, so it's redrawn every sixth frame (staggered across the knobs),
    // and straight away while you're touching it or when its random voltage moves
    ++frame;
    const int every = fastBlit ? 10 : 6;   // tumbling about under gravity, the water inside matters even less
    if (engaged || (frame + (int) (ph1 * (float) every)) % every == 0 || std::abs (modNow - lastMod) > 0.004f)
    {
        lastMod = modNow;
        repaint();
    }
}

BubbleToggle::BubbleToggle (AudioProcessorValueTreeState& s, const char* paramId, const String& text)
{
    setButtonText (text);
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, paramId, *this);
}

//==============================================================================
ShapeEditor::ShapeEditor (CloudfinProcessor& p) : proc (p)
{
    proc.getGrainShape (table.data());
    seenVersion = proc.getGrainShapeVersion();
    const char* names[] = { "saw", "bell", "tri", "rev", "pulse", "smooth" };
    for (int i = 0; i < 6; ++i)
    {
        auto* b = presets.add (new TextButton (names[i]));
        b->onClick = [this, i] { applyPreset (i); };
        addAndMakeVisible (b);
    }
}

void ShapeEditor::resized()
{
    auto r = getLocalBounds();
    auto row = r.removeFromBottom (20);
    plot = r.reduced (4, 6).toFloat().withTrimmedBottom (2.0f);
    const int bw = (row.getWidth() - 5 * 3) / 6;
    for (auto* b : presets) { b->setBounds (row.removeFromLeft (bw)); row.removeFromLeft (3); }
}

void ShapeEditor::applyPreset (int which)
{
    if (which < fp::GrainShifter::numShapes)
        fp::GrainShifter::makeShape (which, table.data());
    else
    {
        for (int pass = 0; pass < 4; ++pass)
        {
            auto src = table;
            for (int i = 1; i + 1 < (int) table.size(); ++i)
                table[(size_t) i] = 0.25f * src[(size_t) i - 1] + 0.5f * src[(size_t) i] + 0.25f * src[(size_t) i + 1];
        }
    }
    push();
}

void ShapeEditor::push()
{
    proc.setGrainShape (table.data());
    seenVersion = proc.getGrainShapeVersion();
    repaint();
}

void ShapeEditor::drawAt (Point<float> p)
{
    constexpr int N = CloudfinProcessor::kShapeN;
    auto idx = [&] (Point<float> q) { return jlimit (0, N - 1, roundToInt ((q.x - plot.getX()) / plot.getWidth() * (float) (N - 1))); };
    auto val = [&] (Point<float> q) { return jlimit (0.0f, 1.0f, 1.0f - (q.y - plot.getY()) / plot.getHeight()); };
    const int i0 = idx (last), i1 = idx (p);
    const float v0 = val (last), v1 = val (p);
    if (i0 == i1) { table[(size_t) i1] = v1; return; }
    const int step = i1 > i0 ? 1 : -1;
    for (int i = i0; i != i1 + step; i += step)
        table[(size_t) i] = v0 + (v1 - v0) * (float) (i - i0) / (float) (i1 - i0);
}

void ShapeEditor::mouseDown (const MouseEvent& e)
{
    if (! plot.expanded (6.0f).contains (e.position)) return;
    drawing = true;
    last = e.position;
    drawAt (e.position);
    push();
}

void ShapeEditor::mouseDrag (const MouseEvent& e)
{
    if (! drawing) return;
    drawAt (e.position);
    last = e.position;
    push();
}

void ShapeEditor::mouseUp (const MouseEvent&) { drawing = false; }

void ShapeEditor::poll (float t, float grainSeconds)
{
    const float dt = jlimit (0.0f, 0.1f, t - time);
    time = t;
    if (! drawing && proc.getGrainShapeVersion() != seenVersion)
    {
        proc.getGrainShape (table.data());
        seenVersion = proc.getGrainShapeVersion();
    }
    riderPos = frac (riderPos + dt / jlimit (0.7f, 4.0f, grainSeconds * 6.0f));
}

void ShapeEditor::paint (Graphics& g)
{
    auto* lf = dynamic_cast<AeroLookAndFeel*> (&getLookAndFeel());
    const Theme& T = lf != nullptr ? lf->theme() : Theme::duskTheme();
    constexpr int N = CloudfinProcessor::kShapeN;
    auto box = plot.expanded (4.0f, 6.0f);
    // a recessed glass well
    g.setGradientFill (T.dark ? ColourGradient (Colour (0xff081030).withAlpha (0.75f), 0, box.getY(), Colour (0xff142456).withAlpha (0.6f), 0, box.getBottom(), false)
                              : ColourGradient (Colours::white.withAlpha (0.55f), 0, box.getY(),
                                                (T.dusk ? Colour (0xffffe4d6) : Colour (0xffd6f0fc)).withAlpha (0.45f), 0, box.getBottom(), false));
    g.fillRoundedRectangle (box, 12.0f);
    {
        Graphics::ScopedSaveState ss (g);
        Path well; well.addRoundedRectangle (box, 12.0f);
        g.reduceClipRegion (well);
        g.setColour (Colours::black.withAlpha (T.dark ? 0.45f : 0.14f));
        g.drawRoundedRectangle (box.translated (0, 2.5f).expanded (1.0f), 12.0f, 4.0f); // inner shadow at the top
    }
    g.setColour (T.panelEdge);
    g.drawRoundedRectangle (box, 12.0f, 1.2f);

    g.setColour (T.aqua.withAlpha (0.18f));
    for (int i = 1; i < 4; ++i)
    {
        g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 4.0f), plot.getX(), plot.getRight());
        g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 4.0f), plot.getY(), plot.getBottom());
    }

    auto pt = [&] (int i) { return Point<float> (plot.getX() + plot.getWidth() * (float) i / (float) (N - 1),
                                                 plot.getBottom() - table[(size_t) i] * plot.getHeight()); };
    Path curve, fill;
    curve.startNewSubPath (pt (0));
    fill.startNewSubPath (plot.getX(), plot.getBottom());
    for (int i = 0; i < N; ++i) { if (i) curve.lineTo (pt (i)); fill.lineTo (pt (i)); }
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();

    g.setGradientFill (ColourGradient (T.aquaLight.withAlpha (0.85f), 0, plot.getY(), T.ocean.withAlpha (0.6f), 0, plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (Colours::white.withAlpha (0.95f));
    g.strokePath (curve, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (T.ocean.withAlpha (0.8f));
    g.strokePath (curve, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));

    // the rider
    if (lf != nullptr)
    {
        const float fi = riderPos * (float) (N - 1);
        const int i = jmin ((int) fi, N - 2);
        const float v = table[(size_t) i] + (fi - (float) i) * (table[(size_t) i + 1] - table[(size_t) i]);
        const float x = plot.getX() + riderPos * plot.getWidth();
        const float y = plot.getBottom() - v * plot.getHeight();
        lf->drawWaterBubble (g, Rectangle<float> (13.0f, 13.0f).withCentre ({ x, y - 7.0f }), 0.45f, 0.2f, false);
    }

    g.setFont (AeroLookAndFeel::uiFont (10.5f, true));
    g.setColour (look::text (T, T.mist.withAlpha (0.85f)));
    g.drawText ("draw the grain", plot.reduced (6.0f, 2.0f), Justification::topRight);
}

//==============================================================================
ChainStrip::ChainStrip (CloudfinProcessor& p) : proc (p)
{
    order = proc.getOrder();
    for (int i = 0; i < fp::numStages; ++i) chipX[order[(size_t) i]] = slotX (i);
}

int ChainStrip::stageAt (Point<float> pos) const
{
    for (int i = 0; i < fp::numStages; ++i)
    {
        const int st = order[(size_t) i];
        if (Rectangle<float> (chipX[st], 1.0f, chipW, (float) getHeight() - 2.0f).contains (pos)) return st;
    }
    return -1;
}

void ChainStrip::mouseDown (const MouseEvent& e)
{
    dragStage = stageAt (e.position);
    if (dragStage >= 0) { grabOffset = e.position.x - chipX[dragStage]; dragX = chipX[dragStage]; }
}

void ChainStrip::mouseDrag (const MouseEvent& e)
{
    if (dragStage < 0) return;
    dragX = jlimit (slotX (0), slotX (fp::numStages - 1), e.position.x - grabOffset);
    chipX[dragStage] = dragX;
    // slot it in where it's hovering; the others slide out of the way
    const int want = jlimit (0, fp::numStages - 1, roundToInt ((dragX - endW) / (chipW + gap)));
    int cur = 0;
    for (int i = 0; i < fp::numStages; ++i) if (order[(size_t) i] == dragStage) cur = i;
    if (want != cur)
    {
        auto o = order;
        const int st = o[(size_t) cur];
        if (want > cur) for (int i = cur; i < want; ++i) o[(size_t) i] = o[(size_t) i + 1];
        else            for (int i = cur; i > want; --i) o[(size_t) i] = o[(size_t) i - 1];
        o[(size_t) want] = st;
        order = o;
    }
    repaint();
}

void ChainStrip::mouseUp (const MouseEvent&)
{
    if (dragStage >= 0)
    {
        dragStage = -1;
        proc.setOrder (order); // applied with a 40 ms dip, so it never clicks
    }
}

void ChainStrip::mouseDoubleClick (const MouseEvent&)
{
    order = fp::defaultOrder();
    proc.setOrder (order);
}

void ChainStrip::shuffle()
{
    const auto cur = proc.getOrder();
    auto o = cur;
    for (int tries = 0; tries < 8 && o == cur; ++tries)
        for (int i = fp::numStages - 1; i > 0; --i)
            std::swap (o[(size_t) i], o[(size_t) rnd.nextInt (i + 1)]);
    order = o;
    proc.setOrder (o);
}

void ChainStrip::poll (float dt)
{
    if (dragStage < 0) order = proc.getOrder();
    const float k = jmin (1.0f, dt * 12.0f);
    for (int i = 0; i < fp::numStages; ++i)
    {
        const int st = order[(size_t) i];
        if (st != dragStage) chipX[st] += (slotX (i) - chipX[st]) * k;
    }
}

void ChainStrip::paint (Graphics& g)
{
    auto* lf = dynamic_cast<AeroLookAndFeel*> (&getLookAndFeel());
    const Theme& T = lf != nullptr ? lf->theme() : Theme::duskTheme();
    const float h = (float) getHeight();

    // the arrows between slots, and the loop back at the end
    g.setFont (AeroLookAndFeel::uiFont (16.0f, true));
    g.setColour (T.deep.withAlpha (0.55f));
    for (int i = 0; i < fp::numStages - 1; ++i)
        g.drawText (chevGlyph, Rectangle<float> (slotX (i) + chipW, 0, gap, h), Justification::centred);
    g.drawText (chevGlyph, Rectangle<float> (0, 0, endW, h), Justification::centred);                                  // in
    g.drawText (loopGlyph, Rectangle<float> (slotX (fp::numStages - 1) + chipW, 0, endW, h), Justification::centred);   // and round again

    auto chip = [&] (int st, bool lifted)
    {
        auto r = Rectangle<float> (chipX[st], 1.0f, chipW, h - 5.0f);
        if (lifted) r = r.expanded (3.0f, 2.0f).translated (0, -3.0f);
        const float rad = r.getHeight() * 0.5f;
        const auto c = stageColour (st);
        g.setColour (Colour (0x38102a50));
        g.fillRoundedRectangle (r.translated (0, lifted ? 7.0f : 4.0f), rad);
        g.setColour (c.darker (0.55f));                        // the chip's thickness
        g.fillRoundedRectangle (r.translated (0, lifted ? 4.0f : 2.5f), rad);
        g.setGradientFill (ColourGradient (Colours::white.interpolatedWith (c, 0.25f), 0, r.getY(), c.interpolatedWith (Colours::white, 0.15f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.8f), 0, r.getY(), Colours::white.withAlpha (0.05f), 0, r.getCentreY(), false));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f).reduced (3.0f, 1.0f), rad * 0.6f);
        g.setColour (Colours::white);
        g.drawRoundedRectangle (r, rad, lifted ? 1.8f : 1.2f);
        g.setFont (AeroLookAndFeel::uiFont (12.5f, true));
        g.setColour (T.ink);
        g.drawText (stageName (st), r, Justification::centred);
    };
    for (int i = 0; i < fp::numStages; ++i)
        if (order[(size_t) i] != dragStage) chip (order[(size_t) i], false);
    if (dragStage >= 0) chip (dragStage, true);
}

//==============================================================================
void ThemeSwitch::setMode (int m, bool animate)
{
    mode = jlimit (0, 2, m);
    if (! animate) pos = (float) mode * 0.5f;
    repaint();
}

void ThemeSwitch::mouseUp (const MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    const int seg = jlimit (0, 2, (int) (e.position.x / ((float) getWidth() / 3.0f)));
    if (seg == mode) return;
    setMode (seg, true);
    if (onChange) onChange (mode);
}

void ThemeSwitch::poll (float dt)
{
    const float target = (float) mode * 0.5f;
    if (std::abs (pos - target) > 0.001f) { pos += (target - pos) * jmin (1.0f, dt * 10.0f); repaint(); }
}

namespace
{
    // 0 sun, 1 setting sun, 2 moon
    void drawSkyIcon (Graphics& g, int type, Point<float> c, float d, float alpha)
    {
        if (type == 0)
        {
            g.setColour (Colour (0xffffd25a).withAlpha (alpha));
            for (int i = 0; i < 8; ++i)
            {
                const float a = (float) i * MathConstants<float>::twoPi / 8.0f;
                g.drawLine (c.x + std::cos (a) * d * 0.34f, c.y + std::sin (a) * d * 0.34f, c.x + std::cos (a) * d * 0.5f, c.y + std::sin (a) * d * 0.5f, 1.5f);
            }
            g.setGradientFill (ColourGradient (Colour (0xfffff6c8).withAlpha (alpha), c.x - 2, c.y - 3, Colour (0xffffb83d).withAlpha (alpha), c.x, c.y + d * 0.3f, false));
            g.fillEllipse (Rectangle<float> (d * 0.52f, d * 0.52f).withCentre (c));
        }
        else if (type == 1)
        {
            {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (Rectangle<float> (c.x - d, c.y - d, 2 * d, d + d * 0.08f).toNearestInt());
            g.setGradientFill (ColourGradient (Colour (0xffffe0a0).withAlpha (alpha), c.x, c.y - d * 0.3f, Colour (0xffff8a5a).withAlpha (alpha), c.x, c.y + d * 0.1f, false));
            g.fillEllipse (Rectangle<float> (d * 0.62f, d * 0.62f).withCentre ({ c.x, c.y + d * 0.08f }));
            g.setColour (Colour (0xffffc07a).withAlpha (alpha));
            for (int i = 0; i < 5; ++i)
            {
                const float a = MathConstants<float>::pi * (1.0f + 0.2f + 0.15f * (float) i);
                g.drawLine (c.x + std::cos (a) * d * 0.40f, c.y + d * 0.08f + std::sin (a) * d * 0.40f,
                            c.x + std::cos (a) * d * 0.52f, c.y + d * 0.08f + std::sin (a) * d * 0.52f, 1.4f);
            }
            }
            g.setColour (Colours::white.withAlpha (0.8f * alpha));
            g.drawLine (c.x - d * 0.45f, c.y + d * 0.12f, c.x + d * 0.45f, c.y + d * 0.12f, 1.4f);
        }
        else
        {
            Path clip;
            clip.addRectangle (Rectangle<float> (2 * d, 2 * d).withCentre (c));
            clip.addEllipse (Rectangle<float> (d * 0.6f, d * 0.6f).withCentre ({ c.x + d * 0.2f, c.y - d * 0.14f }));
            clip.setUsingNonZeroWinding (false);
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);
            g.setColour (Colour (0xfff3f6ff).withAlpha (alpha));
            g.fillEllipse (Rectangle<float> (d * 0.66f, d * 0.66f).withCentre (c));
        }
    }
}

void ThemeSwitch::paint (Graphics& g)
{
    auto full = getLocalBounds().toFloat().reduced (1.0f);
    auto r = full.withTrimmedBottom (3.0f);
    const float rad = r.getHeight() * 0.5f;

    // track colour slides day -> dusk -> night
    const Colour a[3] = { Colour (0xffbfe8ff), Colour (0xff7d8cc8), Colour (0xff0b1640) };
    const Colour b[3] = { Colour (0xff6cc6f5), Colour (0xfff2a582), Colour (0xff2c4fa0) };
    const float u = jlimit (0.0f, 1.0f, pos) * 2.0f;
    const int i0 = jmin (1, (int) u);
    const float f = fp::smoothstep (u - (float) i0);
    const auto ca = a[i0].interpolatedWith (a[i0 + 1], f), cb = b[i0].interpolatedWith (b[i0 + 1], f);

    g.setColour (Colour (0x38102a50));
    g.fillRoundedRectangle (r.translated (0, 4.0f), rad);
    g.setColour (cb.darker (0.5f));
    g.fillRoundedRectangle (r.translated (0, 2.5f), rad);
    g.setGradientFill (ColourGradient (ca, r.getX(), 0, cb, r.getRight(), 0, false));
    g.fillRoundedRectangle (r, rad);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.6f), 0, r.getY(), Colours::white.withAlpha (0.05f), 0, r.getCentreY(), false));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f).reduced (4.0f, 1.0f), rad * 0.6f);
    g.setColour (Colours::white.withAlpha (0.9f));
    g.drawRoundedRectangle (r, rad, 1.3f);

    const float segW = r.getWidth() / 3.0f;
    const float d = r.getHeight() - 6.0f;
    for (int k = 0; k < 3; ++k)
        drawSkyIcon (g, k, { r.getX() + segW * ((float) k + 0.5f), r.getCentreY() }, d * 0.8f, 0.45f);

    // the glossy knob, carrying the current sky
    const Point<float> kc (r.getX() + segW * (0.5f + 2.0f * jlimit (0.0f, 1.0f, pos)), r.getCentreY());
    auto kr = Rectangle<float> (d + 4.0f, d + 4.0f).withCentre (kc);
    g.setColour (Colour (0x40102a50));
    g.fillEllipse (kr.translated (0, 2.0f));
    g.setGradientFill (ColourGradient (Colours::white, kc.x, kr.getY(), Colour (0xffcfe0f2), kc.x, kr.getBottom(), false));
    g.fillEllipse (kr);
    g.setColour (Colours::white);
    g.drawEllipse (kr, 1.0f);
    drawSkyIcon (g, mode, kc, d * 0.85f, 1.0f);
}

//==============================================================================
//==============================================================================
// The whole window goes through a sixteen-colour screen: it is drawn as ever into a picture the
// size of the window (in points, so on a Retina display each of its pixels is doubled), every pixel
// is snapped to that mode's sixteen colours through an 8 x 8 ordered dither, and the picture is put
// up without smoothing. Edges and type stay as sharp as they were; every soft gradient becomes a fine
// printed-looking pattern, like a mid-90s display showing more than it has colours for.
namespace
{
// sixteen colours for each mode, picked (by median cut) from the window as it looked before
const uint32 ditherPalettes[3][16] = {
    { 0x4f91ba, 0x7ab4d6, 0x8dc0dc, 0xa1c4db, 0x91cce8, 0xa7cae0, 0xaccce1, 0xb4cfe1, 0xa2dbf2, 0xbdd8e9, 0xc6d9e7, 0xcce6f5, 0xdbe8ef, 0xdeedf7, 0xe4f2f9, 0xf3f8fb },   // day
    { 0x586895, 0x7580a8, 0x7c91b8, 0xa696a9, 0x92a0c1, 0xafa1b5, 0xb7a5b4, 0xbfa6ad, 0xb0afc4, 0xc6b2b9, 0xacbfd5, 0xcac2cd, 0xdac8cb, 0xdfd4d8, 0xe6dcdf, 0xf2edee },   // dusk
    { 0x070e28, 0x0f1731, 0x101c39, 0x161f37, 0x18233d, 0x192844, 0x202942, 0x25304a, 0x283857, 0x2e436a, 0x3d4867, 0x485776, 0x4b7aa7, 0x7c8da9, 0x7dcaea, 0xcddbea } }; // night

struct DitherScreen : public CachedComponentImage
{
    DitherScreen (Component& c, std::function<int()> modeNow, std::function<float()> scaleNow) : comp (c), mode (std::move (modeNow)), scale (std::move (scaleNow))
    {
        // the 8 x 8 Bayer matrix, as how far each pixel is pushed before it's snapped (about half the gap between neighbouring colours either way)
        int m[8][8] = { { 0 } };
        for (int n = 1; n < 8; n *= 2)
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                {
                    const int v = 4 * m[y][x];
                    m[y][x] = v; m[y][x + n] = v + 2; m[y + n][x] = v + 3; m[y + n][x + n] = v + 1;
                }
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) offset[y][x] = roundToInt ((((float) m[y][x] + 0.5f) / 64.0f - 0.5f) * 131.0f);
    }

    void paint (Graphics& g) override
    {
        const float s = jmax (0.25f, scale());
        const int w = roundToInt ((float) comp.getWidth() * s), h = roundToInt ((float) comp.getHeight() * s);
        if (image.getWidth() != w || image.getHeight() != h) image = Image (Image::ARGB, w, h, true);
        {
            Graphics ig (image);
            ig.addTransform (AffineTransform::scale (s));
            comp.paintEntireComponent (ig, true);
        }
        if (const int now = jlimit (0, 2, mode()); now != lutMode) buildLut (now);
        const Colour keys[2] = { Theme::forMode (lutMode).deep, Theme::forMode (lutMode).ink };
        {
            Image::BitmapData px (image, Image::BitmapData::readWrite);
            for (int y = 0; y < h; ++y)
            {
                auto* row = (PixelARGB*) px.getLinePointer (y);
                const int* off = offset[y & 7];
                for (int x = 0; x < w; ++x)
                {
                    if (look::tune().crisp)
                    {
                        // type drawn in the theme's text colour goes through as it is: solid, never dithered
                        const int r0 = row[x].getRed(), g0 = row[x].getGreen(), b0 = row[x].getBlue();
                        bool isText = false;
                        for (const auto& k : keys)
                            if (std::abs (r0 - (int) k.getRed()) < 14 && std::abs (g0 - (int) k.getGreen()) < 14 && std::abs (b0 - (int) k.getBlue()) < 14)
                            { row[x].setARGB (255, k.getRed(), k.getGreen(), k.getBlue()); isText = true; break; }
                        if (isText) continue;
                    }
                    const int o = off[x & 7];
                    const int r = jlimit (0, 255, (int) row[x].getRed() + o), gr = jlimit (0, 255, (int) row[x].getGreen() + o), b = jlimit (0, 255, (int) row[x].getBlue() + o);
                    const uint32 c = ditherPalettes[lutMode][lut[((r >> 3) << 10) | ((gr >> 3) << 5) | (b >> 3)]];
                    row[x].setARGB (255, (uint8) (c >> 16), (uint8) (c >> 8), (uint8) c);
                }
            }
        }
        g.setImageResamplingQuality (Graphics::lowResamplingQuality);
        g.drawImageTransformed (image, AffineTransform::scale (1.0f / s));
    }
    bool invalidateAll() override { return true; }
    bool invalidate (const Rectangle<int>&) override { return true; }
    void releaseResources() override { image = {}; }

    // for every 15-bit colour, which of the sixteen is nearest
    void buildLut (int m)
    {
        lutMode = m;
        for (int i = 0; i < 32768; ++i)
        {
            const int r = ((i >> 10) & 31) * 8 + 4, g = ((i >> 5) & 31) * 8 + 4, b = (i & 31) * 8 + 4;
            int best = 0, bestD = 1 << 30;
            for (int k = 0; k < 16; ++k)
            {
                const uint32 c = ditherPalettes[m][k];
                const int dr = r - (int) ((c >> 16) & 255), dg = g - (int) ((c >> 8) & 255), db = b - (int) (c & 255);
                const int d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
                if (d < bestD) { bestD = d; best = k; }
            }
            lut[i] = (uint8) best;
        }
    }

    Component& comp;
    std::function<int()> mode;
    std::function<float()> scale;
    Image image;
    int offset[8][8] {};
    uint8 lut[32768] {};
    int lutMode = -1;
};
}

CloudfinEditor::CloudfinEditor (CloudfinProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      freeze (p.apvts, ids::grFreeze, "freeze"),
      randOct (p.apvts, ids::grRandOct, "random octave"),
      specFreeze (p.apvts, ids::sgFreeze, "spectral freeze")
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setCachedComponentImage (new DitherScreen (content, [this] { return lnf.theme().mode; }, [this] { return (float) getWidth() / (float) designW; }));

    panels.reserve (9);
    auto addPanel = [this] (const String& title, const String& sub, int stage)
    {
        panels.push_back ({ {}, title, sub, {}, stage });
        return &panels.back();
    };
    sgP = addPanel ("spectral smear", "", fp::stGate);
    psP = addPanel ("pitch", "", fp::stPitch);
    fmP = addPanel ("formant", "a  e  i  o  u", fp::stFormant);
    grP = addPanel ("grains", "4 s live tape", fp::stGrains);
    dlP = addPanel ("ji delay", "", fp::stDelay);
    rvP = addPanel ("reverb", "", fp::stReverb);
    cpP = addPanel ("compander", "", fp::stComp);
    fbP = addPanel ("feedback", "", -1);
    drP = addPanel ("drift", "", -1);

    sgP->knobs = { knob (ids::sgCutoff, "cutoff"), knob (ids::sgAttack, "attack"), knob (ids::sgSmear, "smear"),
                   knob (ids::sgKnee, "knee"), knob (ids::sgBlur, "blur"), knob (ids::sgScatter, "scatter"),
                   knob (ids::sgTilt, "tilt"), knob (ids::sgMix, "mix") };
    psP->knobs = { knob (ids::psShift, "shift"), knob (ids::psGrain, "grain"), knob (ids::psFeedback, "feedback"),
                   knob (ids::psMix, "mix") };
    fmP->knobs = { knob (ids::fmVowel, "vowel"), knob (ids::fmShift, "shift"), knob (ids::fmRes, "resonance"),
                   knob (ids::fmMix, "mix") };
    grP->knobs = { knob (ids::grSize, "size"), knob (ids::grDensity, "density"), knob (ids::grPosition, "position"),
                   knob (ids::grSpray, "spray"), knob (ids::grWidth, "rand pan"), knob (ids::grReverse, "reverse"),
                   knob (ids::grOctave, "octave"), knob (ids::grFade, "freeze fade"), knob (ids::grMix, "mix") };
    dlP->knobs = { knob (ids::dlTime, "time"), knob (ids::dlFeedback, "feedback"), knob (ids::dlTone, "tone"),
                   knob (ids::dlMix, "mix") };
    rvP->knobs = { knob (ids::rvDecay, "decay"), knob (ids::rvTone, "tone"), knob (ids::rvMix, "mix") };
    cpP->knobs = { knob (ids::cpThresh, "threshold"), knob (ids::cpRatio, "ratio"), knob (ids::cpBelow, "below"),
                   knob (ids::cpAttack, "attack"), knob (ids::cpRelease, "release"), knob (ids::cpMakeup, "makeup"),
                   knob (ids::cpMix, "mix") };
    fbP->knobs = { knob (ids::masterFb, "amount") };
    drP->knobs = { knob (ids::modRate, "rate", false), knob (ids::modSlew, "slew", false) };

    scaleBox.addItemList ([] { StringArray a; for (auto& s : fp::getScales()) a.add (s.name); return a; }(), 1);
    scaleBox.setJustificationType (Justification::centredLeft);
    content.addAndMakeVisible (scaleBox);
    scaleAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ids::psScale, scaleBox);

    for (Component* c : std::initializer_list<Component*> { &freeze, &randOct, &specFreeze, &shapeEditor, &chain, &shuffleBtn, &themeSwitch, &gravityBtn })
        content.addAndMakeVisible (c);

    shuffleBtn.onClick = [this] { chain.shuffle(); };
    randomBtn.onClick = [this] { proc.randomize(); };
    randomBtn.setTooltip ("a random patch: every control somewhere new");
    content.addAndMakeVisible (randomBtn);
    shuffleBtn.setTooltip ("shuffle the order of the modules");
    themeSwitch.onChange = [this] (int m) { setThemeMode (m); };
    gravityAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ids::gravity, gravityBtn);

    // presets, in their groups
    {
        String group;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            if (p.getProgramGroup (i) != group) { group = p.getProgramGroup (i); if (i > 0) presetBox.addSectionHeading (group); }
            presetBox.addItem (p.getProgramName (i), i + 1);
        }
        presetBox.setTextWhenNothingSelected ("presets");
        presetBox.setJustificationType (Justification::centredLeft);
        presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
        content.addAndMakeVisible (presetBox);
    }
    content.addAndMakeVisible (hydroCtl);
    waveH.assign (120, 0.0f);
    waveV.assign (120, 0.0f);

    buildClouds();
    Random rnd (7);
    for (int i = 0; i < 15; ++i)   // (each is drawn twice: once on each side of the centre line)
        ambient.push_back ({ rnd.nextFloat() * designW, 10.0f + rnd.nextFloat() * 26.0f, 5.0f + std::pow (rnd.nextFloat(), 2.0f) * 26.0f,
                             rnd.nextFloat() * 6.2832f, rnd.nextFloat() * 2000.0f });

    layoutContent();

    // every control becomes a body for the gravity toy
    for (auto* k : knobs)
        bodies.push_back ({ k, k, {}, {}, {}, 44.0f });
    bodies.push_back ({ &freeze, nullptr, {}, {}, {}, 46.0f });
    bodies.push_back ({ &randOct, nullptr, {}, {}, {}, 22.0f });
    bodies.push_back ({ &specFreeze, nullptr, {}, {}, {}, 22.0f });
    for (auto& b : bodies) b.home = b.pos = b.comp->getBounds().toFloat().getCentre();

    const int mode = jlimit (0, 2, proc.themeMode.load());
    themeSwitch.setMode (mode, false);
    setThemeMode (mode);

    setResizable (true, true);
    setResizeLimits (designW / 2, designH / 2, (int) (designW * 1.6), (int) (designH * 1.6));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / designH);
    setSize ((int) (designW * 0.85), (int) (designH * 0.85));

    lastTick = Time::getMillisecondCounterHiRes() * 0.001;
    timerCallback();
    startTimerHz (30);
}

CloudfinEditor::~CloudfinEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

FloatKnob* CloudfinEditor::knob (const char* id, const String& title, bool withAtt)
{
    auto* k = knobs.add (new FloatKnob (proc, id, title, withAtt, this));
    content.addAndMakeVisible (k);
    return k;
}

void CloudfinEditor::setThemeMode (int mode)
{
    proc.themeMode.store (mode);
    const Theme& th = Theme::forMode (mode);
    lnf.setTheme (th);
    for (auto* k : knobs) k->applyTheme (th);
    backLayer = frontLayer = {};
    content.repaint();
    sendLookAndFeelChange();
}

void CloudfinEditor::setGravity (bool on)
{
    gravityOn = on;
    haveScreenPos = false;
    if (on)
    {
        Random rnd;
        for (auto& b : bodies)
            b.vel += { (rnd.nextFloat() - 0.5f) * 300.0f, -rnd.nextFloat() * 250.0f };
    }
}


//==============================================================================
HydroControl::HydroControl (CloudfinProcessor& p)
    : proc (p),
      levelAtt (*p.apvts.getParameter (ids::hydroLevel), [this] (float v) { level = v; repaint(); }),
      intoAtt (*p.apvts.getParameter (ids::hydroInto), [this] (float v) { into = v; repaint(); })
{
    levelAtt.sendInitialUpdate();
    intoAtt.sendInitialUpdate();
    setRepaintsOnMouseActivity (true);
}

Rectangle<float> HydroControl::capsule() const { return getLocalBounds().toFloat().withTrimmedRight (106.0f).reduced (1.0f, 3.0f); }
Rectangle<float> HydroControl::pill() const    { return getLocalBounds().toFloat().removeFromRight (100.0f).reduced (1.0f, 4.0f); }

void HydroControl::paint (Graphics& g)
{
    if (theme == nullptr) return;
    const auto& T = *theme;
    const auto c = capsule();
    const float rad = c.getHeight() * 0.5f;
    // glass capsule
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.75f), 0, c.getY(), T.pillBottom.withAlpha (0.6f), 0, c.getBottom(), false));
    g.fillRoundedRectangle (c, rad);
    // water inside, up to the level, with a wobbling edge
    if (level > 0.002f)
    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (c.reduced (2.0f), rad - 2.0f);
        g.reduceClipRegion (clip);
        const float x1 = c.getX() + 2.0f + (c.getWidth() - 4.0f) * level;
        Path w;
        w.startNewSubPath (c.getX(), c.getY());
        for (int i = 0; i <= 8; ++i)
        {
            const float y = c.getY() + c.getHeight() * (float) i / 8.0f;
            w.lineTo (x1 + 2.5f * std::sin (t * 3.0f + (float) i * 0.9f), y);
        }
        w.lineTo (c.getX(), c.getBottom());
        w.closeSubPath();
        g.setGradientFill (ColourGradient (T.aquaLight, 0, c.getY(), T.ocean, 0, c.getBottom(), false));
        g.fillPath (w);
        for (int i = 0; i < 5; ++i)   // bubbles drifting along it
        {
            const float u = std::fmod (t * 0.15f + (float) i * 0.21f, 1.0f);
            const float bx = c.getX() + 6.0f + u * (x1 - c.getX() - 10.0f);
            g.setColour (Colours::white.withAlpha (0.6f));
            g.drawEllipse (Rectangle<float> (3.0f, 3.0f).withCentre ({ bx, c.getCentreY() + 4.0f * std::sin (t * 2.0f + (float) i) }), 0.8f);
        }
    }
    g.setColour (Colours::white.withAlpha (0.9f));
    g.drawRoundedRectangle (c, rad, 1.2f);
    g.setFont (AeroLookAndFeel::uiFont (12.0f, true));
    g.setColour (T.ink);
    g.drawText ("hydrophone " + String (roundToInt (level * 100.0f)) + " %", c, Justification::centred);

    // where it goes
    const auto p = pill();
    g.setGradientFill (ColourGradient (T.pillTop, 0, p.getY(), T.pillBottom, 0, p.getBottom(), false));
    g.fillRoundedRectangle (p, p.getHeight() * 0.5f);
    g.setColour (Colours::white.withAlpha (isMouseOver() ? 1.0f : 0.8f));
    g.drawRoundedRectangle (p, p.getHeight() * 0.5f, 1.0f);
    g.setFont (AeroLookAndFeel::uiFont (11.0f, true));
    g.setColour (T.ink);
    g.drawText (into > 0.5f ? "into the input" : "onto the output", p, Justification::centred);
}

void HydroControl::mouseDown (const MouseEvent& e)
{
    if (pill().contains (e.position)) { intoAtt.setValueAsCompleteGesture (into > 0.5f ? 0.0f : 1.0f); return; }
    if (capsule().contains (e.position)) { dragging = true; levelAtt.beginGesture(); mouseDrag (e); }
}
void HydroControl::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    const auto c = capsule();
    levelAtt.setValueAsPartOfGesture (jlimit (0.0f, 1.0f, (e.position.x - c.getX() - 2.0f) / (c.getWidth() - 4.0f)));
}
void HydroControl::mouseUp (const MouseEvent&) { if (dragging) { dragging = false; levelAtt.endGesture(); } }
void HydroControl::mouseDoubleClick (const MouseEvent& e) { if (capsule().contains (e.position)) levelAtt.setValueAsCompleteGesture (0.0f); }

//==============================================================================
// The water
float CloudfinEditor::surfaceY (float x) const noexcept
{
    const float H = (float) designH;
    const int n = (int) waveH.size();
    float h = 0.0f;
    if (n > 1)
    {
        const float u = jlimit (0.0f, (float) (n - 1), x / (float) designW * (float) (n - 1));
        const int i = jmin ((int) u, n - 2);
        h = waveH[(size_t) i] + (u - (float) i) * (waveH[(size_t) i + 1] - waveH[(size_t) i]);
    }
    const float swell = (2.0f + 5.0f * gravVis) * std::sin (x * 0.011f + t * 1.1f) + (1.5f + 3.0f * gravVis) * std::sin (x * 0.027f - t * 1.7f);
    return H - waterLevel + h + swell;
}

void CloudfinEditor::splash (float x, float strength)
{
    const int n = (int) waveV.size();
    const int i = jlimit (0, n - 1, roundToInt (x / (float) designW * (float) (n - 1)));
    for (int k = -2; k <= 2; ++k)
        if (isPositiveAndBelow (i + k, n)) waveV[(size_t) (i + k)] += strength * (k == 0 ? 1.0f : 0.5f);
    Random r;
    const int count = jlimit (2, 14, (int) (std::abs (strength) * 0.03f));
    for (int k = 0; k < count; ++k)
        droplets.push_back ({ { x + (r.nextFloat() - 0.5f) * 30.0f, surfaceY (x) - 2.0f },
                              { (r.nextFloat() - 0.5f) * 260.0f, -120.0f - r.nextFloat() * std::abs (strength) * 0.6f }, 1.0f });
    splashEnergy = jmin (1.0f, splashEnergy + std::abs (strength) * 0.0008f);
}

void CloudfinEditor::stepWater (float dt)
{
    const float target = gravityOn ? poolLevel : calmLevel;
    waterLevel += (target - waterLevel) * jmin (1.0f, dt * 1.2f);
    // the surface: neighbouring columns pull on each other, everything springs back, slowly losing energy
    const int n = (int) waveH.size();
    for (int sub = 0; sub < 4; ++sub)
    {
        const float h = dt / 4.0f;
        for (int i = 0; i < n; ++i)
        {
            const float l = waveH[(size_t) jmax (0, i - 1)], r = waveH[(size_t) jmin (n - 1, i + 1)];
            waveV[(size_t) i] += ((l + r - 2.0f * waveH[(size_t) i]) * 900.0f - waveH[(size_t) i] * 6.0f) * h;
            waveV[(size_t) i] *= 1.0f - 1.6f * h;
        }
        for (int i = 0; i < n; ++i) waveH[(size_t) i] = jlimit (-60.0f, 60.0f, waveH[(size_t) i] + waveV[(size_t) i] * h);
    }
    for (auto& d : droplets) { d.v.y += 1400.0f * dt; d.p += d.v * dt; d.life -= dt * 1.4f; }
    droplets.erase (std::remove_if (droplets.begin(), droplets.end(),
                                    [this] (const Droplet& d) { return d.life <= 0.0f || (d.v.y > 0.0f && d.p.y > surfaceY (d.p.x)); }), droplets.end());

    // how much it's sloshing, for the sound: splashes, plus the swell of things bobbing about
    float motion = 0.0f;
    for (auto v : waveV) motion += std::abs (v);
    motion = jlimit (0.0f, 1.0f, motion / ((float) jmax (1, n) * 40.0f));
    splashEnergy *= std::exp (-dt * 1.5f);
    proc.sloshEnergy.store (gravityOn ? jlimit (0.0f, 1.0f, 0.5f * motion + splashEnergy) : 0.0f, std::memory_order_relaxed);
}

void CloudfinEditor::drawWater (Graphics& g)
{
    const Theme& T = lnf.theme();
    const float H = (float) designH, W = (float) designW;
    Path surf;
    const int cols = 96;
    for (int i = 0; i <= cols; ++i)
    {
        const float x = W * (float) i / (float) cols;
        if (i == 0) surf.startNewSubPath (x, surfaceY (x)); else surf.lineTo (x, surfaceY (x));
    }
    Path body (surf);
    body.lineTo (W, H + 2.0f); body.lineTo (0.0f, H + 2.0f); body.closeSubPath();
    // flat translucent layers (much cheaper than a gradient this size): lighter near the top, deeper below
    g.setColour (T.aquaLight.interpolatedWith (T.ocean, 0.35f).withAlpha (0.5f));
    g.fillPath (body);
    if (waterLevel > 40.0f)
    {
        g.setColour (T.ocean.withAlpha (0.25f));
        g.fillRect (Rectangle<float> (0.0f, H - waterLevel * 0.55f, W, waterLevel * 0.55f + 2.0f));
    }
    // light wobbling through it (deep enough under the surface that it needs no clipping)
    if (waterLevel > 40.0f)
    {
        for (int k = 0; k < 4; ++k)
        {
            Path c;
            const float y0 = H - waterLevel * (0.25f + 0.1f * (float) k);
            for (int i = 0; i <= 30; ++i)
            {
                const float x = W * (float) i / 30.0f;
                const float y = y0 + 6.0f * std::sin (x * 0.02f + t * (0.8f + 0.1f * (float) k) + (float) k);
                if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
            }
            g.setColour (Colours::white.withAlpha (0.06f));
            g.strokePath (c, PathStrokeType (3.0f));
        }
        for (int k = 0; k < 14; ++k)   // bubbles rising
        {
            const float u = std::fmod (t * 0.12f + (float) k * 0.137f, 1.0f);
            const float x = std::fmod ((float) k * 233.0f + 40.0f * std::sin (t * 0.5f + (float) k), W);
            const float y = H - u * (waterLevel - 6.0f);
            g.setColour (Colours::white.withAlpha (0.35f * (1.0f - u)));
            g.drawEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ x, y }), 1.0f);
        }
    }
    // the surface: a bright line with a softer glow under it
    g.setColour (Colours::white.withAlpha (0.85f));
    g.strokePath (surf, PathStrokeType (1.6f));
    for (auto& d : droplets)
    {
        g.setColour (T.aquaLight.withAlpha (jlimit (0.0f, 1.0f, d.life)));
        g.fillEllipse (Rectangle<float> (4.0f, 5.0f).withCentre (d.p));
    }
}

void CloudfinEditor::paint (Graphics& g) { g.fillAll (lnf.theme().skyMid); }

void CloudfinEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
}

// Everything is laid out about the vertical centre line (x = 750): three rows of
// [side | centre | side], and each side panel has its mirror on the other side.
void CloudfinEditor::layoutContent()
{
    const bool tight = look::tune().compact;
    constexpr int pad = 14, cx = designW / 2;
    const int cellH = tight ? 118 : 136, top = tight ? 34 : 38;
    const int ph = tight ? 176 : 200, y1 = tight ? 106 : 112, y2 = y1 + ph + (tight ? 6 : 20), y3 = y2 + ph + (tight ? 6 : 20);
    auto placeRow = [&] (Panel* p, int x, int cellW = 84)
    {
        for (auto* k : p->knobs) { k->setBounds (x, p->r.getY() + top, cellW, cellH); x += cellW; }
    };
    auto placeCentred = [&] (Panel* p, int cellW = 84)
    {
        placeRow (p, p->r.getCentreX() - (int) p->knobs.size() * cellW / 2, cellW);
    };

    // header: the light and the presets on the left, the hydrophone and gravity on the right
    themeSwitch.setBounds (24, 19, 132, 30);
    presetBox.setBounds (172, 20, 250, 28);
    hydroCtl.setBounds (designW - 172 - 250, 18, 250, 32);
    gravityBtn.setBounds (designW - 24 - 132, 20, 132, 28);
    {
        const int stripW = (int) (fp::numStages * ChainStrip::chipW + (fp::numStages - 1) * ChainStrip::gap + 2.0f * ChainStrip::endW);
        chain.setBounds (cx - stripW / 2, 74, stripW, 28);
        shuffleBtn.setBounds (chain.getX() - 12 - 92, 75, 92, 26);
        randomBtn.setBounds (chain.getRight() + 12, 75, 92, 26);
    }

    // row 1: pitch | spectral smear | formant
    psP->r = { 20, y1, 364, ph };   placeRow (psP, psP->r.getX() + pad);
    scaleBox.setBounds (psP->r.getX() + 114, psP->r.getY() + 9, psP->r.getWidth() - 114 - 16, 24);
    sgP->r = { 400, y1, 700, ph };  placeCentred (sgP);
    specFreeze.setBounds (sgP->r.getRight() - 150, sgP->r.getY() + 8, 136, 26);
    fmP->r = { 1116, y1, 364, ph }; placeRow (fmP, fmP->r.getX() + pad);

    // row 2: feedback | grains | drift. The grain shape sits dead centre, the freeze bubble and
    // the mix on either side of it, then four knobs each way.
    fbP->r = { 20, y2, 190, ph };   placeCentred (fbP);
    grP->r = { 226, y2, 1048, ph };
    {
        constexpr int cell = 74, shapeW = 220, bigW = 96;
        const int y = grP->r.getY() + top;
        shapeEditor.setBounds (cx - shapeW / 2, y - 2, shapeW, tight ? 122 : 136);
        const int fx = shapeEditor.getX() - 8 - bigW;                     // the freeze bubble, left of the shape
        freeze.setBounds (fx, y, bigW, tight ? 88 : 100);
        randOct.setBounds (fx - 14, y + (tight ? 92 : 108), 132, 24);
        for (int i = 0; i < 4; ++i) grP->knobs[(size_t) i]->setBounds (fx - (4 - i) * cell, y, cell, cellH);
        const int mx = shapeEditor.getRight() + 8;                        // the mix, mirroring it
        grP->knobs[8]->setBounds (mx + (bigW - cell) / 2, y, cell, cellH);
        for (int i = 0; i < 4; ++i) grP->knobs[(size_t) (4 + i)]->setBounds (mx + bigW + i * cell, y, cell, cellH);
    }
    drP->r = { 1290, y2, 190, ph }; placeCentred (drP);

    // row 3: ji delay | compander | reverb. The compander has a gain meter at each end.
    dlP->r = { 20, y3, 364, ph };   placeRow (dlP, dlP->r.getX() + pad);
    cpP->r = { 400, y3, 700, ph };  placeCentred (cpP);
    {
        const int half = (int) cpP->knobs.size() * 84 / 2, y = cpP->r.getY() + top + 6;
        meterAreas[0] = { cx - half - 12 - 22, y, 22, tight ? 98 : 122 };
        meterAreas[1] = { cx + half + 12, y, 22, tight ? 98 : 122 };
    }
    rvP->r = { 1116, y3, 364, ph }; placeCentred (rvP);
    backLayer = frontLayer = {};
}

//==============================================================================
// Depth of field: a separable box blur (three passes ~ gaussian) for the cached
// background layer. Runs only when the layer is rebuilt, never per frame.
static void blurImage (Image& img, int radius)
{
    if (radius < 1 || ! img.isValid()) return;
    Image::BitmapData bd (img, Image::BitmapData::readWrite);
    const int w = bd.width, h = bd.height, ch = bd.pixelStride;
    std::vector<uint8> line ((size_t) jmax (w, h) * (size_t) ch);
    const int win = 2 * radius + 1;

    auto pass = [&] (uint8* start, int count, int stride)
    {
        for (int i = 0; i < count; ++i)
            std::memcpy (&line[(size_t) (i * ch)], start + (size_t) i * (size_t) stride, (size_t) ch);
        int sum[4] = {};
        for (int i = -radius; i <= radius; ++i)
        {
            const int j = jlimit (0, count - 1, i);
            for (int c = 0; c < ch; ++c) sum[c] += line[(size_t) (j * ch + c)];
        }
        for (int i = 0; i < count; ++i)
        {
            uint8* px = start + (size_t) i * (size_t) stride;
            for (int c = 0; c < ch; ++c) px[c] = (uint8) (sum[c] / win);
            const int add = jmin (i + radius + 1, count - 1), rem = jmax (i - radius, 0);
            for (int c = 0; c < ch; ++c) sum[c] += line[(size_t) (add * ch + c)] - line[(size_t) (rem * ch + c)];
        }
    };

    for (int it = 0; it < 3; ++it)
    {
        for (int y = 0; y < h; ++y) pass (bd.getLinePointer (y), w, bd.pixelStride);
        for (int x = 0; x < w; ++x) pass (bd.getPixelPointer (x, 0), h, bd.lineStride);
    }
}

// one out-of-focus disc of light, with the brighter rim real bokeh has
static void drawBokeh (Graphics& g, Point<float> c, float r, Colour col, float a)
{
    ColourGradient gr (col.withAlpha (a * 0.55f), c.x, c.y, col.withAlpha (0.0f), c.x + r, c.y, true);
    gr.addColour (0.80, col.withAlpha (a * 0.70f));
    gr.addColour (0.93, col.withAlpha (a));
    g.setGradientFill (gr);
    g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c));
}

//==============================================================================
// The clouds: cumulus heaped up from overlapping puffs on a flat base. Each is given by how far
// it sits from the centre line, and is drawn once on each side (the one on the line mirrors itself).
void CloudfinEditor::buildClouds()
{
    clouds.clear();
    Random rnd (11);
    auto make = [&] (float dx, float baseY, float w, float alpha)
    {
        Cloud c;
        c.dx = dx; c.baseY = baseY; c.w = w; c.alpha = alpha;
        const int n = 9 + rnd.nextInt (4);
        for (int i = 0; i < n; ++i)
        {
            const float u = (float) i / (float) (n - 1) * 2.0f - 1.0f;      // along the base
            const float hump = 1.0f - u * u;                                 // tallest in the middle
            const float r = w * (0.09f + 0.15f * hump) * (0.8f + 0.4f * rnd.nextFloat());
            c.puffs.push_back ({ u * w * 0.42f, -r * (0.5f + 0.5f * hump * rnd.nextFloat()), r });
        }
        for (int i = 0; i < 4; ++i)                                           // an upper storey
        {
            const float r = w * (0.11f + 0.07f * rnd.nextFloat());
            c.puffs.push_back ({ (rnd.nextFloat() - 0.5f) * w * 0.45f, -w * (0.17f + 0.09f * rnd.nextFloat()), r });
        }
        if (dx == 0.0f)
        {
            const auto half = c.puffs;
            for (auto& p : half) c.puffs.push_back ({ -p.x, p.y, p.r });
        }
        clouds.push_back (c);
    };
    // far ones along the horizon first, then the big near ones
    make (120.0f, 524.0f, 170.0f, 0.55f);
    make (365.0f, 516.0f, 230.0f, 0.62f);
    make (640.0f, 522.0f, 280.0f, 0.66f);
    make (0.0f,   330.0f, 330.0f, 0.92f);
    make (650.0f, 150.0f, 300.0f, 0.90f);
    make (300.0f, 196.0f, 250.0f, 0.95f);
    make (505.0f, 400.0f, 430.0f, 1.0f);
    make (175.0f, 452.0f, 210.0f, 0.95f);
}

void CloudfinEditor::renderStatic (float scale)
{
    staticScale = scale;
    constexpr int M = parallaxMargin;
    // the background is out of focus: render it at half resolution and blur it
    const float bs = scale * 0.5f;
    backLayer = Image (Image::ARGB, roundToInt ((designW + 2 * M) * bs), roundToInt ((designH + 2 * M) * bs), true);
    {
        Graphics g (backLayer);
        g.addTransform (AffineTransform::translation ((float) M, (float) M).scaled (bs));
        drawBackScene (g);
    }
    blurImage (backLayer, jmax (1, roundToInt (3.0f * bs)));
    frontLayer = Image (Image::ARGB, roundToInt (designW * scale), roundToInt (designH * scale), true);
    {
        Graphics g (frontLayer);
        g.addTransform (AffineTransform::scale (scale));
        drawFrontScene (g, false);
    }
    panelLayer = Image (Image::ARGB, roundToInt (designW * scale), roundToInt (designH * scale), true);
    {
        Graphics g (panelLayer);
        g.addTransform (AffineTransform::scale (scale));
        drawFrontScene (g, true);
    }
}

// Far away: sky, sun or moon, stars, clouds, and the sea under them. All of it is the same on
// both sides of the centre line. Drifts with the mouse.
void CloudfinEditor::drawBackScene (Graphics& g)
{
    const Theme& T = lnf.theme();
    constexpr float M = (float) parallaxMargin, cx = designW * 0.5f, hz = horizonY;
    const auto full = Rectangle<float> (-M, -M, designW + 2 * M, designH + 2 * M);

    // sky
    ColourGradient sky (T.skyTop, 0, full.getY(), T.skyLow, 0, hz + 40.0f, false);
    sky.addColour (T.mode == 0 ? 0.48 : 0.45, T.skyMid);
    if (T.mode != 0) sky.addColour (0.8, T.skyWarm);
    g.setGradientFill (sky);
    g.fillRect (full);

    Point<float> sun;
    Colour rayCol = Colours::white;
    if (T.mode == 0)
    {
        sun = { cx, 64 };
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.9f), sun.x, sun.y, Colours::white.withAlpha (0.0f), sun.x + 620, sun.y, true));
        g.fillRect (full);
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (84, 84).withCentre (sun));
    }
    else if (T.dusk)
    {
        sun = { cx, hz - 14.0f };
        rayCol = T.warm;
        g.setGradientFill (ColourGradient (Colour (0xffffe2b0).withAlpha (0.95f), sun.x, sun.y, Colour (0x00ffb070), sun.x + 640, sun.y, true));
        g.fillRect (full);
        g.setGradientFill (ColourGradient (Colour (0xfffff4d8), sun.x, sun.y - 30, Colour (0xffffb070), sun.x, sun.y + 60, false));
        g.fillEllipse (Rectangle<float> (120, 120).withCentre (sun));     // (the sea hides its lower half)
    }
    else
    {
        // stars, thinning toward the horizon
        Random rnd (42);
        for (int i = 0; i < 130; ++i)
        {
            const float x = rnd.nextFloat() * (cx + M);
            const float y = full.getY() + std::pow (rnd.nextFloat(), 1.4f) * (hz + M);
            const float sz = 0.5f + std::pow (rnd.nextFloat(), 3.0f) * 1.8f;
            const float a = (0.25f + 0.75f * rnd.nextFloat()) * (1.0f - jlimit (0.0f, 0.85f, y / hz));
            g.setColour ((rnd.nextFloat() < 0.15f ? Colour (0xffcfe0ff) : Colours::white).withAlpha (a));
            g.fillEllipse (cx - x - sz, y - sz, 2 * sz, 2 * sz);
            g.fillEllipse (cx + x - sz, y - sz, 2 * sz, 2 * sz);
        }
        // the moon, on the centre line
        sun = { cx, 84 };
        rayCol = Colour (0xffcfe6ff);
        g.setGradientFill (ColourGradient (Colour (0xffcfe6ff).withAlpha (0.45f), sun.x, sun.y, Colour (0x00cfe6ff), sun.x + 230, sun.y, true));
        g.fillEllipse (Rectangle<float> (460, 460).withCentre (sun));
        auto mr = Rectangle<float> (60, 60).withCentre (sun);
        g.setGradientFill (ColourGradient (Colours::white, sun.x, sun.y - 8, Colour (0xffc9d8f2), sun.x, mr.getBottom() + 10, true));
        g.fillEllipse (mr);
        g.setColour (Colour (0xff8ea3c9).withAlpha (0.25f)); // maria
        g.fillEllipse (Rectangle<float> (13, 10).withCentre ({ sun.x - 11, sun.y - 5 }));
        g.fillEllipse (Rectangle<float> (13, 10).withCentre ({ sun.x + 11, sun.y - 5 }));
        g.fillEllipse (Rectangle<float> (9, 8).withCentre ({ sun.x, sun.y + 12 }));
    }

    // shafts of light, fanning out evenly on both sides (down from a high sun or the moon, up from a low one)
    for (int i = 0; i < 4; ++i)
        for (float side : { -1.0f, 1.0f })
        {
            const float mid = (T.dusk ? -1.0f : 1.0f) * MathConstants<float>::halfPi;
            const float off = (T.dusk ? 0.34f : 0.16f) + (T.dusk ? 0.30f : 0.24f) * (float) i;
            const float wd = 0.05f + 0.03f * (float) (i % 2);
            const float a0 = mid + side * off, a1 = mid + side * (off + wd);
            Path ray;
            ray.startNewSubPath (sun);
            ray.lineTo (sun.x + 2400 * std::cos (a0), sun.y + 2400 * std::sin (a0));
            ray.lineTo (sun.x + 2400 * std::cos (a1), sun.y + 2400 * std::sin (a1));
            ray.closeSubPath();
            const float ra = T.mode == 0 ? 0.16f : (T.dusk ? 0.20f : 0.07f);
            g.setGradientFill (ColourGradient (rayCol.withAlpha (ra), sun.x, sun.y, rayCol.withAlpha (0.0f), sun.x + 1100, sun.y, true));
            g.fillPath (ray);
        }

    // one cloud: a shaded underside, then brighter and brighter layers heaped toward the light,
    // gloss on the biggest puffs, and the light caught along its flat base
    auto drawCloud = [&] (const Cloud& c, float side)
    {
        const Point<float> C (cx + side * c.dx, c.baseY);
        auto light = sun - C;
        light /= jmax (1.0e-4f, light.getDistanceFromOrigin());
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (Rectangle<float> (C.x - c.w, C.y - c.w, 2.0f * c.w, c.w + c.w * 0.035f).getSmallestIntegerContainer());
        g.setColour (T.cloudShade);
        for (auto& p : c.puffs)
            g.fillEllipse (Rectangle<float> (2.1f * p.r, 2.1f * p.r).withCentre ({ C.x + side * p.x, C.y + p.y + p.r * 0.08f }));
        const float widths[] = { 1.0f, 0.9f, 0.78f, 0.62f, 0.44f, 0.26f };
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (T.cloud[i]);
            for (auto& p : c.puffs)
            {
                const float r = p.r * widths[i];
                g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (Point<float> (C.x + side * p.x, C.y + p.y) + light * (p.r - r) * 0.7f));
            }
        }
        for (auto& p : c.puffs)
            if (p.r > c.w * 0.17f)
            {
                const auto h = Point<float> (C.x + side * p.x, C.y + p.y) + light * p.r * 0.5f;
                g.setGradientFill (ColourGradient (Colours::white.withAlpha (T.dark ? 0.30f : 0.75f), h.x, h.y, Colours::white.withAlpha (0.0f), h.x + p.r * 0.5f, h.y, true));
                g.fillEllipse (Rectangle<float> (p.r, p.r).withCentre (h));
            }
        Path body;
        for (auto& p : c.puffs) body.addEllipse (Rectangle<float> (2 * p.r, 2 * p.r).withCentre ({ C.x + side * p.x, C.y + p.y }));
        g.reduceClipRegion (body);
        const auto under = T.mode == 0 ? T.cloudShade.withAlpha (0.55f) : T.cloudGlowA.withAlpha (T.dark ? 0.30f : 0.55f);
        g.setGradientFill (ColourGradient (under, C.x, C.y, under.withAlpha (0.0f), C.x, C.y - c.w * 0.16f, false));
        g.fillRect (Rectangle<float> (C.x - c.w, C.y - c.w * 0.16f, 2.0f * c.w, c.w * 0.2f));
    };
    auto drawClouds = [&] (float alphaScale)
    {
        for (auto& c : clouds)
        {
            g.beginTransparencyLayer (c.alpha * alphaScale);
            drawCloud (c, 1.0f);
            if (c.dx != 0.0f) drawCloud (c, -1.0f);
            g.endTransparencyLayer();
        }
    };
    drawClouds (1.0f);

    // the sea: calm, lighter at the horizon, with the clouds lying in it
    const auto sea = full.withTop (hz);
    const auto seaTop = T.mode == 0 ? T.aquaLight.interpolatedWith (T.skyLow, 0.35f)
                      : T.dusk     ? T.skyLow.interpolatedWith (T.aqua, 0.30f)
                                   : T.skyLow.interpolatedWith (T.ocean, 0.45f).darker (0.25f);
    g.setGradientFill (ColourGradient (seaTop, 0, hz, T.dark ? T.ocean.darker (1.2f) : T.ocean, 0, full.getBottom(), false));
    g.fillRect (sea);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (sea.getSmallestIntegerContainer());
        g.addTransform (AffineTransform::translation (0, -hz).scaled (1.0f, -0.55f).translated (0, hz));
        drawClouds (T.dark ? 0.22f : 0.34f);
    }
    // haze on the horizon
    const auto haze = (T.mode == 0 ? Colours::white : (T.dusk ? Colour (0xffffe2c0) : Colour (0xff8fc8ff))).withAlpha (T.dark ? 0.22f : 0.6f);
    g.setGradientFill (ColourGradient (haze, 0, hz, haze.withAlpha (0.0f), 0, hz - 34.0f, false));
    g.fillRect (full.getX(), hz - 34.0f, full.getWidth(), 34.0f);
    g.setGradientFill (ColourGradient (haze, 0, hz, haze.withAlpha (0.0f), 0, hz + 26.0f, false));
    g.fillRect (full.getX(), hz, full.getWidth(), 26.0f);

    // the sun's (or the moon's) path down the middle of the water, and long quiet ripples either side
    {
        Random rnd (5);
        const float depth = full.getBottom() - hz;
        for (int i = 0; i < 70; ++i)
        {
            const float v = std::pow (rnd.nextFloat(), 1.3f);
            const float y = hz + 3.0f + v * depth;
            const float off = rnd.nextFloat() * (10.0f + v * 90.0f), w = 8.0f + v * 60.0f * rnd.nextFloat(), h = 1.4f + 3.0f * v;
            g.setColour (rayCol.withAlpha ((0.25f + 0.55f * rnd.nextFloat()) * (T.dark ? 0.55f : 0.85f)));
            g.fillEllipse (Rectangle<float> (w, h).withCentre ({ cx - off, y }));
            g.fillEllipse (Rectangle<float> (w, h).withCentre ({ cx + off, y }));
        }
        for (int i = 0; i < 46; ++i)
        {
            const float v = std::pow (rnd.nextFloat(), 1.2f);
            const float y = hz + 5.0f + v * depth;
            const float off = 60.0f + rnd.nextFloat() * (cx + M), w = 40.0f + v * 190.0f, h = 1.2f + 2.6f * v;
            g.setColour (Colours::white.withAlpha ((0.06f + 0.14f * rnd.nextFloat()) * (T.dark ? 0.5f : 1.0f)));
            g.fillEllipse (Rectangle<float> (w, h).withCentre ({ cx - off, y }));
            g.fillEllipse (Rectangle<float> (w, h).withCentre ({ cx + off, y }));
        }
    }
}

// Near: the wordmark and the glass panels. Fixed in place.
void CloudfinEditor::drawFrontScene (Graphics& g, bool panelsPart)
{
    const Theme& T = lnf.theme();

    // wordmark, extruded
    if (! panelsPart)
    {
        GlyphArrangement ga;
        ga.addLineOfText (AeroLookAndFeel::titleFont (56.0f), "Cloudfin", 0, 0);
        Path tp;
        ga.createPath (tp);
        const auto b = tp.getBounds();
        tp.applyTransform (AffineTransform::translation (designW * 0.5f - b.getCentreX(), 38.0f - b.getCentreY()));
        DropShadow (T.dark ? Colour (0x99000000) : (T.dusk ? Colour (0x66503a8a) : Colour (0x663a8fc4)), 18, { 0, 9 }).drawForPath (g, tp);
        for (int i = 7; i >= 1; --i)
        {
            g.setColour (T.wordExtrude.interpolatedWith (T.wordC, (float) (7 - i) / 14.0f));
            g.fillPath (tp, AffineTransform::translation ((float) i * 0.35f, (float) i * 0.85f));
        }
        const auto tb = tp.getBounds();
        ColourGradient fill (T.wordA, 0, tb.getY(), T.wordC, 0, tb.getBottom(), false);
        fill.addColour (0.5, T.wordB);
        g.setGradientFill (fill);
        g.fillPath (tp);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (tp);
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.75f), 0, tb.getY(), Colours::white.withAlpha (0.1f), 0, tb.getCentreY(), false));
            g.fillRect (tb.withHeight (tb.getHeight() * 0.5f));
        }
        g.setColour (Colours::white.withAlpha (T.dark ? 0.8f : 1.0f));
        g.strokePath (tp, PathStrokeType (1.6f));
    }

    if (! panelsPart) return;
    for (auto& p : panels)
        drawPanelGlass (g, p);

    // warm apertures: pools of late sunlight falling through gaps in the clouds onto the glass (in mirrored pairs)
    if (T.dusk)
    {
        const float spots[][3] = { { 490, 210, 190 }, { 230, 430, 170 }, { 150, 660, 210 }, { 640, 240, 150 }, { 0, 640, 220 } };
        for (auto& sp : spots)
            for (float side : { -1.0f, 1.0f })
            {
                const float x = designW * 0.5f + side * sp[0];
                g.setGradientFill (ColourGradient (Colour (0xffffc98a).withAlpha (sp[0] == 0 ? 0.11f : 0.22f), x, sp[1],
                                                   Colour (0x00ffb070), x + sp[2], sp[1], true));
                g.fillEllipse (Rectangle<float> (2 * sp[2], 2 * sp[2]).withCentre ({ x, sp[1] }));
            }
    }
    // moonlight on the glass
    if (T.dark)
    {
        const float spots[][3] = { { 0, 200, 260 }, { 420, 400, 200 }, { 600, 620, 180 } };
        for (auto& sp : spots)
            for (float side : { -1.0f, 1.0f })
            {
                const float x = designW * 0.5f + side * sp[0];
                g.setGradientFill (ColourGradient (Colour (0xffbfe0ff).withAlpha (sp[0] == 0 ? 0.05f : 0.10f), x, sp[1],
                                                   Colour (0x00bfe0ff), x + sp[2], sp[1], true));
                g.fillEllipse (Rectangle<float> (2 * sp[2], 2 * sp[2]).withCentre ({ x, sp[1] }));
            }
    }
}

// A thick slab of frosted glass: two-layer shadow, a visible edge underneath,
// bevelled rim, and a recessed tray the knobs float above.
void CloudfinEditor::drawPanelGlass (Graphics& g, const Panel& p)
{
    const Theme& T = lnf.theme();
    auto rf = p.r.toFloat();
    const float rad = 22.0f, slab = 6.0f;
    Path shape;
    shape.addRoundedRectangle (rf, rad);
    Path slabShape;
    slabShape.addRoundedRectangle (rf.translated (0, slab), rad);

    const auto shCol = T.dark ? Colour (0xff000310) : (T.dusk ? Colour (0xff3a2f6a) : Colour (0xff246c9c));
    DropShadow (shCol.withAlpha (T.dark ? 0.55f : 0.28f), 36, { 0, 20 }).drawForPath (g, slabShape);
    DropShadow (shCol.withAlpha (T.dark ? 0.6f : 0.30f), 8, { 0, 3 }).drawForPath (g, slabShape);

    // the glass edge, seen below the face
    g.setGradientFill (ColourGradient (T.panelSlab.brighter (0.2f), 0, rf.getBottom() - 10, T.panelSlab, 0, rf.getBottom() + slab, false));
    g.fillPath (slabShape);
    g.setColour (T.panelEdge.withMultipliedAlpha (0.5f));
    g.strokePath (slabShape, PathStrokeType (1.0f));

    // the face
    g.setGradientFill (ColourGradient (T.panelTop, 0, rf.getY(), T.panelBottom, 0, rf.getBottom(), false));
    g.fillPath (shape);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        auto band = rf.withHeight (rf.getHeight() * 0.42f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (T.gloss), 0, band.getY(), Colours::white.withAlpha (0.0f), 0, band.getBottom(), false));
        g.fillRoundedRectangle (band.reduced (2.0f, 1.0f), rad);
        g.setGradientFill (ColourGradient (T.panelGlow, rf.getCentreX(), rf.getBottom() + 30.0f,
                                           Colours::transparentWhite, rf.getCentreX(), rf.getBottom() - rf.getHeight() * 0.7f, true));
        g.fillRect (rf);

        // recessed tray behind the controls
        auto tray = Rectangle<float> (rf.getX() + 9.0f, rf.getY() + 37.0f, rf.getWidth() - 18.0f, 140.0f);
        Path trayPath;
        trayPath.addRoundedRectangle (tray, 16.0f);
        g.setColour ((T.dark ? Colours::black : Colour (0xff1d4f7a)).withAlpha (T.dark ? 0.22f : 0.06f));
        g.fillPath (trayPath);
        {
            Graphics::ScopedSaveState s2 (g);
            g.reduceClipRegion (trayPath);
            g.setColour (Colours::black.withAlpha (T.dark ? 0.35f : 0.10f));       // inner shadow, top
            g.drawRoundedRectangle (tray.translated (0, 3.0f).expanded (1.5f), 16.0f, 5.0f);
        }
        g.setColour (Colours::white.withAlpha (T.dark ? 0.12f : 0.55f));          // lit lower lip
        g.drawRoundedRectangle (tray.translated (0, 1.0f), 16.0f, 1.0f);
    }

    // bevel: bright upper rim, shaded lower rim
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (rf.withHeight (rf.getHeight() * 0.5f).toNearestInt());
        g.setColour (Colours::white.withAlpha (T.dark ? 0.35f : 0.9f));
        g.drawRoundedRectangle (rf.reduced (1.5f), rad - 1.5f, 2.0f);
    }
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (rf.withTrimmedTop (rf.getHeight() * 0.5f).toNearestInt());
        g.setColour (shCol.withAlpha (T.dark ? 0.5f : 0.16f));
        g.drawRoundedRectangle (rf.reduced (1.5f), rad - 1.5f, 2.0f);
    }
    g.setColour (T.panelEdge);
    g.strokePath (shape, PathStrokeType (1.2f));

    // the title: on the left, or on the right for the panels right of the centre (their mirror image)
    const bool right = mirrored (p);
    auto tr = p.r.reduced (18, 0).withHeight (34).translated (0, 4);
    if (p.stage >= 0) tr = right ? tr.withTrimmedRight (28) : tr.withTrimmedLeft (28); // room for the order badge
    g.setFont (AeroLookAndFeel::titleFont (19.0f));
    g.setColour (T.titleShadow);
    g.drawText (p.title, tr.translated (0, 1), right ? Justification::centredRight : Justification::centredLeft);
    g.setColour (T.deep);
    g.drawText (p.title, tr, right ? Justification::centredRight : Justification::centredLeft);
    g.setFont (AeroLookAndFeel::uiFont (11.0f, true));
    g.setColour (look::text (T, T.mist));
    g.drawText (p.sub, tr, right ? Justification::centredLeft : Justification::centredRight);
}

// What moves in the far layer: a slow glow passing through each cloud (the same on both sides),
// and glints coming and going on the water.
void CloudfinEditor::drawSkyLights (Graphics& g, float alpha)
{
    const Theme& T = lnf.theme();
    constexpr float cx = designW * 0.5f, hz = horizonY;
    if (T.dark) alpha *= 1.5f; // they glow at night

    int k = 0;
    for (auto& c : clouds)
    {
        const float breathe = 0.5f + 0.5f * std::sin (t * (0.21f + 0.04f * (float) (k % 4)) + (float) k * 1.9f);
        const float sway = c.w * 0.22f * std::sin (t * 0.11f + (float) k);
        const auto col = (k % 3 == 0) ? T.glintB : T.glintA;
        const float rr = c.w * (0.34f + 0.08f * breathe);
        for (float side : { -1.0f, 1.0f })
        {
            if (side < 0.0f && c.dx == 0.0f) break;
            const Point<float> p (cx + side * (c.dx + sway), c.baseY - c.w * 0.14f);
            g.setGradientFill (ColourGradient (col.withAlpha (jmin (1.0f, 0.30f * alpha * c.alpha * (0.35f + 0.65f * breathe))), p.x, p.y, col.withAlpha (0.0f), p.x + rr, p.y, true));
            g.fillEllipse (Rectangle<float> (2 * rr, 2 * rr).withCentre (p));
        }
        ++k;
    }

    const auto glint = T.mode == 0 ? Colours::white : T.glint;
    for (int i = 0; i < 30; ++i)
    {
        const float fi = (float) i;
        const float v = std::pow (frac (fi * 0.618034f + 0.21f), 1.3f);
        const float y = hz + 6.0f + v * ((float) designH - hz);
        const float off = frac (fi * 0.414214f + 0.4f) * (i < 12 ? 30.0f + v * 110.0f : (float) designW * 0.5f);
        const float tw = std::pow (jmax (0.0f, std::sin (t * (0.9f + 0.17f * (float) (i % 5)) + fi * 2.3f)), 3.0f);
        if (tw < 0.03f) continue;
        const float w = (10.0f + 34.0f * v) * (0.6f + 0.4f * tw), h = 1.6f + 2.4f * v;
        g.setColour (glint.withAlpha (jmin (1.0f, 0.7f * tw * alpha)));
        g.fillEllipse (Rectangle<float> (w, h).withCentre ({ cx - off, y }));
        g.fillEllipse (Rectangle<float> (w, h).withCentre ({ cx + off, y }));
    }
}

void CloudfinEditor::drawAmbient (Graphics& g)
{
    const Theme& T = lnf.theme();
    const float span = (float) designH + 90.0f;
    for (auto& b : ambient)
        for (float side : { -1.0f, 1.0f })   // each bubble has a twin across the centre line
        {
            const float y = (float) designH + 45.0f - frac ((t * b.speed + b.offset) / span) * span;
            const float x = designW * 0.5f + side * (b.x * 0.5f + 16.0f * std::sin (t * 0.4f + b.phase));
            auto r = Rectangle<float> (b.size, b.size).withCentre ({ x, y });
            const float a = jmap (b.size, 5.0f, 31.0f, 0.9f, 0.55f);
            const auto rim = T.dusk ? T.warm : (T.dark ? T.aqua : T.aquaLight);
            if (T.dark)
            {
                g.setGradientFill (ColourGradient (T.aqua.withAlpha (0.25f * a), x, y, T.aqua.withAlpha (0.0f), x + b.size, y, true));
                g.fillEllipse (r.expanded (b.size * 0.5f));
            }
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f * a), r.getCentreX(), r.getCentreY(),
                                               rim.withAlpha (0.45f * a), r.getRight(), r.getCentreY(), true));
            g.fillEllipse (r);
            g.setColour (Colours::white.withAlpha ((T.dark ? 0.55f : 0.8f) * a));
            g.drawEllipse (r, 0.9f);
            g.setColour (Colours::white.withAlpha (0.85f * a));
            g.fillEllipse (Rectangle<float> (b.size * 0.3f, b.size * 0.2f).withCentre ({ x, r.getY() + b.size * 0.24f }));
        }
}

// Out-of-focus light. Background discs sit behind the glass and drift with the far
// layer; foreground discs float in front of everything, big and faint, drifting the
// other way (they're nearer than the plane of focus).
void CloudfinEditor::drawBokehLayer (Graphics& g, bool foreground)
{
    const Theme& T = lnf.theme();
    Colour cols[3];
    if (T.mode == 0)  { cols[0] = Colours::white; cols[1] = Colour (0xffbff0ff); cols[2] = Colour (0xfffff3c8); }
    else if (T.dusk)  { cols[0] = Colour (0xffffd08a); cols[1] = Colour (0xffffb0a0); cols[2] = Colour (0xffd8c8ff); }
    else              { cols[0] = Colour (0xff5fe0ff); cols[1] = Colour (0xffa58cff); cols[2] = Colour (0xffe8f4ff); }

    const int n = foreground ? 4 : 13;   // (each drawn on both sides of the centre line)
    for (int i = 0; i < n; ++i)
    {
        const float fi = (float) i + (foreground ? 100.0f : 0.0f);
        const float bx = designW * 0.5f + frac (fi * 0.618034f + 0.13f) * (designW * 0.5f + 100.0f);
        const float by = frac (fi * 0.414214f + 0.57f) * (designH + 160.0f) - 80.0f;
        const float drift = foreground ? 14.0f : 9.0f;
        const Point<float> c (bx + drift * std::sin (t * (0.05f + 0.011f * (float) (i % 5)) + fi),
                              by + drift * std::cos (t * (0.04f + 0.009f * (float) (i % 7)) + fi * 1.3f));
        const float pulse = 0.75f + 0.25f * std::sin (t * (0.3f + 0.05f * (float) (i % 4)) + fi * 2.1f);
        if (foreground)
        {
            const float r = 55.0f + frac (fi * 0.7548f) * 95.0f;
            // under gravity the biggest ones fade away (the water's the thing to look at, and they're the costliest to draw)
            const float keep = r > 110.0f ? 1.0f - gravVis : 1.0f;
            if (keep > 0.02f)
                for (auto pt : { c, mirrorOf (c) }) drawBokeh (g, pt, r, cols[i % 3], (T.dark ? 0.09f : 0.11f) * pulse * keep);
        }
        else
        {
            const float r = 10.0f + std::pow (frac (fi * 0.7548f), 1.5f) * 42.0f;
            for (auto pt : { c, mirrorOf (c) }) drawBokeh (g, pt, r, cols[i % 3], (T.mode == 0 ? 0.30f : (T.dusk ? 0.32f : 0.36f)) * pulse);
        }
    }
}

// sparkles in the sky layer: golden glints at dusk, twinkling stars at night
void CloudfinEditor::drawStars (Graphics& g)
{
    const Theme& T = lnf.theme();
    if (T.mode == 0) return;
    const int count = T.dark ? 12 : 7;   // (each has a twin across the centre line)
    for (int i = 0; i < count; ++i)
      for (float side : { -1.0f, 1.0f })
    {
        const float px = designW * 0.5f + side * frac ((float) i * 0.618f + 0.11f) * designW * 0.5f;
        const float py = T.dark ? 8.0f + frac ((float) i * 0.381f + 0.27f) * (designH * 0.55f)
                                : 110.0f + frac ((float) i * 0.381f + 0.27f) * (designH - 140.0f);
        const float tw = 0.5f + 0.5f * std::sin (t * (0.6f + 0.13f * (float) i) + (float) i * 1.7f);
        if (T.dark)
        {
            const float L = 3.0f + 6.0f * tw;
            g.setColour (Colours::white.withAlpha (0.35f + 0.6f * tw));
            g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre ({ px, py }));
            g.setColour (Colour (0xffcfe6ff).withAlpha (0.5f * tw));
            g.drawLine (px - L, py, px + L, py, 0.8f);
            g.drawLine (px, py - L, px, py + L, 0.8f);
        }
        else
        {
            const float rr = 6.0f + 10.0f * tw;
            g.setGradientFill (ColourGradient (Colour (0xffffe6b8).withAlpha (0.55f * tw), px, py, Colour (0x00ffd090), px + rr, py, true));
            g.fillEllipse (Rectangle<float> (2 * rr, 2 * rr).withCentre ({ px, py }));
        }
    }
}

float CloudfinEditor::modValue (const char* id) const
{
    const int i = modIndexOf (id);
    auto* prm = proc.apvts.getParameter (id);
    if (prm == nullptr) return 0.0f;
    if (i < 0) return prm->convertFrom0to1 (prm->getValue());
    return prm->convertFrom0to1 (proc.modNorm[(size_t) i]->load());
}

void CloudfinEditor::drawDynamic (Graphics& g)
{
    const Theme& T = lnf.theme();
    auto info = [&] (Panel* p, const String& s)
    {
        g.setFont (AeroLookAndFeel::uiFont (11.5f, true));
        g.setColour (look::text (T, T.deep.withAlpha (0.75f)));
        g.drawText (s, p->r.reduced (14, 0).removeFromBottom (25), Justification::centred);
    };

    // order badges: where each module sits in the chain right now
    {
        const auto order = proc.getOrder();
        for (auto& p : panels)
        {
            if (p.stage < 0) continue;
            int pos = 0;
            for (int i = 0; i < fp::numStages; ++i) if (order[(size_t) i] == p.stage) pos = i;
            auto b = Rectangle<float> (22.0f, 22.0f).withCentre ({ mirrored (p) ? (float) p.r.getRight() - 29.0f : (float) p.r.getX() + 29.0f, (float) p.r.getY() + 21.0f });
            const auto c = stageColour (p.stage);
            g.setColour (c.darker (0.6f));
            g.fillEllipse (b.translated (0, 2.0f));
            g.setGradientFill (ColourGradient (c.interpolatedWith (Colours::white, 0.45f), 0, b.getY(), c.darker (0.15f), 0, b.getBottom(), false));
            g.fillEllipse (b);
            g.setColour (Colours::white);
            g.drawEllipse (b, 1.3f);
            g.setFont (AeroLookAndFeel::uiFont (12.5f, true));
            g.setColour (T.ink);
            g.drawText (String (pos + 1), b, Justification::centred);
        }
    }

    const auto& scales = fp::getScales();
    const int si = jlimit (0, (int) scales.size() - 1, (int) proc.apvts.getRawParameterValue (ids::psScale)->load());
    const auto& sc = scales[(size_t) si];
    const auto q = fp::quantise (sc, (double) modValue (ids::psShift) * 100.0);

    {
        const float sf = proc.spectralFreezeAmount.load();
        info (sgP, sf >= 1.0f ? "spectral freeze: holding the spectrum, forever"
                 : sf > 0.0f ? (specFreeze.getToggleState() ? "freezing the spectrum..." : "letting it go...")
                 : "below the cutoff each bin fades; whatever gets through rings on for the smear time");
    }

    {
        String periodText;
        if (q.period != 0)
            periodText = (q.period > 0 ? "  +" : "  ") + String (q.period) + (std::abs (sc.period - 1200.0) < 1.0 ? " oct" : " tritave");
        info (psP, "lands on  " + String (sc.degrees[(size_t) q.degreeIndex].label) + periodText
                   + "   (" + (q.cents >= 0 ? "+" : "") + String (q.cents, 1) + " c)");
    }

    {
        const float v = modValue (ids::fmVowel), sh = modValue (ids::fmShift);
        const int i = jlimit (0, 4, (int) v);
        const float tt = v - (float) i;
        String vow = tt < 0.1f || i == 4 ? String (fp::FormantFilter::vowelName (i))
                   : tt > 0.9f ? String (fp::FormantFilter::vowelName (i + 1))
                   : String (fp::FormantFilter::vowelName (i)) + String (CharPointer_UTF8 (" \xe2\x86\x92 ")) + fp::FormantFilter::vowelName (i + 1);
        info (fmP, "singing  \"" + vow + "\"" + sep + (sh >= 0 ? "+" : "") + String (sh, 1) + " st throat");
    }

    {
        const float f = proc.freezeAmount.load();
        info (grP, f <= 0.0f ? "listening (live)" : f >= 1.0f ? "frozen" : (freeze.getToggleState() ? "freezing..." : "thawing..."));
    }

    // delay taps (just intonation from the scale, from the degree the shift lands on)
    {
        const auto tr = fp::delayTapRatios (sc, q.degreeIndex);
        const float Tm = modValue (ids::dlTime);
        auto ms = [&] (int k) { return String (roundToInt (Tm * (float) std::min (4.0, tr.ratio[k]))); };
        info (dlP, "L  " + ms (0) + " / " + ms (2) + " / " + ms (4) + "     R  " + ms (1) + " / " + ms (3) + " / " + ms (5) + "  ms");
        auto tr2 = dlP->r.reduced (18, 0).withHeight (34).translated (0, 4).withTrimmedLeft (110);
        g.setFont (AeroLookAndFeel::uiFont (10.5f, true));
        g.setColour (look::text (T, T.mist));
        g.drawText ("L " + String (tr.label[0]) + " " + tr.label[2] + " " + tr.label[4] + "   R " + tr.label[1] + " " + tr.label[3] + " " + tr.label[5],
                    tr2, Justification::centredRight);
    }

    {
        const float tn = modValue (ids::rvTone);
        auto hz = [] (float f) { return f < 1000.0f ? String (roundToInt (f)) : String (f * 0.001f, 1) + "k"; };
        info (rvP, "hp " + hz (fp::FDNReverb::highpassHz (tn)) + "  " + chevGlyph + "  lp " + hz (fp::FDNReverb::lowpassHz (tn)) + " Hz");
    }
    info (cpP, String ("below: lift (") + minusGlyph + ") makes feedback bloom, sink (+) dries it out");
    info (fbP, String ("out ") + loopGlyph + " in");
    info (drP, "a random voltage per knob");

    // compander gain meters: a little glass column at each end of the row, both showing the same thing
    for (auto& area : meterAreas)
    {
        auto m = area.toFloat();
        const float rad = m.getWidth() * 0.5f;
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.85f), m.getX(), 0, T.cloud[0], m.getRight(), 0, false));
        g.fillRoundedRectangle (m, rad);
        const float gdb = jlimit (-24.0f, 24.0f, proc.compGainDb.load());
        const float cy = m.getCentreY();
        const float h = gdb / 24.0f * (m.getHeight() * 0.5f - 3.0f);
        auto bar = gdb < 0 ? Rectangle<float> (m.getX() + 4, cy, m.getWidth() - 8, -h)
                           : Rectangle<float> (m.getX() + 4, cy - h, m.getWidth() - 8, h);
        g.setColour (gdb < 0 ? T.aqua : T.violet);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (Colours::white);
        g.drawRoundedRectangle (m, rad, 1.2f);
        g.setColour (look::text (T, T.deep.withAlpha (0.5f)));
        g.fillRect (m.getX() + 2, cy - 0.5f, m.getWidth() - 4, 1.0f);
        g.setFont (AeroLookAndFeel::uiFont (9.5f, true));
        g.drawText (String (roundToInt (gdb)) + " dB", Rectangle<float> (m.getCentreX() - 22, m.getBottom() + 1, 44, 12), Justification::centred);
    }
}

void CloudfinEditor::paintContent (Graphics& g)
{
    const float k = jlimit (1.0f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    pixelScale = k;
    FloatKnob::cacheScale = k;
    FloatKnob::fastBlit = gravVis > 0.5f;
    if (! backLayer.isValid() || ! frontLayer.isValid() || ! panelLayer.isValid() || std::abs (k - staticScale) > 0.05f)
        renderStatic (k);

    // far: sky, clouds and sea (drifting with the mouse: parallax), stars, light in the clouds and on the water, air bubbles.
    // It's all soft and far away, so on a retina screen it's composed at 1x and scaled up: a quarter of the work.
    const auto far = parallax;
    auto drawFar = [&] (Graphics& fg)
    {
        fg.drawImageTransformed (backLayer, AffineTransform::scale (2.0f / staticScale)
                                                .translated (far.x - (float) parallaxMargin, far.y - (float) parallaxMargin));
        {
            Graphics::ScopedSaveState ss (fg);
            fg.addTransform (AffineTransform::translation (far));
            drawStars (fg);
            drawBokehLayer (fg, false);
            drawSkyLights (fg, 1.0f);
        }
        Graphics::ScopedSaveState ss (fg);   // middle: air bubbles, a bit nearer so they drift more
        fg.addTransform (AffineTransform::translation (far * 0.45f));
        drawAmbient (fg);
    };
    if (k > 1.2f && composeFarAt1x)
    {
        // under gravity it's refreshed every other frame (it all moves slowly), and shown every frame
        const bool fresh = ! farLayer.isValid();
        if (fresh) farLayer = Image (Image::ARGB, designW, designH, false);
        if (fresh || gravVis < 0.5f || (++farFrame & 1) == 0)
        {
            Graphics fg (farLayer);
            drawFar (fg);
        }
        g.setImageResamplingQuality (Graphics::lowResamplingQuality);   // pixel-doubled: it's all soft anyway
        g.drawImageAt (farLayer, 0, 0);
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
    }
    else drawFar (g);

    // near: the glass, fixed; the frames fade away when gravity's on. These are pixel-for-pixel, so no smoothing needed.
    g.setImageResamplingQuality (Graphics::lowResamplingQuality);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (designW / 2 - 260, 0, 520, 92);   // the wordmark's layer is empty everywhere else
        g.drawImageTransformed (frontLayer, AffineTransform::scale (1.0f / staticScale));
    }
    if (gravVis < 0.99f)
    {
        g.setOpacity (1.0f - gravVis);
        g.drawImageTransformed (panelLayer, AffineTransform::scale (1.0f / staticScale));
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        if (gravVis > 0.01f) g.beginTransparencyLayer (1.0f - gravVis);
        drawDynamic (g);
        if (gravVis > 0.01f) g.endTransparencyLayer();
    }
}

void CloudfinEditor::paintForeground (Graphics& g)
{
    Graphics::ScopedSaveState ss (g);
    drawWater (g);
    g.addTransform (AffineTransform::translation (parallax * -1.8f));
    drawBokehLayer (g, true);
}

//==============================================================================
// Gravity: discs with collisions, walls, rolling, and inertia when the window moves.
void CloudfinEditor::stepPhysics (float dt)
{
    const float W = (float) designW, H = (float) designH;
    const int n = (int) bodies.size();

    if (gravityOn)
    {
        // the window moved: everything inside lags behind, then tumbles
        const auto sp = getScreenPosition();
        if (haveScreenPos && dt > 0.0f)
        {
            const float s = jmax (0.1f, (float) getWidth() / W);
            const auto d = (sp - lastScreenPos).toFloat() / s;
            if (d.x != 0.0f || d.y != 0.0f)
                for (auto& b : bodies) b.vel -= d / dt * 0.85f;
        }
        lastScreenPos = sp;
        haveScreenPos = true;

        std::vector<char> held ((size_t) n);
        for (int i = 0; i < n; ++i) held[(size_t) i] = bodies[(size_t) i].comp->isMouseOverOrDragging (true) ? 1 : 0;

        constexpr int sub = 8;
        constexpr float G = 2400.0f, maxV = 1600.0f;
        const float h = dt / (float) sub;
        std::vector<char> contact ((size_t) n);
        for (int step = 0; step < sub; ++step)
        {
            std::fill (contact.begin(), contact.end(), 0);
            if (lastDepth.size() != (size_t) n) lastDepth.assign ((size_t) n, 0.0f);
            for (int i = 0; i < n; ++i)
            {
                auto& b = bodies[(size_t) i];
                if (held[(size_t) i]) { b.vel = {}; b.spin = 0.0f; continue; }
                // the water holds them up: buoyancy by how much of the ball is under, and drag in it
                const float depth = b.pos.y + b.r - surfaceY (b.pos.x);
                const float under = jlimit (0.0f, 1.0f, depth / (2.0f * b.r));
                if (lastDepth[(size_t) i] <= 0.0f && depth > 0.0f && b.vel.y > 150.0f) splash (b.pos.x, b.vel.y * 0.35f);
                lastDepth[(size_t) i] = depth;
                if (under > 0.0f)
                {
                    b.vel.y -= G * 1.75f * under * h;
                    b.vel *= 1.0f - 2.6f * under * h;
                    // and the waves push them about
                    const float slope = (surfaceY (b.pos.x + 8.0f) - surfaceY (b.pos.x - 8.0f)) / 16.0f;
                    b.vel.x -= slope * 900.0f * under * h;
                    const int col = jlimit (0, (int) waveV.size() - 1, roundToInt (b.pos.x / W * (float) (waveV.size() - 1)));
                    waveV[(size_t) col] += b.vel.y * 0.05f * under * h * 60.0f;
                }
                b.vel.y += G * h;
                b.vel *= 1.0f - 0.12f * h;
                b.vel.x = jlimit (-maxV, maxV, b.vel.x);
                b.vel.y = jlimit (-maxV, maxV, b.vel.y);
                b.pos += b.vel * h;
            }

            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j)
                {
                    auto& a = bodies[(size_t) i];
                    auto& b = bodies[(size_t) j];
                    const auto d = b.pos - a.pos;
                    const float R = a.r + b.r;
                    const float dist2 = d.x * d.x + d.y * d.y;
                    if (dist2 >= R * R || dist2 < 1.0e-6f) continue;
                    const float wa = held[(size_t) i] ? 0.0f : 1.0f, wb = held[(size_t) j] ? 0.0f : 1.0f;
                    const float ws = wa + wb;
                    if (ws == 0.0f) continue;
                    const float dist = std::sqrt (dist2);
                    const auto nrm = d / dist;
                    const float overlap = R - dist;
                    a.pos -= nrm * (overlap * wa / ws);
                    b.pos += nrm * (overlap * wb / ws);
                    const float rel = (b.vel - a.vel).getDotProduct (nrm);
                    if (rel < 0.0f)
                    {
                        const float imp = -(1.0f + 0.3f) * rel / ws;
                        a.vel -= nrm * (imp * wa);
                        b.vel += nrm * (imp * wb);
                    }
                    // a little friction where they touch
                    const Point<float> tn (-nrm.y, nrm.x);
                    const float vt = (b.vel - a.vel).getDotProduct (tn);
                    a.vel += tn * (vt * 0.04f * wa);
                    b.vel -= tn * (vt * 0.04f * wb);
                    contact[(size_t) i] = contact[(size_t) j] = 1;
                }

            for (int i = 0; i < n; ++i)
            {
                auto& b = bodies[(size_t) i];
                const float floorY = H - b.r - 2.0f;
                if (b.pos.y > floorY)
                {
                    b.pos.y = floorY;
                    if (b.vel.y > 0.0f) b.vel.y *= -0.3f;
                    b.vel.x *= 1.0f - 1.2f * h;
                    contact[(size_t) i] = 1;
                }
                if (b.pos.y < b.r)              { b.pos.y = b.r;              b.vel.y = std::abs (b.vel.y) * 0.5f; }
                if (b.pos.x < b.r + 2.0f)       { b.pos.x = b.r + 2.0f;       b.vel.x = std::abs (b.vel.x) * 0.5f; }
                if (b.pos.x > W - b.r - 2.0f)   { b.pos.x = W - b.r - 2.0f;   b.vel.x = -std::abs (b.vel.x) * 0.5f; }
                // rolling: touching something, it turns with its travel; in the air it keeps spinning
                if (contact[(size_t) i]) b.spin += (b.vel.x / b.r - b.spin) * jmin (1.0f, 20.0f * h);
                else                     b.spin *= 1.0f - 0.3f * h;
                b.angle += b.spin * h;
            }
        }
    }
    else
    {
        haveScreenPos = false;
        // float back home (critically damped), unwinding the spin
        for (auto& b : bodies)
        {
            const auto d = b.home - b.pos;
            b.vel += (d * 40.0f - b.vel * 12.6f) * dt;
            b.pos += b.vel * dt;
            b.angle = std::remainder (b.angle, MathConstants<float>::twoPi) * std::exp (-5.0f * dt);
            b.spin = 0.0f;
            if (d.getDistanceFromOrigin() < 0.3f && b.vel.getDistanceFromOrigin() < 2.0f) { b.pos = b.home; b.vel = {}; }
        }
    }

    for (auto& b : bodies)
    {
        const float away = jlimit (0.0f, 1.0f, b.pos.getDistanceFrom (b.home) / 40.0f);
        const float bobW = gravityOn ? 0.0f : 1.0f - away;
        Point<float> bob;
        if (b.knob != nullptr) bob = b.knob->bob;
        else if (b.comp == &freeze) bob = { 1.5f * std::sin (t * 0.5f + 1.0f), 3.5f * std::sin (t * 0.7f + 2.0f) };
        auto off = b.pos - b.home + bob * bobW;
        if (! gravityOn && std::abs (b.angle) < 1.0e-3f)
        {
            // floating gently: snap to the screen's pixel grid, so the cached picture is copied, not resampled
            const float px = jmax (1.0f, pixelScale);
            off = { std::round (off.x * px) / px, std::round (off.y * px) / px };
            FloatKnob::justMoving = true;
            b.comp->setTransform (AffineTransform::translation (off));
            FloatKnob::justMoving = false;
        }
        else
        {
            FloatKnob::justMoving = true;   // (only knobs use that cache; other controls repaint as usual)
            b.comp->setTransform (AffineTransform::rotation (b.angle, b.home.x, b.home.y).translated (off));
            FloatKnob::justMoving = false;
        }
    }
}

void CloudfinEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) jlimit (0.0, 0.1, now - lastTick);
    lastTick = now;
    t += dt;
    lnf.time = t;

    for (auto* k : knobs) k->animate (t, dt);
    {
        const bool g = proc.apvts.getRawParameterValue (ids::gravity)->load() > 0.5f;
        if (g != gravityOn) setGravity (g);
        gravVis += ((gravityOn ? 1.0f : 0.0f) - gravVis) * jmin (1.0f, dt * 3.0f);
        // the frames' own controls go with the frames
        for (Component* c : std::initializer_list<Component*> { &scaleBox, &shapeEditor })
        {
            c->setAlpha (1.0f - gravVis);
            c->setVisible (gravVis < 0.98f);
        }
        const int pr = proc.currentPreset.load();
        if (pr != shownPreset) { shownPreset = pr; presetBox.setSelectedId (pr + 1, dontSendNotification); }
        hydroCtl.theme = &lnf.theme();
        hydroCtl.t = t;
        if (proc.apvts.getRawParameterValue (ids::hydroLevel)->load() > 0.0f) hydroCtl.repaint();
    }
    stepWater (dt);
    stepPhysics (dt);

    {
        Point<float> target (0.0f, 3.5f * std::sin (t * 0.17f + 1.0f));   // (at rest it only bobs up and down, so the picture stays centred)
        if (getScreenBounds().contains (Desktop::getMousePosition()))
        {
            const auto m = content.getMouseXYRelative().toFloat();
            target += Point<float> (jlimit (-1.0f, 1.0f, (m.x / designW - 0.5f) * 2.0f),
                                    jlimit (-1.0f, 1.0f, (m.y / designH - 0.5f) * 2.0f)) * -12.0f;
        }
        parallax += (target - parallax) * jmin (1.0f, dt * 2.5f);
    }

    const float f = proc.freezeAmount.load();
    if (std::abs (f - lastFreeze) > 0.002f)
    {
        lastFreeze = f;
        freeze.getProperties().set ("fill", jmax (f, freeze.getToggleState() ? 0.08f : 0.0f));
    }
    specFreeze.getProperties().set ("fill", jmax (proc.spectralFreezeAmount.load(), specFreeze.getToggleState() ? 0.08f : 0.0f));

    shapeEditor.poll (t, modValue (ids::grSize) * 0.001f);
    chain.poll (dt);
    themeSwitch.poll (dt);
    content.repaint();
}
