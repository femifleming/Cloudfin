// Offline checks for Leitner (not shipped): the Ambisonic encoding (direction, mirror image, the
// raster's steps, every order), the state, CPU, and a picture of the window.
#include "../Source/LeitnerEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cstdio>
#include <set>

using namespace juce;
static constexpr double SR = 48000.0;
static constexpr int BLOCK = 256;
static int failures = 0;
static void check (bool ok, const String& what) { std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8()); if (! ok) ++failures; }

static void setP (RangedAudioParameter* p, float real) { p->setValueNotifyingHost (p->convertTo0to1 (real)); }

// everything in Cloudfin out of the way: the lines carry the input untouched
static void dry (LeitnerProcessor& p)
{
    for (auto id : { ids::sgMix, ids::psMix, ids::fmMix, ids::grMix, ids::dlMix, ids::rvMix, ids::cpMix, ids::masterFb, ids::hydroLevel })
        setP (p.apvts.getParameter (id), 0.0f);
    setP (p.room, 0.0f);
}

// a 440 Hz tone into the left, the right or both, for 'seconds'; returns all 64 channels
static AudioBuffer<float> run (LeitnerProcessor& p, double seconds, bool left, bool right, int channels = 64, double* realtime = nullptr)
{
    p.setPlayConfigDetails (2, channels, SR, BLOCK);
    p.prepareToPlay (SR, BLOCK);
    const int total = (int) (seconds * SR) / BLOCK * BLOCK;
    AudioBuffer<float> all (channels, total), buf (channels, BLOCK);
    MidiBuffer midi;
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += BLOCK)
    {
        buf.clear();
        for (int i = 0; i < BLOCK; ++i)
        {
            const float s = 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * 440.0 * (pos + i) / SR);
            if (left) buf.setSample (0, i, s);
            if (right) buf.setSample (1, i, s);
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < channels; ++c) all.copyFrom (c, pos, buf, c, 0, BLOCK);
    }
    if (realtime != nullptr) *realtime = seconds / std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return all;
}

// the direction a first-order decoder would point to over a stretch: (X, Y, Z) / W by least squares
static leitner::Vec3 directionOf (const AudioBuffer<float>& a, int from, int len)
{
    double ww = 0, wx = 0, wy = 0, wz = 0;
    for (int i = from; i < from + len; ++i)
    {
        const double w = a.getSample (0, i);
        ww += w * w; wy += w * a.getSample (1, i); wz += w * a.getSample (2, i); wx += w * a.getSample (3, i);
    }
    return { (float) (wx / ww), (float) (wy / ww), (float) (wz / ww) };
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    {   // the harmonics themselves: SN3D, orthogonal, at every order (a mean over many directions)
        const auto dirs = fp::ambi::fibonacci (4000);
        bool ok = true; double worst = 0.0;
        for (int a = 0; a < 64; ++a)
            for (int b = a; b < 64; ++b)
            {
                double s = 0;
                for (auto& d : dirs) { const auto y = fp::ambi::sh (d, 7); s += (double) y[(size_t) a] * y[(size_t) b]; }
                s /= (double) dirs.size();
                const double want = a == b ? 1.0 / (2.0 * fp::ambi::orderOf (a) + 1.0) : 0.0;
                worst = std::max (worst, std::abs (s - want));
                if (std::abs (s - want) > 0.01) ok = false;
            }
        check (ok, String::formatted ("the 64 harmonics are orthogonal with SN3D weights (worst error %.4f)", worst));
    }
    {   // a held line: the encoded direction is where the figure says the line is, and line 2 is its mirror image
        LeitnerProcessor p; dry (p);
        setP (p.hold, 1.0f); setP (p.points, (float) leitner::kMaxPoints); setP (p.figure, (float) leitner::fgLine);
        setP (p.size, 0.9f); setP (p.height, 0.3f); setP (p.apart, 40.0f); setP (p.turn, 0.0f);
        const auto sp = p.readSpace();
        const auto want = leitner::linePoint (0, sp, {}, 0.0f);
        auto a = run (p, 0.5, true, false);
        auto d = directionOf (a, 12000, 8000);
        const auto wn = want.normalised();
        check (std::abs (d.x - wn.x) < 0.01f && std::abs (d.y - wn.y) < 0.01f && std::abs (d.z - wn.z) < 0.01f,
               String::formatted ("line 1 is encoded at (%.3f, %.3f, %.3f); the figure says (%.3f, %.3f, %.3f)", d.x, d.y, d.z, wn.x, wn.y, wn.z));
        check (std::abs (a.getRMSLevel (0, 12000, 8000) - 0.25f / std::sqrt (2.0f)) < 0.003f, "the omni channel carries the input at unity");
        LeitnerProcessor q; dry (q);
        setP (q.hold, 1.0f); setP (q.points, (float) leitner::kMaxPoints); setP (q.size, 0.9f); setP (q.height, 0.3f); setP (q.apart, 40.0f);
        auto b = run (q, 0.5, false, true);
        auto e = directionOf (b, 12000, 8000);
        check (std::abs (e.x - d.x) < 0.01f && std::abs (e.y + d.y) < 0.01f && std::abs (e.z - d.z) < 0.01f,
               String::formatted ("line 2 is its mirror image: (%.3f, %.3f, %.3f)", e.x, e.y, e.z));
    }
    {   // every order: (N + 1)^2 channels carry sound, the rest are silent
        bool ok = true; String detail;
        for (int o = 3; o <= 7; ++o)
        {
            LeitnerProcessor p; dry (p);
            setP (p.order, (float) (o - 3)); setP (p.figure, (float) leitner::fgSpiral);
            const auto a = run (p, 1.0, true, true);
            int live = 0; bool finite = true;
            for (int c = 0; c < 64; ++c)
            {
                if (a.getMagnitude (c, 0, a.getNumSamples()) > 1.0e-6f) live = c + 1;
                for (int i = 0; i < a.getNumSamples(); i += 97) if (! std::isfinite (a.getSample (c, i))) finite = false;
            }
            detail << o << ":" << live << " ";
            if (live != (o + 1) * (o + 1) || ! finite) ok = false;
        }
        check (ok, "channels in use at orders 3 to 7: " + detail);
        // a 16-channel track asked for 7th order gets 3rd
        LeitnerProcessor p; dry (p); setP (p.order, 4.0f);
        const auto a = run (p, 0.3, true, true, 16);
        check (p.orderNow.load() == 3 && a.getMagnitude (13, 0, a.getNumSamples()) > 1.0e-6f   /* (ACN 13: even left to right, so the two mirrored lines add) */, "7th order on a 16-channel track comes out as 3rd");
    }
    {   // the raster: with 4 loudspeakers and no legato, the line only ever sounds from 4 places
        LeitnerProcessor p; dry (p);
        setP (p.figure, (float) leitner::fgCircle); setP (p.points, 4.0f); setP (p.legato, 0.0f); setP (p.seconds, 1.0f);
        const auto a = run (p, 2.0, true, false);
        std::set<int> places; int between = 0, n = 0;
        for (int i = 4800; i + 240 < a.getNumSamples(); i += 240)
        {
            const auto d = directionOf (a, i, 240);
            const float az = std::atan2 (d.y, d.x) * 180.0f / MathConstants<float>::pi;
            const float len = std::sqrt (d.x * d.x + d.y * d.y);
            ++n;
            if (len > 0.97f) places.insert ((int) std::lround (az / 10.0f)); else ++between;
        }
        check (places.size() == 4 && between < n / 5, String::formatted ("4 points, no legato: it sounds from %d places (%d of %d moments in a switch)", (int) places.size(), between, n));
        LeitnerProcessor q; dry (q);
        setP (q.figure, (float) leitner::fgCircle); setP (q.points, (float) leitner::kMaxPoints); setP (q.seconds, 1.0f);
        const auto b = run (q, 2.0, true, false);
        std::set<int> smooth;
        for (int i = 4800; i + 240 < b.getNumSamples(); i += 240) { const auto d = directionOf (b, i, 240); smooth.insert ((int) std::lround (std::atan2 (d.y, d.x) * 18.0f / MathConstants<float>::pi)); }
        check (smooth.size() > 30, String::formatted ("continuous: it passes through %d of 36 directions", (int) smooth.size()));
    }
    {   // Xenakis: a sieve thins the raster; chance makes the passes unequal; with neither, every pass is the same
        LeitnerProcessor p; dry (p);
        setP (p.figure, (float) leitner::fgCircle); setP (p.points, 12.0f); setP (p.legato, 0.0f); setP (p.seconds, 1.0f); setP (p.sieve, 1.0f);
        const auto a = run (p, 2.0, true, false);
        std::set<int> places;
        for (int i = 4800; i + 240 < a.getNumSamples(); i += 240)
        {
            const auto d = directionOf (a, i, 240);
            if (std::sqrt (d.x * d.x + d.y * d.y) > 0.97f) places.insert ((int) std::lround (std::atan2 (d.y, d.x) * 18.0f / MathConstants<float>::pi));
        }
        check (places.size() == 6, String::formatted ("12 points through the sieve 2/0: it sounds from %d places", (int) places.size()));
        float w[leitner::kMaxPoints]; int idx[leitner::kMaxPoints];
        leitner::Params sp; sp.points = 16; sp.sieve = 2;
        const int n = leitner::rasterPoints (sp, w, idx);
        String got; for (int i = 0; i < n; ++i) got << idx[i] << " ";
        check (got == "0 1 3 5 6 9 12 13 15 ", "the sieve 3/0 + 4/1 on 16 points lets through " + got);

        auto passLengths = [] (float chance)
        {
            LeitnerProcessor q; dry (q);
            setP (q.seconds, 0.2f); setP (q.chance, chance);
            q.setPlayConfigDetails (2, 16, SR, BLOCK); q.prepareToPlay (SR, BLOCK);
            AudioBuffer<float> buf (16, BLOCK); MidiBuffer midi;
            std::vector<int> lengths; uint32_t pass = 0; int since = 0;
            for (int b = 0; b < 4000; ++b)
            {
                buf.clear(); q.processBlock (buf, midi); since += BLOCK;
                if (const auto now = q.space.passView.load(); now != pass) { pass = now; lengths.push_back (since); since = 0; }
            }
            return lengths;
        };
        auto spread = [] (const std::vector<int>& v) { int lo = 1 << 30, hi = 0; for (size_t i = 1; i < v.size(); ++i) { lo = std::min (lo, v[i]); hi = std::max (hi, v[i]); } return (double) hi / (double) std::max (1, lo); };
        const auto even = passLengths (0.0f), byChance = passLengths (1.0f);
        check (spread (even) < 1.1 && spread (byChance) > 4.0, String::formatted ("chance 0: longest pass %.2fx the shortest; chance 100 %%: %.1fx", spread (even), spread (byChance)));
    }
    {   // the pulse layer: gaps of silence, a Euclidean pattern, Fell's cells, the rate bending, and line 2 changed by jux
        auto envelope = [] (LeitnerProcessor& p, bool right)
        {
            // how much of the time the omni channel is sounding, in 1 ms steps, and how many separate pulses there were
            const auto a = run (p, 2.0, ! right, right, 16);
            std::vector<int> on;
            for (int i = 0; i + 48 <= a.getNumSamples(); i += 48) on.push_back (a.getMagnitude (0, i, 48) > 0.02f ? 1 : 0);
            return on;
        };
        auto count = [] (const std::vector<int>& on, int from, int to) { int n = 0; for (int i = std::max (1, from); i < to && i < (int) on.size(); ++i) if (on[(size_t) i] && ! on[(size_t) i - 1]) ++n; return n; };
        auto fill = [] (const std::vector<int>& on) { int n = 0; for (int v : on) n += v; return (double) n / (double) on.size(); };
        auto base = [] (LeitnerProcessor& p) { dry (p); setP (p.pulse, 1.0f); setP (p.pulseRate, 10.0f); setP (p.pulseWidth, 0.5f); setP (p.seconds, 1.0f); setP (p.points, (float) leitner::kMaxPoints); };

        LeitnerProcessor a; base (a);
        const auto ea = envelope (a, false);
        check (std::abs (count (ea, 0, 2000) - 20) <= 1 && std::abs (fill (ea) - 0.5) < 0.08, String::formatted ("10 pulses a second, half width: %d pulses in 2 s, sounding %.0f %% of the time", count (ea, 0, 2000), 100.0 * fill (ea)));

        LeitnerProcessor b; base (b); setP (b.hits, 3.0f); setP (b.steps, 8.0f);
        const auto eb = envelope (b, false);
        check (std::abs (count (eb, 0, 2000) - 8) <= 1, String::formatted ("3 hits in 8 steps: %d pulses in 2 s (20 without the pattern)", count (eb, 0, 2000)));

        LeitnerProcessor c; base (c); setP (c.pulseBend, 1.0f);
        const auto ec = envelope (c, false);
        check (count (ec, 0, 300) * 3 < count (ec, 700, 1000), String::formatted ("bend up: %d pulses in the first 0.3 s of a pass, %d in the last 0.3 s", count (ec, 0, 300), count (ec, 700, 1000)));

        // Fell's cell 7 1 1 3: gaps in that proportion
        leitner::Params sp; sp.cell = 6; sp.pulseRate = 10.0f;
        String gaps; for (int k = 0; k < 4; ++k) gaps << roundToInt (leitner::pulseGap (sp, 0, 0, k, 0.5f) * 1000.0f) << " ";
        check (gaps == "233 33 33 100 ", "the cell 7 1 1 3 at 10 a second gives gaps of " + gaps + "ms");
        sp.rotate = 1;
        check (std::abs (leitner::pulseGap (sp, 0, 1, 0, 0.5f) - 0.0333f) < 0.001f, "rotate 1: on the next pass the cell starts a step further on");

        LeitnerProcessor d; base (d); setP (d.hits, 3.0f); setP (d.steps, 8.0f); setP (d.jux, 1.0f);
        const auto e1 = envelope (d, false);
        LeitnerProcessor d2; base (d2); setP (d2.hits, 3.0f); setP (d2.steps, 8.0f); setP (d2.jux, 1.0f);
        const auto e2 = envelope (d2, true);
        int differ = 0; for (size_t i = 0; i < std::min (e1.size(), e2.size()); ++i) differ += e1[i] != e2[i] ? 1 : 0;
        check (differ > 100, String::formatted ("jux backwards: line 2 differs from line 1 for %d ms of 2 s", differ));

        // each pulse sounds from one place: the direction doesn't move while a pulse lasts
        LeitnerProcessor f; base (f); setP (f.figure, (float) leitner::fgCircle); setP (f.pulseRate, 4.0f); setP (f.pulseWidth, 0.6f);
        const auto g = run (f, 1.0, true, false, 16);
        const auto d0 = directionOf (g, 24000 + 480, 2400), d1 = directionOf (g, 24000 + 4800, 2400);   // early and late in the pulse that starts at 0.5 s
        check (std::abs (d0.x - d1.x) < 0.02f && std::abs (d0.y - d1.y) < 0.02f, String::formatted ("a pulse stays put: (%.2f, %.2f) at its start, (%.2f, %.2f) 90 ms on", d0.x, d0.y, d1.x, d1.y));
    }
    {   // every figure at 7th order with the room: finite, and how fast
        bool ok = true; double slowest = 1.0e9; float peak = 0.0f;
        for (int f = 0; f < leitner::numFigures; ++f)
        {
            LeitnerProcessor p;
            setP (p.order, 4.0f); setP (p.figure, (float) f); setP (p.room, 0.5f); setP (p.seconds, 0.7f); setP (p.swell, 0.6f); setP (p.lift, 0.6f); setP (p.counter, 1.0f);
            double rt = 0;
            const auto a = run (p, 2.0, true, true, 64, &rt);
            slowest = std::min (slowest, rt);
            for (int c = 0; c < 64; ++c) { peak = std::max (peak, a.getMagnitude (c, 0, a.getNumSamples())); for (int i = 0; i < a.getNumSamples(); i += 61) if (! std::isfinite (a.getSample (c, i))) ok = false; }
        }
        check (ok && slowest > 3.0 && peak < 2.0f, String::formatted ("all %d figures at 7th order with the room: finite, peak %.2f, slowest %.1fx realtime", (int) leitner::numFigures, peak, slowest));
    }
    {   // the programs: every one of Leitner's sounds and stays finite; a sound program leaves the space alone; your own presets
        bool ok = true; String quiet;
        LeitnerProcessor names;
        for (int k = 0; k < LeitnerProcessor::numSpacePrograms(); ++k)
        {
            LeitnerProcessor p; p.setCurrentProgram (k);
            const auto a = run (p, 3.0, true, true, 16);
            bool finite = true; float peak = 0.0f;
            for (int c = 0; c < 16; ++c) { peak = std::max (peak, a.getMagnitude (c, 0, a.getNumSamples())); for (int i = 0; i < a.getNumSamples(); i += 53) if (! std::isfinite (a.getSample (c, i))) finite = false; }
            if (! finite || peak < 0.02f || peak > 1.5f) { ok = false; quiet << p.getProgramName (k) << String::formatted (" (peak %.3f) ", peak); }
        }
        check (ok, String::formatted ("all %d of Leitner's programs sound and stay finite ", LeitnerProcessor::numSpacePrograms()) + quiet);
        LeitnerProcessor p;
        p.setCurrentProgram (2);                                             // Ton-Tor
        check (p.figure->getIndex() == leitner::fgGate && p.getProgramName (p.getCurrentProgram()) == "Ton-Tor", "a program sets its figure");
        setP (p.order, 2.0f);
        p.setCurrentProgram (LeitnerProcessor::numSpacePrograms() + 5);      // one of Cloudfin's
        check (p.figure->getIndex() == leitner::fgGate && p.order->getIndex() == 2, "a sound-only program leaves the space and the order alone");
        p.setCurrentProgram (11);
        check (p.order->getIndex() == 2 && p.figure->getIndex() == leitner::fgPolytope, "one of Leitner's keeps the order you set");

        const auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("leitner-preset-test");
        dir.deleteRecursively();
        setP (p.turn, 77.0f); setP (p.apvts.getParameter (ids::dlMix), 0.66f);
        check (p.saveUserPreset (dir.getChildFile ("mine")) && dir.getChildFile ("mine.leitner").existsAsFile(), "a preset saves to a file");
        LeitnerProcessor q;
        check (q.loadUserPreset (dir.getChildFile ("mine.leitner")) && std::abs (q.turn->get() - 77.0f) < 0.5f && q.figure->getIndex() == leitner::fgPolytope
               && std::abs (q.apvts.getRawParameterValue (ids::dlMix)->load() - 0.66f) < 0.01f, "and loads back, the space and the sound");
        dir.deleteRecursively();
        check (names.userPresetFolder().getFullPathName().contains ("Leitner"), "your presets live in " + names.userPresetFolder().getFullPathName());
    }
    {   // the state, and a stereo track
        LeitnerProcessor p;
        setP (p.figure, (float) leitner::fgGate); setP (p.order, 2.0f); setP (p.points, 12.0f); setP (p.lift, 0.7f); setP (p.apvts.getParameter (ids::rvMix), 0.61f);
        MemoryBlock mb; p.getStateInformation (mb);
        LeitnerProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (q.figure->getIndex() == leitner::fgGate && q.order->getIndex() == 2 && q.points->get() == 12 && std::abs (q.lift->get() - 0.7f) < 0.01f
               && std::abs (q.apvts.getRawParameterValue (ids::rvMix)->load() - 0.61f) < 0.01f, "the space and Cloudfin's settings come back with the session");
        LeitnerProcessor s; dry (s);
        const auto a = run (s, 0.5, true, true, 2);
        check (a.getMagnitude (0, 0, a.getNumSamples()) > 0.05f && s.orderNow.load() == 0, "on a stereo track a left-right fold comes out");
    }
    {   // the window: from the front, turned, and one of Xenakis's figures; and how nearly its two halves mirror
        struct Shot { int figure; float yaw, pitch, scatter; const char* name; };
        const Shot shots[] = { { leitner::fgGate, 0.0f, 0.0f, 0.0f, "front" }, { leitner::fgSoundcube, 38.0f, 24.0f, 0.0f, "turned" },
                               { leitner::fgParaboloid, -30.0f, 18.0f, 0.0f, "paraboloid" }, { leitner::fgPolytope, 25.0f, 30.0f, 0.4f, "polytope" }, { leitner::fgCircle, 30.0f, 35.0f, 0.0f, "pulses" } };
        for (auto& sh : shots)
        {
            LeitnerProcessor p;
            setP (p.figure, (float) sh.figure); setP (p.seconds, 3.0f); setP (p.points, 12.0f); setP (p.scatter, sh.scatter);
            if (sh.figure == leitner::fgGate) setP (p.counter, 1.0f);
            if (String (sh.name) == "pulses") { setP (p.pulse, 1.0f); setP (p.cell, 6.0f); setP (p.hits, 5.0f); setP (p.steps, 8.0f); setP (p.pulseBend, 0.4f); setP (p.jux, 1.0f); setP (p.seconds, 4.0f); }
            setP (p.apvts.getParameter (ids::att (ids::grSize)), 0.5f);
            p.viewYaw.store (sh.yaw); p.viewPitch.store (sh.pitch);
            run (p, 0.8, true, true);
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            ed->setSize (1500, 790);
            ed->setVisible (true);
            MessageManager::getInstance()->runDispatchLoopUntil (150);
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
            const auto f = outDir.getChildFile ("leitner_" + String (sh.name) + ".png"); f.deleteFile();
            FileOutputStream os (f);
            PNGImageFormat().writeImageToStream (img, os);
            if (sh.yaw == 0.0f)
            {
                // ink on the left half against ink on the right half, in 30-pixel cells: text can't mirror letter for
                // letter, but where there is ink on one side there should be ink on the other
                const int W = img.getWidth(), H = img.getHeight(), cell = 30;
                int same = 0, total = 0;
                for (int cy = 0; cy + cell <= H; cy += cell)
                    for (int cx = 0; cx + cell <= W / 2; cx += cell)
                    {
                        int l = 0, r = 0;
                        for (int y = cy; y < cy + cell; y += 2)
                            for (int x = cx; x < cx + cell; x += 2)
                            {
                                if (img.getPixelAt (x, y).getBrightness() < 0.6f) ++l;
                                if (img.getPixelAt (W - 1 - x, y).getBrightness() < 0.6f) ++r;
                            }
                        ++total; if ((l > 2) == (r > 2)) ++same;
                    }
                check (same > total * 88 / 100, String::formatted ("the window mirrors: %d of %d cells have ink on both sides or neither", same, total));
            }
        }
    }
    std::printf ("\n%s (%d failed)\n", failures == 0 ? "all good" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
