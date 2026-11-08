#pragma once
#include "Utils.h"

namespace fp
{
//==============================================================================
// Zero-latency delay-line pitch shifter ("rotating tape heads"): two read heads,
// half a window (the grain) apart, each faded with sin^2 so their gains always
// sum to 1. Hermite interpolation on reads. Feedback re-injects the shifted
// signal into the line, so each repeat climbs/falls by another scale step.
//
// Grain size is seamless: there are two complete shifter voices on the same
// delay line. Changing the grain never stretches the running voice (that would
// bend the pitch); instead the idle voice starts at the new grain size and the
// two are crossfaded over 50 ms. Continuous modulation simply chains fades.
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        minWin = (float) (0.004 * sr);
        maxWin = (float) (0.400 * sr);
        for (auto& d : line) d.prepare ((int) (maxWin + minDelay + 16));
        for (int c = 0; c < 2; ++c)
        {
            fbLP[c].setup (sr, 9000.0);
            fbHP[c].setHighpass (sr, 60.0, 0.707);
            dc[c].setup (sr, 8.0);
        }
        fadeInc = (float) (1.0 / (0.050 * sr));
        targetWin = std::clamp ((float) (0.065 * sr), minWin, maxWin);
        logRatio.setup (sr, 0.02);
        logRatio.snap (0.0f);
        reset();
    }

    void reset()
    {
        for (auto& d : line) d.reset();
        for (int c = 0; c < 2; ++c) { fbLP[c].reset(); fbHP[c].reset(); dc[c].reset(); fbState[c] = 0.0f; }
        for (auto& v : voice) { v.phase = 0.0f; v.win = targetWin; }
        active = 0; fading = false; xf = 0.0f;
    }

    // ratio as log2 (octaves). Glides over 20 ms so scale steps never click.
    void setPitchOctaves (float oct) noexcept { logRatio.set (oct); }

    // grain (window) size; changes are crossfaded, never glided
    void setGrainSeconds (float s) noexcept { targetWin = std::clamp ((float) (s * sr), minWin, maxWin); }

    void process (float inL, float inR, float feedback, float& outL, float& outR) noexcept
    {
        const float ratio = std::exp2 (logRatio.next());

        const float in[2] = { inL, inR };
        for (int c = 0; c < 2; ++c)
            line[c].push (in[c] + feedback * fbState[c]);

        if (! fading)
        {
            const float cw = voice[active].win;
            if (std::abs (targetWin - cw) > std::max (2.0f, cw * 0.004f))
            {
                auto& o = voice[1 - active];
                o.win = targetWin;
                o.phase = alignedPhase (voice[active], targetWin);
                fading = true;
                xf = 0.0f;
            }
        }

        float out[2];
        run (voice[active], ratio, out);
        if (fading)
        {
            float nw[2];
            run (voice[1 - active], ratio, nw);
            xf = std::min (1.0f, xf + fadeInc);
            const float s = std::sin (xf * (float) kPi * 0.5f);
            const float gNew = s * s;
            for (int c = 0; c < 2; ++c) out[c] += gNew * (nw[c] - out[c]);
            if (xf >= 1.0f) { active = 1 - active; fading = false; }
        }

        for (int c = 0; c < 2; ++c)
        {
            // feedback path: gentle band-limit + saturation keeps it musical and bounded
            const float f = fbHP[c].process (fbLP[c].process (dc[c].process (out[c])));
            fbState[c] = softClip (f);
        }
        outL = out[0]; outR = out[1];
    }

private:
    struct Voice { float phase = 0.0f, win = 3000.0f; };

    // Phase for a new voice so that one of its heads reads exactly where the old
    // voice's loudest head reads. Both heads then travel at the same speed, so the
    // crossfade is between (nearly) identical signals: no phasing, no pitch wobble.
    float alignedPhase (const Voice& old, float newWin) const noexcept
    {
        float p2 = old.phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        const float s1 = std::sin ((float) kPi * old.phase);
        const float dom = (s1 * s1 >= 0.5f) ? old.phase : p2;     // loudest head's phase
        const float rel = dom * old.win / newWin;                 // that delay, in new-window units
        if (rel >= 1.0f) return old.phase;                        // can't reach it: plain crossfade
        // a head at phase 'rel' reads the same sample; put head 1 or head 2 there
        // (both give that head the same gain, sin^2(pi*rel)), keep head 1's phase in [0,1)
        return rel;
    }

    void run (Voice& v, float ratio, float* out) noexcept
    {
        // heads move through the window at (1 - ratio) samples per sample
        v.phase += (1.0f - ratio) / v.win;
        v.phase -= std::floor (v.phase);
        float p2 = v.phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;

        const float d1 = minDelay + v.phase * v.win;
        const float d2 = minDelay + p2 * v.win;
        const float s1 = std::sin ((float) kPi * v.phase);
        const float s2 = std::sin ((float) kPi * p2);
        const float g1 = s1 * s1, g2 = s2 * s2; // g1 + g2 == 1
        for (int c = 0; c < 2; ++c)
            out[c] = g1 * line[c].read (d1) + g2 * line[c].read (d2);
    }

    double sr = 48000.0;
    float minDelay = 4.0f, minWin = 200.0f, maxWin = 20000.0f, targetWin = 3000.0f;
    Voice voice[2];
    int active = 0;
    bool fading = false;
    float xf = 0.0f, fadeInc = 0.001f;
    DelayLine line[2];
    OnePoleLP fbLP[2];
    Biquad fbHP[2];
    DCBlocker dc[2];
    float fbState[2] {};
    Ramp logRatio;
};
} // namespace fp
