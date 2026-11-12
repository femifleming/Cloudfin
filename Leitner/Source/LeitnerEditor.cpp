#include "LeitnerEditor.h"

using namespace juce;

namespace lt
{
const Colour paper (0xfff3f0e8), ink (0xff141414), faint (0xff8c8880), red (0xffc4281c);
Font typed (float h, bool bold) { return Font (FontOptions ("American Typewriter", h, bold ? Font::bold : Font::plain)); }

//==============================================================================
Rule::Rule (RangedAudioParameter& main, RangedAudioParameter* d, const String& n, bool m)
    : param (main), driftParam (d), att (main, [this] (float v) { value = param.convertTo0to1 (v); repaint(); }), name (n), mirrored (m)
{
    att.sendInitialUpdate();
    if (driftParam != nullptr)
    {
        driftAtt = std::make_unique<ParameterAttachment> (*driftParam, [this] (float v) { drift = v; repaint(); });
        driftAtt->sendInitialUpdate();
    }
}
// name | line | value on the left-hand sheets; value | line | name on the right-hand ones
Rectangle<int> Rule::lineArea() const
{
    auto r = getLocalBounds();
    if (mirrored) { r.removeFromRight (nameW); r.removeFromLeft (valueW); } else { r.removeFromLeft (nameW); r.removeFromRight (valueW); }
    return r.reduced (5, 0);
}
float Rule::along (int x) const
{
    const auto l = lineArea();
    const float t = (float) (x - l.getX()) / (float) l.getWidth();
    return jlimit (0.0f, 1.0f, mirrored ? 1.0f - t : t);
}

void Rule::paint (Graphics& g)
{
    g.setFont (typed (11.0f));
    g.setColour (ink);
    const auto b = getLocalBounds();
    g.drawText (name, mirrored ? b.withLeft (b.getRight() - nameW) : b.withWidth (nameW), mirrored ? Justification::centredRight : Justification::centredLeft);
    g.drawText (param.getCurrentValueAsText(), mirrored ? b.withWidth (valueW) : b.withLeft (b.getRight() - valueW),
                mirrored ? Justification::centredLeft : Justification::centredRight);
    const auto l = lineArea().toFloat();
    const float y = l.getCentreY() + 1.0f;
    auto X = [&] (float t) { return mirrored ? l.getRight() - l.getWidth() * t : l.getX() + l.getWidth() * t; };
    g.drawLine (l.getX(), y, l.getRight(), y, 0.8f);
    for (int i = 0; i <= 4; ++i)   // the scale
        g.drawLine (X ((float) i / 4.0f), y - (i % 4 == 0 ? 4.0f : 2.0f), X ((float) i / 4.0f), y, 0.8f);
    if (driftParam != nullptr && std::abs (drift) > 0.004f)
    {
        // its drift: a red stroke under the line as far as chance may carry it, and a circle where it is now
        g.setColour (red);
        g.drawLine (X (value), y + 4.0f, X (jlimit (0.0f, 1.0f, value + drift)), y + 4.0f, 1.6f);
        if (live != nullptr) { const float x = X (jlimit (0.0f, 1.0f, live->load (std::memory_order_relaxed))); g.drawEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f, 1.0f); }
        g.setColour (ink);
    }
    g.fillRect (Rectangle<float> (X (value) - 1.5f, y - 7.0f, 3.0f, 12.0f));
}
void Rule::setFrom (const MouseEvent& e)
{
    if (dragging == 1) att.setValueAsPartOfGesture (param.convertFrom0to1 (along (e.x)));
    else if (dragging == 2)
    {
        float v = jlimit (-1.0f, 1.0f, driftAtDown + 2.0f * (along (e.x) - downAt));
        if (std::abs (v) < 0.04f) v = 0.0f;
        driftAtt->setValueAsPartOfGesture (v);
    }
}
void Rule::mouseDown (const MouseEvent& e)
{
    dragging = (e.mods.isAltDown() && driftParam != nullptr) ? 2 : 1;
    downAt = along (e.x); driftAtDown = drift;
    if (dragging == 1) { att.beginGesture(); setFrom (e); } else driftAtt->beginGesture();
}
void Rule::mouseDrag (const MouseEvent& e) { setFrom (e); }
void Rule::mouseUp (const MouseEvent&)
{
    if (dragging == 1) att.endGesture();
    if (dragging == 2) driftAtt->endGesture();
    dragging = 0;
}
void Rule::mouseDoubleClick (const MouseEvent& e)
{
    if (e.mods.isAltDown() && driftParam != nullptr) driftAtt->setValueAsCompleteGesture (0.0f);
    else att.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

//==============================================================================
Choice::Choice (RangedAudioParameter& p, const String& n, bool m) : param (p), att (p, [this] (float) { repaint(); }), name (n), mirrored (m) { att.sendInitialUpdate(); }
void Choice::paint (Graphics& g)
{
    g.setFont (typed (11.0f));
    g.setColour (ink);
    const auto b = getLocalBounds();
    g.drawText (name, mirrored ? b.withLeft (b.getRight() - nameW) : b.withWidth (nameW), mirrored ? Justification::centredRight : Justification::centredLeft);
    auto r = mirrored ? b.withTrimmedRight (nameW + 5) : b.withTrimmedLeft (nameW + 5);
    g.drawLine ((float) r.getX(), (float) getHeight() - 3.0f, (float) r.getRight(), (float) getHeight() - 3.0f, 0.8f);
    const float ax = mirrored ? (float) r.getX() + 5.0f : (float) r.getRight() - 5.0f;
    Path t; t.addTriangle (ax - 4.0f, 7.0f, ax + 4.0f, 7.0f, ax, 13.0f);
    g.fillPath (t);
    g.drawText (param.getCurrentValueAsText(), mirrored ? r.withTrimmedLeft (13) : r.withTrimmedRight (13), mirrored ? Justification::centredRight : Justification::centredLeft, true);
}
void Choice::mouseDown (const MouseEvent&)
{
    PopupMenu m;
    const auto names = param.getAllValueStrings();
    const int now = roundToInt (param.convertFrom0to1 (param.getValue()));
    for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == now);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) att.setValueAsCompleteGesture ((float) (r - 1)); });
}

Check::Check (RangedAudioParameter& p, const String& n, bool m) : param (p), att (p, [this] (float v) { on = v > 0.5f; repaint(); }), name (n), mirrored (m) { att.sendInitialUpdate(); }
void Check::paint (Graphics& g)
{
    g.setFont (typed (11.0f));
    g.setColour (ink);
    const auto box = Rectangle<float> (mirrored ? (float) getWidth() - 12.0f : 1.0f, (float) getHeight() * 0.5f - 5.5f, 11.0f, 11.0f);
    g.drawRect (box, 0.9f);
    if (on) { g.drawLine ({ box.getTopLeft(), box.getBottomRight() }, 1.4f); g.drawLine ({ box.getTopRight(), box.getBottomLeft() }, 1.4f); }
    if (mirrored) g.drawText (name, 0, 0, getWidth() - 18, getHeight(), Justification::centredRight);
    else          g.drawText (name, 18, 0, getWidth() - 18, getHeight(), Justification::centredLeft);
}
void Check::mouseDown (const MouseEvent&) { att.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

//==============================================================================
void PulseStrip::paint (Graphics& g)
{
    const auto p = proc.readSpace();
    const auto st = proc.space.stateNow();
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ink);
    g.drawRect (b, 0.9f);
    g.setFont (typed (10.0f));
    const String label = line == 0 ? "line 1: its pulses over one pass" : "line 2: the same, as jux has it";
    g.drawText (label, getLocalBounds().reduced (6, 3), mirrored ? Justification::topRight : Justification::topLeft);
    const auto lane = b.reduced (6.0f, 0.0f).withTrimmedTop (22.0f).withTrimmedBottom (10.0f);
    auto X = [&] (float u) { return mirrored ? lane.getRight() - lane.getWidth() * u : lane.getX() + lane.getWidth() * u; };
    const float T = jmax (0.05f, p.seconds) * proc.space.passLength.load();
    if (p.pulse <= 0.0f)
    {
        g.setColour (faint);
        g.drawText ("no pulses: the line sounds all the time", lane.toNearestInt(), Justification::centred);
    }
    else
    {
        g.setColour (line == 0 ? ink : red);
        float t = 0.0f;
        for (int k = 0; k < 400 && t < T; ++k)
        {
            const float gap = leitner::pulseGap (p, line, st.pass, k, t / T);
            if (leitner::pulseSounds (p, line, st.pass, k))
            {
                const float x0 = X (t / T), x1 = X (jmin (1.0f, (t + gap * jlimit (0.02f, 1.0f, p.pulseWidth)) / T));
                g.fillRect (Rectangle<float> (jmin (x0, x1), lane.getY(), jmax (1.0f, std::abs (x1 - x0)), lane.getHeight()));
            }
            t += gap;
        }
    }
    // where the pass has got to
    double u = (double) proc.space.phaseView.load() + (line == 1 ? 0.5 * (double) p.counter : 0.0); u -= std::floor (u);
    g.setColour (ink.withAlpha (0.6f));
    g.drawLine (X ((float) u), lane.getY() - 5.0f, X ((float) u), lane.getBottom() + 5.0f, 1.0f);
}

//==============================================================================
// The room is two units across (the unit sphere's box) and you stand at the middle of it. The camera
// looks at the middle from 2.64 away: from straight behind you to begin with (the back wall then
// comes out 0.45 of the opening, as on the sheets), and from wherever you drag it to.
Room::View Room::view() const
{
    const float yaw = proc.viewYaw.load() * leitner::kPi / 180.0f, pitch = proc.viewPitch.load() * leitner::kPi / 180.0f;
    const float cy = std::cos (yaw), sy = std::sin (yaw), cp = std::cos (pitch), sp = std::sin (pitch);
    return { { cp * cy, cp * sy, -sp }, { sy, -cy, 0.0f }, { sp * cy, sp * sy, cp } };
}
Point<float> Room::project (const View& v, leitner::Vec3 p, float* scale) const
{
    constexpr float D = 2.64f;
    const float depth = jmax (0.35f, p.dot (v.f) + D);
    const float s = (D - 1.0f) / depth;                      // 1 at the opening of the front view
    const float H = (float) getHeight() * 0.47f;
    if (scale != nullptr) *scale = s;
    return { (float) getWidth() * 0.5f + p.dot (v.r) * H * s, (float) getHeight() * 0.5f - p.dot (v.u) * H * s };
}
void Room::mouseDown (const MouseEvent&) { yawDown = proc.viewYaw.load(); pitchDown = proc.viewPitch.load(); }
void Room::mouseDrag (const MouseEvent& e)
{
    float yaw = yawDown - 0.45f * (float) e.getDistanceFromDragStartX();
    while (yaw > 180.0f) yaw -= 360.0f;
    while (yaw < -180.0f) yaw += 360.0f;
    proc.viewYaw.store (yaw);
    proc.viewPitch.store (jlimit (-30.0f, 89.0f, pitchDown + 0.4f * (float) e.getDistanceFromDragStartY()));
    repaint();
}
void Room::mouseDoubleClick (const MouseEvent&) { proc.viewYaw.store (0.0f); proc.viewPitch.store (0.0f); repaint(); }

void Room::paint (Graphics& g)
{
    using leitner::Vec3;
    const auto p = proc.readSpace();
    const auto st = proc.space.stateNow();
    const auto V = view();
    auto P = [&] (Vec3 v) { return project (V, v); };
    g.setColour (ink);
    g.drawRect (getLocalBounds().toFloat(), 1.2f);
    Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (getLocalBounds().reduced (1));

    // the cube's twelve edges
    for (int a = 0; a < 3; ++a)
        for (float s : { -1.0f, 1.0f }) for (float t : { -1.0f, 1.0f })
        {
            Vec3 from, to;
            if (a == 0) { from = { -1, s, t }; to = { 1, s, t }; } else if (a == 1) { from = { s, -1, t }; to = { s, 1, t }; } else { from = { s, t, -1 }; to = { s, t, 1 }; }
            g.drawLine ({ P (from), P (to) }, 1.0f);
        }

    // the loudspeakers: small open circles in a grid on every face, each drawn as it lies on its wall.
    // A wall between you and the room is drawn faint so you can see in through it.
    constexpr int N = 5;
    for (int face = 0; face < 6; ++face)
    {
        const int axis = face / 2; const float side = face % 2 == 0 ? -1.0f : 1.0f;
        Vec3 n {}, t1 {}, t2 {};
        if (axis == 0) { n = { side, 0, 0 }; t1 = { 0, 1, 0 }; t2 = { 0, 0, 1 }; }
        if (axis == 1) { n = { 0, side, 0 }; t1 = { 1, 0, 0 }; t2 = { 0, 0, 1 }; }
        if (axis == 2) { n = { 0, 0, side }; t1 = { 1, 0, 0 }; t2 = { 0, 1, 0 }; }
        const bool near = n.dot (V.f) < -0.25f;
        g.setColour (ink.withAlpha (near ? 0.16f : 0.75f));
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
            {
                const float a = -1.0f + (2.0f * (float) i + 1.0f) / (float) N, b = -1.0f + (2.0f * (float) j + 1.0f) / (float) N;
                const Vec3 c = n + t1 * a + t2 * b;
                const auto pc = P (c), pa = P (c + t1 * 0.05f), pb = P (c + t2 * 0.05f);
                Path e; e.addEllipse (-1.0f, -1.0f, 2.0f, 2.0f);
                g.strokePath (e, PathStrokeType (0.7f), AffineTransform (pa.x - pc.x, pb.x - pc.x, pc.x, pa.y - pc.y, pb.y - pc.y, pc.y));
            }
    }

    // which way is the front: an arrow on the floor
    {
        g.setColour (ink.withAlpha (0.8f));
        Path arrow; arrow.addArrow ({ P ({ 0.45f, 0, -1 }), P ({ 0.9f, 0, -1 }) }, 1.2f, 8.0f, 9.0f);
        g.fillPath (arrow);
        g.setFont (typed (9.5f));   // (and a word over the front wall, where the figure never hides it)
        g.drawText ("front  vorn", Rectangle<float> (90.0f, 12.0f).withCentre (P ({ 1.0f, 0, 1.0f }).translated (0.0f, -8.0f)), Justification::centred);
    }

    // the figure: solid black, standing in the middle, ears at the centre of the room
    {
        const auto head = P ({ 0, 0, 0 }), feet = P ({ 0, 0, -1 });
        const float h = jmax (4.0f, head.getDistanceFrom (feet)), w = h * 0.2f;
        g.setColour (ink);
        Path body;
        body.addEllipse (-w * 0.36f, -h * 0.09f, w * 0.72f, h * 0.17f);
        body.addRoundedRectangle (-w * 0.62f, h * 0.11f, w * 1.24f, h * 0.42f, w * 0.3f);
        body.addRectangle (-w * 0.5f, h * 0.5f, w * 0.44f, h * 0.5f);
        body.addRectangle (w * 0.06f, h * 0.5f, w * 0.44f, h * 0.5f);
        const float tilt = std::atan2 (feet.x - head.x, feet.y - head.y);
        g.fillPath (body, AffineTransform::rotation (-tilt).translated (head));
    }

    // the two lines of sound: heavy, with arrows; the loudspeakers each is built from; where it is now
    float ws[leitner::kMaxPoints]; int idx[leitner::kMaxPoints];
    const int n = p.points < leitner::kMaxPoints ? leitner::rasterPoints (p, ws, idx) : 0;
    for (int li = 1; li >= 0; --li)
    {
        const Colour col = li == 0 ? ink : red;
        Path path;
        constexpr int S = 240;
        for (int i = 0; i <= S; ++i)
        {
            const auto q = P (leitner::linePoint (li, p, st, (float) i / (float) S));
            if (i == 0) path.startNewSubPath (q); else path.lineTo (q);
        }
        g.setColour (col);
        g.strokePath (path, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        for (float w : { 0.3f, 0.7f })
        {
            const auto a = P (leitner::linePoint (li, p, st, w)), b = P (leitner::linePoint (li, p, st, w + 0.012f));
            if (a.getDistanceFrom (b) < 0.5f) continue;
            Path head; head.addArrow ({ a, a + (b - a) * (11.0f / a.getDistanceFrom (b)) }, 0.0f, 10.0f, 11.0f);
            g.fillPath (head);
        }
        for (int k = 0; k < n; ++k)
        {
            float s; const auto c = project (V, leitner::linePoint (li, p, st, ws[k], idx[k]), &s);
            const float r = 6.0f * s;
            g.setColour (paper); g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
            g.setColour (col);   g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.3f);
        }
        auto& v = proc.space.view[li];
        const float lev = jlimit (0.0f, 1.0f, 0.35f + 4.0f * v.level.load());
        auto sounding = [&] (float x, float y, float z, float gain)
        {
            if (gain < 0.02f) return;
            float s; const auto c = project (V, { x, y, z }, &s);
            const float r = (6.0f + 7.0f * lev * gain) * s;
            g.setColour (col.withAlpha (jlimit (0.0f, 1.0f, gain)));
            g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        };
        sounding (v.ax.load(), v.ay.load(), v.az.load(), v.ga.load());
        sounding (v.bx.load(), v.by.load(), v.bz.load(), v.gb.load());
    }
}
} // namespace lt

//==============================================================================
namespace
{
const char* const stageTitle[fp::numStages] = { "SPECTRAL SMEAR", "PITCH", "FORMANT", "GRAINS", "JI DELAY", "REVERB", "COMPANDER" };
const char* const stageWord[fp::numStages] = { "smear", "pitch", "formant", "grains", "delay", "reverb", "comp" };
constexpr int kRow = 20, kColW = 212;
const Rectangle<int> sheetL (12, 12, 464, 766), sheetC (488, 12, 524, 766), sheetR (1024, 12, 464, 766);
}

Font LeitnerEditor::Look::getTextButtonFont (TextButton&, int) { return lt::typed (11.0f); }
Font LeitnerEditor::Look::getPopupMenuFont() { return lt::typed (13.0f); }
void LeitnerEditor::Look::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    g.setColour (down ? lt::ink.withAlpha (0.15f) : over ? lt::ink.withAlpha (0.06f) : lt::paper);
    g.fillRect (b.getLocalBounds());
    g.setColour (lt::ink);
    g.drawRect (b.getLocalBounds().toFloat(), 0.9f);
}

LeitnerEditor::LeitnerEditor (LeitnerProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    look.setColour (PopupMenu::backgroundColourId, lt::paper);
    look.setColour (PopupMenu::textColourId, lt::ink);
    look.setColour (PopupMenu::headerTextColourId, lt::faint);
    look.setColour (PopupMenu::highlightedBackgroundColourId, lt::ink);
    look.setColour (PopupMenu::highlightedTextColourId, lt::paper);
    look.setColour (TextButton::textColourOffId, lt::ink);
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.addAndMakeVisible (room);
    room.setBounds (502, 86, 496, 318);

    // one column of controls. Columns on the right-hand side of the window are drawn reversed.
    struct Column
    {
        LeitnerEditor& ed; int x, y, w; bool mirrored;
        void add (Component* c, int h = kRow) { c->setBounds (x, y, w, h); ed.parts.add (c); ed.content.addAndMakeVisible (c); y += h; }
        void head (const String& t, int stage) { ed.headings.push_back ({ t, { x, y, w, 18 }, stage, mirrored }); y += 24; }
        void fx (const char* id, const String& name)
        {
            auto* r = new lt::Rule (*ed.proc.apvts.getParameter (id), ed.proc.apvts.getParameter (ids::att (id)), name, mirrored);
            if (const int mi = modIndexOf (id); mi >= 0) r->live = ed.proc.modNorm[(size_t) mi].get();
            add (r);
        }
        void rule (RangedAudioParameter* prm, const String& name) { add (new lt::Rule (*prm, nullptr, name, mirrored)); }
        void choice (RangedAudioParameter* prm, const String& name, int nameW = 66) { auto* c = new lt::Choice (*prm, name, mirrored); c->nameW = nameW; add (c); }
        void check (RangedAudioParameter* prm, const String& name) { add (new lt::Check (*prm, name, mirrored)); }
        void gap() { y += 14; }
    };
    auto& ap = proc.apvts;

    // the left sheet: the first half of the sound program
    {
        Column a { *this, 26, 74, kColW, false };
        a.head (stageTitle[fp::stGate], fp::stGate);
        for (auto [id, n] : { std::pair { ids::sgCutoff, "cutoff" }, { ids::sgAttack, "attack" }, { ids::sgSmear, "smear" }, { ids::sgKnee, "knee" },
                              { ids::sgBlur, "blur" }, { ids::sgScatter, "scatter" }, { ids::sgTilt, "tilt" }, { ids::sgMix, "mix" } }) a.fx (id, n);
        a.check (ap.getParameter (ids::sgFreeze), "spectral freeze"); a.gap();
        a.head (stageTitle[fp::stPitch], fp::stPitch);
        for (auto [id, n] : { std::pair { ids::psShift, "shift" }, { ids::psGrain, "grain" }, { ids::psFeedback, "feedback" }, { ids::psMix, "mix" } }) a.fx (id, n);
        a.choice (ap.getParameter (ids::psScale), "scale", 40);

        Column b { *this, 26 + kColW + 14, 74, kColW, false };
        b.head (stageTitle[fp::stFormant], fp::stFormant);
        for (auto [id, n] : { std::pair { ids::fmVowel, "vowel" }, { ids::fmShift, "shift" }, { ids::fmRes, "resonance" }, { ids::fmMix, "mix" } }) b.fx (id, n);
        b.gap();
        b.head (stageTitle[fp::stDelay], fp::stDelay);
        for (auto [id, n] : { std::pair { ids::dlTime, "time" }, { ids::dlFeedback, "feedback" }, { ids::dlTone, "tone" }, { ids::dlMix, "mix" } }) b.fx (id, n);
        b.gap();
        b.head (stageTitle[fp::stReverb], fp::stReverb);
        for (auto [id, n] : { std::pair { ids::rvDecay, "decay" }, { ids::rvTone, "tone" }, { ids::rvMix, "mix" } }) b.fx (id, n);
    }
    // the right sheet: the second half, reversed, block for block the same sizes as the left
    {
        Column a { *this, designW - 26 - kColW, 74, kColW, true };
        a.head (stageTitle[fp::stGrains], fp::stGrains);
        for (auto [id, n] : { std::pair { ids::grSize, "size" }, { ids::grDensity, "density" }, { ids::grPosition, "position" }, { ids::grSpray, "spray" },
                              { ids::grWidth, "rand pan" }, { ids::grReverse, "reverse" }, { ids::grOctave, "octave" }, { ids::grFade, "freeze fade" }, { ids::grMix, "mix" } }) a.fx (id, n);
        a.gap();
        a.head ("WASSER  xx  GRAIN FREEZE, WATER", -1);
        a.check (ap.getParameter (ids::grFreeze), "freeze the grains");
        a.check (ap.getParameter (ids::grRandOct), "random octave");
        shapeBtn.setButtonText ("grain shape: saw");
        shapeBtn.setBounds (a.x, a.y + 1, kColW, kRow - 2);
        a.y += kRow;
        shapeBtn.onClick = [this]
        {
            PopupMenu m;
            static const char* const names[] = { "saw", "bell", "triangle", "reverse", "pulse" };
            for (int i = 0; i < 5; ++i) m.addItem (i + 1, names[i]);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (shapeBtn), [this] (int r)
            {
                if (r <= 0) return;
                float t[CloudfinProcessor::kShapeN];
                fp::GrainShifter::makeShape (r - 1, t);
                proc.setGrainShape (t);
                shapeBtn.setButtonText ("grain shape: " + String (names[r - 1]));
            });
        };
        content.addAndMakeVisible (shapeBtn);
        a.rule (ap.getParameter (ids::hydroLevel), "hydrophone");
        a.choice (ap.getParameter (ids::hydroInto), "goes to", 52);

        Column b { *this, designW - 26 - 2 * kColW - 14, 74, kColW, true };
        b.head (stageTitle[fp::stComp], fp::stComp);
        for (auto [id, n] : { std::pair { ids::cpThresh, "threshold" }, { ids::cpRatio, "ratio" }, { ids::cpBelow, "below" }, { ids::cpMakeup, "makeup" } }) b.fx (id, n);
        b.gap();
        b.head ("COMPANDER, ITS TIMES", -1);
        for (auto [id, n] : { std::pair { ids::cpAttack, "attack" }, { ids::cpRelease, "release" }, { ids::cpMix, "mix" } }) b.fx (id, n);
        b.y += kRow;   // (the delay opposite has a fourth line)
        b.gap();
        b.head ("ZUFALL  xx  LOOP, DRIFT", -1);
        b.fx (ids::masterFb, "feedback");
        b.rule (ap.getParameter (ids::modRate), "drift rate");
        b.rule (ap.getParameter (ids::modSlew), "drift slew");
    }
    // the lower half of the side sheets: the pulse layer, and each line's pulses drawn out
    {
        constexpr int y0 = 470;
        Column a { *this, 26 + kColW + 14, y0, kColW, false };
        a.head (String (CharPointer_UTF8 ("PULSE  xx  after Fell, L\xc3\xa1z\xc3\xa1r")), -1);
        a.rule (proc.pulse, "pulse"); a.rule (proc.pulseRate, "rate"); a.rule (proc.pulseBend, "bend"); a.rule (proc.pulseWidth, "width");
        a.choice (proc.cell, "cell");
        Column b { *this, designW - 26 - 2 * kColW - 14, y0, kColW, true };
        b.head ("after Fell, Kindohm  xx  PATTERN", -1);
        b.rule (proc.rotate, "rotate"); b.rule (proc.hits, "hits"); b.rule (proc.steps, "steps"); b.rule (proc.drop, "drop");
        b.choice (proc.jux, "jux", 30);
        strip1.setBounds (26, y0 + 2, kColW, 122);
        strip2.setBounds (designW - 26 - kColW, y0 + 2, kColW, 122);
        content.addAndMakeVisible (strip1); content.addAndMakeVisible (strip2);
    }
    // the middle sheet, under the room: the space, in two facing columns
    {
        Column a { *this, 506, 474, 238, false };
        a.choice (proc.figure, "figure");
        a.rule (proc.seconds, "pass"); a.rule (proc.rest, "rest"); a.rule (proc.points, "points"); a.rule (proc.legato, "legato");
        a.choice (proc.sieve, "sieve");
        a.rule (proc.chance, "chance"); a.rule (proc.walk, "walk"); a.rule (proc.scatter, "scatter");
        a.rule (proc.level, "level");
        Column b { *this, 756, 474, 238, true };
        b.rule (proc.size, "size"); b.rule (proc.height, "height"); b.rule (proc.turn, "turn"); b.rule (proc.apart, "apart"); b.rule (proc.counter, "counter");
        b.rule (proc.swell, "swell"); b.rule (proc.lift, "lift"); b.rule (proc.room, "room"); b.rule (proc.roomDecay, "decay");
        b.choice (proc.order, "order");
        auto* hold = new lt::Check (*proc.hold, "hold the lines still", false);
        hold->setBounds (670, 680, 160, kRow);
        parts.add (hold); content.addAndMakeVisible (hold);

        presetBtn.setBounds (506, 738, 238, kRow + 2);
        presetBtn.onClick = [this]
        {
            PopupMenu m; String group;
            for (int i = 0; i < proc.getNumPrograms(); ++i)
            {
                if (proc.getProgramGroup (i) != group) { group = proc.getProgramGroup (i); if (i > 0) m.addSectionHeader (group); }
                m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
            }
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
        };
        randomBtn.setButtonText ("a random patch");
        randomBtn.setBounds (756, 738, 238, kRow + 2);
        randomBtn.onClick = [this] { proc.randomize(); };
        content.addAndMakeVisible (presetBtn); content.addAndMakeVisible (randomBtn);
    }

    setResizable (true, true);
    setResizeLimits (designW / 2, designH / 2, (int) (designW * 1.6), (int) (designH * 1.6));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / designH);
    setSize ((int) (designW * 0.85), (int) (designH * 0.85));
    timerCallback();
    startTimerHz (30);
}

LeitnerEditor::~LeitnerEditor() { stopTimer(); setLookAndFeel (nullptr); }

void LeitnerEditor::paint (Graphics& g) { g.fillAll (Colour (0xffd9d5cb)); }
void LeitnerEditor::resized()
{
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) designW));
}

void LeitnerEditor::paintSheets (Graphics& g)
{
    using lt::typed;
    for (auto sheet : { sheetL, sheetC, sheetR })
    {
        g.setColour (Colours::black.withAlpha (0.16f)); g.fillRect (sheet.translated (0, 4).expanded (1, 0));
        g.setColour (lt::paper); g.fillRect (sheet);
    }
    // ring-binder holes down the outer edge of each side sheet
    g.setColour (Colour (0xffd9d5cb));
    for (int i = 0; i < 26; ++i)
    {
        g.fillEllipse (15.5f, 28.0f + (float) i * 28.6f, 7.0f, 7.0f);
        g.fillEllipse ((float) designW - 22.5f, 28.0f + (float) i * 28.6f, 7.0f, 7.0f);
    }

    // the middle sheet
    const auto mid = sheetC.reduced (14, 0);
    g.setColour (lt::ink);
    g.setFont (typed (21.0f));
    g.drawText ("L  E  I  T  N  E  R", mid.getX(), 22, mid.getWidth(), 26, Justification::centred);
    g.setFont (typed (10.5f));
    const int o = proc.orderNow.load();
    const String out = o >= 1 ? String (o) + (o == 3 ? "rd" : "th") + " order Ambisonics, " + String (proc.channelsNow.load()) + " channels, ACN / SN3D: route it to a decoder"
                              : String ("stereo output: a left-right fold only. Give the track 16 to 64 channels.");
    g.drawText (out, mid.getX(), 50, mid.getWidth(), 14, Justification::centred);
    g.setColour (lt::faint);
    g.drawText (String (CharPointer_UTF8 ("Cloudfin as a sound space, after Leitner, Xenakis, Fell, L\xc3\xa1z\xc3\xa1r and Kindohm")), mid.getX(), 65, mid.getWidth(), 14, Justification::centred);

    const int fig = proc.figure->getIndex();
    g.setColour (lt::ink);
    g.setFont (typed (12.0f, true));
    g.drawText (String::fromUTF8 (leitner::figureNames()[fig]).toUpperCase() + "  xxxxx  " + String (leitner::figureEnglish()[fig]), mid.getX(), 426, mid.getWidth(), 16, Justification::centred);
    g.setFont (typed (10.5f));
    g.drawText ("line 1 black, the left channel  xxxxx  line 2 red, the right channel, its mirror image", mid.getX(), 443, mid.getWidth(), 14, Justification::centred);
    g.drawLine ((float) mid.getX(), 464.0f, (float) mid.getRight(), 464.0f, 0.8f);
    g.setColour (lt::faint);
    g.drawText ("drag the room to turn it  xxxxx  double-click: from the front again", mid.getX(), 407, mid.getWidth(), 14, Justification::centred);

    // the order of the effects, centred
    g.setFont (typed (11.0f));
    const auto order = proc.getOrder();
    int widths[fp::numStages], total = 0;
    for (int i = 0; i < fp::numStages; ++i) { widths[i] = roundToInt (GlyphArrangement::getStringWidth (typed (11.0f), stageWord[order[(size_t) i]])) + 6; total += widths[i] + (i > 0 ? 14 : 0); }
    orderWords.clear();
    int x = designW / 2 - total / 2;
    const int oy = 712;
    g.setColour (lt::ink);
    for (int i = 0; i < fp::numStages; ++i)
    {
        if (i > 0) { g.drawText (">", x, oy, 14, 18, Justification::centred); x += 14; }
        orderWords.push_back ({ x, oy, widths[i], 18 });
        g.drawText (stageWord[order[(size_t) i]], x, oy, widths[i], 18, Justification::centred);
        g.drawLine ((float) x + 2.0f, (float) oy + 16.0f, (float) (x + widths[i]) - 2.0f, (float) oy + 16.0f, 0.7f);
        x += widths[i];
    }
    g.setColour (lt::faint);
    g.setFont (typed (10.0f));
    g.drawText ("the sound program runs in this order (click a word to move it earlier)", mid.getX(), 698, mid.getWidth(), 13, Justification::centred);

    // the side sheets
    g.setColour (lt::ink);
    g.setFont (typed (12.0f, true));
    g.drawText ("SOUND PROGRAM I  xxxxx  TON-PROGRAMM", 26, 26, 430, 16, Justification::centredLeft);
    g.drawText ("TON-PROGRAMM  xxxxx  SOUND PROGRAM II", designW - 26 - 430, 26, 430, 16, Justification::centredRight);
    g.setFont (typed (10.5f));
    g.setColour (lt::faint);
    g.drawText ("click or drag along a line to set it", 26, 46, 430, 14, Justification::centredLeft);
    g.drawText ("alt-drag a line to set its drift (red)", designW - 26 - 430, 46, 430, 14, Justification::centredRight);
    g.drawText ("double-click a line to put it back", 26, 754, 430, 14, Justification::centredLeft);
    g.drawText ("alt-double-click: no drift", designW - 26 - 430, 754, 430, 14, Justification::centredRight);

    for (auto& h : headings)
    {
        g.setColour (lt::ink);
        g.setFont (typed (11.5f, true));
        String t = h.text;
        if (h.stage >= 0)
            for (int i = 0; i < fp::numStages; ++i)
                if (order[(size_t) i] == h.stage) t = h.mirrored ? t + "  " + String (i + 1) : String (i + 1) + "  " + t;
        g.drawText (t, h.area, h.mirrored ? Justification::centredRight : Justification::centredLeft);
        g.drawLine ((float) h.area.getX(), (float) h.area.getBottom(), (float) h.area.getRight(), (float) h.area.getBottom(), 1.2f);
    }
}

void LeitnerEditor::clickOrder (Point<int> pt)
{
    for (int i = 1; i < (int) orderWords.size(); ++i)
        if (orderWords[(size_t) i].contains (pt))
        {
            auto o = proc.getOrder();
            std::swap (o[(size_t) i], o[(size_t) i - 1]);
            proc.setOrder (o);
            content.repaint();
        }
}

void LeitnerEditor::timerCallback()
{
    room.repaint(); strip1.repaint(); strip2.repaint();
    const auto packed = CloudfinProcessor::packOrder (proc.getOrder());
    const int fig = proc.figure->getIndex(), out = proc.orderNow.load() * 100 + proc.channelsNow.load();
    if (packed != seenOrder || fig != seenFigure || out != seenOut) { seenOrder = packed; seenFigure = fig; seenOut = out; content.repaint(); }
    const String name = "program: " + proc.getProgramName (proc.getCurrentProgram());
    if (presetBtn.getButtonText() != name) presetBtn.setButtonText (name);
    for (auto* c : parts) if (auto* r = dynamic_cast<lt::Rule*> (c); r != nullptr && r->live != nullptr) r->repaint();
}
