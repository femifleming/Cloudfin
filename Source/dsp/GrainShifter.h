#pragma once
#include "Utils.h"
#include <array>

namespace fp
{
//==============================================================================
// Live granular shifter.
//
// * Records continuously into 4-second stereo "tapes". There are two tapes so a
//   freeze can be captured on one while live grains keep playing from the other;
//   the frozen cloud fades in (equal-power) over the Freeze Fade time.
// * Each tape is written in 3 bands (full, and anti-aliased copies for +1 and +2
//   octaves) so octave-up grains don't alias.
// * The grain envelope is a user-drawn table (kShapeN points). Each grain takes
//   its own copy when it is born, so redrawing never clicks grains in flight,
//   and a 3 ms raised-cosine taper at both ends keeps any drawing click-free.
// * Each grain can play backwards (reverse chance) and gets a random pan (width).
// * Size, density, position, spray and stereo width are knobs (the plugin's
//   attenuverters add the random motion). Grains never cross the record head or
//   a freeze seam, so there are no discontinuities.
// * Output level is normalised for density and envelope shape.
class GrainShifter
{
public:
    static constexpr int kShapeN = 128;

    enum ShapeType { saw = 0, bell, triangle, reverse, pulse, numShapes };

    static void makeShape (int type, float* t) noexcept
    {
        for (int i = 0; i < kShapeN; ++i)
        {
            const float x = (float) i / (float) (kShapeN - 1);
            float v = 0.0f;
            switch (type)
            {
                case saw:      v = x * (x > 0.88f ? 0.5f * (1.0f + std::cos ((float) kPi * (x - 0.88f) / 0.12f)) : 1.0f); break;
                case bell:     v = 0.5f - 0.5f * std::cos ((float) kTwoPi * x); break;
                case triangle: v = 1.0f - std::abs (2.0f * x - 1.0f); break;
                case reverse:  { const float y = 1.0f - x; v = y * (y > 0.88f ? 0.5f * (1.0f + std::cos ((float) kPi * (y - 0.88f) / 0.12f)) : 1.0f); } break;
                case pulse:    v = smoothstep (x / 0.15f) * smoothstep ((1.0f - x) / 0.15f); break;
                default:       v = 1.0f; break;
            }
            t[i] = std::clamp (v, 0.0f, 1.0f);
        }
    }

    void prepare (double sampleRate, uint64_t seed = 0x51ED)
    {
        sr = sampleRate;
        N  = (int) (4.0 * sr);
        rng.seed (seed);

        for (auto& t : tapes)
        {
            for (auto& b : t.data) for (auto& c : b) c.assign ((size_t) N, 0.0f);
            t.recording = true; t.validLen = 0; t.heldAt = 0;
        }
        for (int c = 0; c < 2; ++c)
        {
            aa1[c].setup (sr, sr * 0.22);
            aa2[c].setup (sr, sr * 0.11);
        }
        if (! shapeSet) { makeShape (saw, shape.data()); updateShapeStats(); }
        gainCoef = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
        margin = 16.0;
        reset();
    }

    void reset()
    {
        for (auto& t : tapes)
        {
            for (auto& b : t.data) for (auto& c : b) std::fill (c.begin(), c.end(), 0.0f);
            t.recording = true; t.validLen = 0;
        }
        for (auto& g : grains) g.active = false;
        for (int c = 0; c < 2; ++c) { aa1[c].reset(); aa2[c].reset(); }
        w = 0; frozen = false; heldTape = -1; x = 0.0f;
        nextSpawn[0] = nextSpawn[1] = 0;
        oct.cur = oct.prev = 0; oct.q = 1.0f;
        gainSm = targetGain();
    }

    // --- controls (call at control rate) -------------------------------------
    void setShape (const float* t) noexcept
    {
        for (int i = 0; i < kShapeN; ++i) shape[(size_t) i] = std::clamp (t[i], 0.0f, 1.0f);
        shapeSet = true;
        updateShapeStats();
    }

    void setGrainParams (float sizeMs, float densityOverlap, float position, float spray, float width, float reverseChance) noexcept
    {
        reverseAmt = std::clamp (reverseChance, 0.0f, 1.0f);
        sizeSec  = std::clamp (sizeMs, 5.0f, 4000.0f) * 0.001f;
        density  = std::clamp (densityOverlap, 0.5f, 32.0f);
        posParam = std::clamp (position, 0.0f, 1.0f);
        sprayAmt = std::clamp (spray, 0.0f, 1.0f);
        widthAmt = std::clamp (width, 0.0f, 1.0f);
    }

    void setFadeTime (float seconds) noexcept { xInc = 1.0f / (float) std::max (1.0, seconds * sr); }
    void setOctave (int o, bool randomMode) noexcept { oct.manual = std::clamp (o, -2, 2); oct.random = randomMode; }

    void setFreeze (bool on) noexcept
    {
        if (on == frozen) return;
        frozen = on;
        if (on && heldTape < 0)
        {
            // hold the recording tape that has the most audio in it
            heldTape = tapes[0].validLen >= tapes[1].validLen ? 0 : 1;
            auto& t = tapes[(size_t) heldTape];
            t.recording = false;
            t.heldAt = w;
            // any live grain on that tape that would run past the new seam fades out quickly
            for (auto& g : grains)
                if (g.active && g.tape == heldTape)
                {
                    const double age = wrapAge ((double) w - g.pos);
                    const double endAge = age - g.rate * (double) (g.len - g.age);
                    if (endAge < margin || endAge > (double) t.validLen - margin) g.startKill (sr);
                }
        }
    }

    bool isFrozen() const noexcept { return frozen; }
    float getFreezeAmount() const noexcept { return x; }

    // --- audio ---------------------------------------------------------------
    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        // record
        const float in[2] = { inL, inR };
        float b1[2], b2[2];
        for (int c = 0; c < 2; ++c) { b1[c] = aa1[c].process (in[c]); b2[c] = aa2[c].process (in[c]); }
        for (auto& t : tapes)
            if (t.recording)
            {
                for (int c = 0; c < 2; ++c)
                {
                    t.data[0][(size_t) c][(size_t) w] = in[c];
                    t.data[1][(size_t) c][(size_t) w] = b1[c];
                    t.data[2][(size_t) c][(size_t) w] = b2[c];
                }
                t.validLen = std::min (N, t.validLen + 1);
            }
        w = (w + 1 == N) ? 0 : w + 1;

        // freeze crossfade
        if (frozen) x = std::min (1.0f, x + xInc);
        else        x = std::max (0.0f, x - xInc);

        if (! frozen && x <= 0.0f && heldTape >= 0)
        {
            for (auto& g : grains) if (g.active && ! g.live) g.active = false;
            auto& t = tapes[(size_t) heldTape];
            t.recording = true; t.validLen = 0;
            heldTape = -1;
        }

        const float layerGain[2] = { std::cos (x * (float) kPi * 0.5f), std::sin (x * (float) kPi * 0.5f) };

        oct.tick (rng, sr);

        // schedulers: [0] live cloud, [1] frozen cloud
        for (int layer = 0; layer < 2; ++layer)
        {
            if (--nextSpawn[layer] > 0) continue;
            nextSpawn[layer] = (int) (0.005 * sr); // retry soon if we can't spawn
            if (layerGain[layer] < 1.0e-4f) continue;
            if (layer == 1 && heldTape < 0) continue;
            spawn (layer == 0);
        }

        // render
        float accL = 0.0f, accR = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const auto& t = tapes[(size_t) g.tape];
            const float env = g.envelope() * layerGain[g.live ? 0 : 1];
            const float l = readTape (t, g.band, 0, g.pos);
            const float r = readTape (t, g.band, 1, g.pos);
            accL += env * g.gl * l;
            accR += env * g.gr * r;

            g.pos += g.rate;
            if (g.pos >= (double) N) g.pos -= (double) N;
            else if (g.pos < 0.0) g.pos += (double) N;
            if (++g.age >= g.len || g.killDone()) g.active = false;
        }
        gainSm += gainCoef * (targetGain() - gainSm);
        outL = accL * gainSm;
        outR = accR * gainSm;
    }

private:
    struct Tape
    {
        std::array<std::array<std::vector<float>, 2>, 3> data; // [band][channel]
        bool recording = true;
        int validLen = 0, heldAt = 0;
    };

    struct Grain
    {
        bool active = false, live = true;
        int tape = 0, band = 0, len = 1, age = 0, taper = 1;
        double pos = 0.0, rate = 1.0;
        float gl = 1.0f, gr = 1.0f;
        float kill = 1.0f, killStep = 0.0f;
        std::array<float, kShapeN> env {};

        float envelope() noexcept
        {
            const float fi = (float) age / (float) len * (float) (kShapeN - 1);
            const int   i  = std::min ((int) fi, kShapeN - 2);
            const float fr = fi - (float) i;
            float e = env[(size_t) i] + fr * (env[(size_t) i + 1] - env[(size_t) i]);
            if (age < taper)
                e *= 0.5f - 0.5f * std::cos ((float) kPi * (float) age / (float) taper);
            const int rem = len - age;
            if (rem < taper)
                e *= 0.5f - 0.5f * std::cos ((float) kPi * (float) rem / (float) taper);
            if (killStep > 0.0f) { kill = std::max (0.0f, kill - killStep); e *= kill; }
            return e;
        }
        void startKill (double sr) { if (killStep == 0.0f) killStep = 1.0f / (float) (0.006 * sr); }
        bool killDone() const noexcept { return killStep > 0.0f && kill <= 0.0f; }
    };

    struct OctaveSelector
    {
        int cur = 0, prev = 0, manual = 0, hold = 0;
        bool random = false;
        float q = 1.0f, qInc = 0.0f;

        void moveTo (int o, float slewSec, double sr)
        {
            if (o == cur) return;
            prev = (q >= 0.5f) ? cur : prev;
            cur  = o;
            q    = 0.0f;
            qInc = 1.0f / (float) std::max (1.0, slewSec * sr);
        }

        void tick (Rng& rng, double sr) noexcept
        {
            if (q < 1.0f) q = std::min (1.0f, q + qInc);
            if (random)
            {
                if (--hold <= 0)
                {
                    hold = (int) (sr * rng.range (0.4f, 5.0f));                  // random timing
                    const int o = std::clamp (manual + rng.integer (3) - 1, -2, 2);
                    moveTo (o, 0.04f * std::pow (60.0f, rng.uni()), sr);        // random slew 40ms..2.4s
                }
            }
            else
            {
                moveTo (manual, 0.12f, sr);
            }
        }

        // slew between octaves = probability that new grains use the new octave
        int pick (Rng& rng) noexcept { return rng.uni() < smoothstep (q) ? cur : prev; }
    };

    void updateShapeStats() noexcept
    {
        double s = 0.0;
        for (auto v : shape) s += (double) v * (double) v;
        shapeMeanSq = (float) (s / kShapeN);
    }

    // incoherent grains: summed power ~ overlap * mean(env^2)
    float targetGain() const noexcept
    {
        return std::min (8.0f, 1.3f / std::sqrt (std::max (0.02f, (float) density * shapeMeanSq)));
    }

    double wrapAge (double a) const noexcept
    {
        while (a < 0.0) a += (double) N;
        while (a >= (double) N) a -= (double) N;
        return a;
    }

    float readTape (const Tape& t, int band, int c, double pos) const noexcept
    {
        const auto& d = t.data[(size_t) band][(size_t) c];
        const int i = (int) pos;
        const float f = (float) (pos - (double) i);
        auto at = [&] (int k) { k += i; if (k < 0) k += N; else if (k >= N) k -= N; return d[(size_t) k]; };
        return hermite (at (-1), at (0), at (1), at (2), f);
    }

    void spawn (bool live) noexcept
    {
        int ti;
        if (live)
        {
            ti = -1;
            for (int i = 0; i < 2; ++i)
                if (tapes[(size_t) i].recording && (ti < 0 || tapes[(size_t) i].validLen > tapes[(size_t) ti].validLen))
                    ti = i;
            if (ti < 0) return;
        }
        else ti = heldTape;

        const auto& t = tapes[(size_t) ti];
        if (t.validLen < (int) (0.05 * sr)) return;

        Grain* g = nullptr;
        for (auto& c : grains) if (! c.active) { g = &c; break; }
        if (g == nullptr) return;

        const int o = std::clamp (oct.pick (rng), -2, 2);
        const bool backwards = rng.uni() < reverseAmt;     // reverse grains read the tape backwards
        const double rate = std::exp2 ((double) o) * (backwards ? -1.0 : 1.0);

        double L = (double) sizeSec * (double) rng.range (0.85f, 1.15f) * sr;

        const double headSpeed = t.recording ? 1.0 : 0.0;
        const double drift = (headSpeed - rate) * L;   // how far the age moves during the grain
        const double usable = (double) t.validLen - 2.0 * margin;
        if (std::abs (drift) > usable * 0.9)
            L = usable * 0.9 / std::max (1.0e-6, std::abs (headSpeed - rate));
        L = std::min (L, usable * 0.95);
        if (L < 0.004 * sr) return;
        const double d2 = (headSpeed - rate) * L;

        const double lo = margin + std::max (0.0, -d2);
        const double hi = (double) t.validLen - margin - std::max (0.0, d2);
        if (hi <= lo) return;

        const double v = std::clamp ((double) posParam + 0.5 * (double) sprayAmt * (double) rng.bi() + 0.01 * (double) rng.bi(), 0.0, 1.0);
        const double age0 = lo + v * (hi - lo);
        const double head = t.recording ? (double) w : (double) t.heldAt;

        g->active  = true;
        g->live    = live;
        g->tape    = ti;
        g->band    = o >= 2 ? 2 : (o == 1 ? 1 : 0);
        g->rate    = rate;
        g->len     = std::max (4, (int) L);
        g->age     = 0;
        g->taper   = std::clamp ((int) (0.003 * sr), 2, std::max (2, g->len / 4));
        g->pos     = wrapAge (head - age0);
        g->kill    = 1.0f; g->killStep = 0.0f;
        g->env     = shape;
        const float pan = rng.bi() * widthAmt;
        g->gl = 1.0f - std::max (0.0f, pan);
        g->gr = 1.0f + std::min (0.0f, pan);

        nextSpawn[live ? 0 : 1] = std::max (1, (int) (L / (double) density * (double) rng.range (0.6f, 1.4f)));
    }

    double sr = 48000.0, margin = 16.0;
    int N = 192000, w = 0;
    std::array<Tape, 2> tapes;
    std::array<Grain, 128> grains;
    std::array<float, kShapeN> shape {};
    bool shapeSet = false;
    float shapeMeanSq = 0.3f;
    Butter8LP aa1[2], aa2[2];
    Rng rng;
    OctaveSelector oct;
    bool frozen = false;
    int heldTape = -1;
    float x = 0.0f, xInc = 1.0f / 48000.0f;
    int nextSpawn[2] {};
    float sizeSec = 0.09f, density = 7.0f, posParam = 0.25f, sprayAmt = 0.1f, widthAmt = 0.45f, reverseAmt = 0.0f;
    float gainSm = 0.9f, gainCoef = 0.001f;
};
} // namespace fp
