#pragma once
#include "SpectralGate.h"
#include "PitchShifter.h"
#include "FormantFilter.h"
#include "GrainShifter.h"
#include "StereoDelay.h"
#include "FDNReverb.h"
#include "Compander.h"
#include "Scales.h"
#include <array>

namespace fp
{
struct EngineParams
{
    // spectral gate / smear
    float sgCutoffDb = -24.0f, sgAttackMs = 45.0f, sgSmearS = 1.8f, sgKnee = 4.0f;
    float sgBlur = 0.0f, sgScatter = 0.0f, sgTilt = 0.0f, sgMix = 0.35f;
    bool sgFreeze = false;
    // pitch shifter
    float psShiftSemis = 7.0f, psGrainMs = 65.0f, psFeedback = 0.35f, psMix = 0.3f; int psScale = 0;
    // formant
    float fmVowel = 0.0f, fmShift = 0.0f, fmRes = 1.0f, fmMix = 0.2f;
    // grains
    float grSizeMs = 90.0f, grDensity = 7.0f, grPosition = 0.25f, grSpray = 0.1f, grWidth = 0.45f, grReverse = 0.0f;
    float grOctave = 0.0f, grFade = 1.0f, grMix = 0.4f;
    bool grRandOct = true, grFreeze = false;
    // stereo JI delay
    float dlTimeMs = 380.0f, dlFeedback = 0.35f, dlTone = 5000.0f, dlMix = 0.2f;
    // reverb
    float rvDecay = 4.0f, rvTone = 0.0f, rvMix = 0.3f;
    // compander
    float cpThreshDb = -18.0f, cpRatio = 2.0f, cpBelow = 0.0f, cpAttackMs = 10.0f, cpReleaseMs = 250.0f;
    float cpMakeupDb = 0.0f, cpMix = 1.0f;
    // main loop
    float masterFb = 0.0f;
};

//==============================================================================
// Module order. Any permutation of the seven stages is allowed.
enum Stage { stGate = 0, stPitch, stFormant, stGrains, stDelay, stReverb, stComp, numStages };
using StageOrder = std::array<int, numStages>;

inline StageOrder defaultOrder() noexcept { return { stGate, stPitch, stFormant, stGrains, stDelay, stReverb, stComp }; }

inline bool isValidOrder (const StageOrder& o) noexcept
{
    int seen = 0;
    for (int v : o) { if (v < 0 || v >= numStages || (seen & (1 << v))) return false; seen |= 1 << v; }
    return true;
}

//==============================================================================
// The whole Cloudfin chain, processed sample by sample so the global
// output -> input feedback loop has no block-size dependence.
//   in (+ feedback) -> spectral gate -> pitch shifter -> formant -> grains
//                   -> stereo JI delay -> HP FDN verb -> compander -> out
// (that's the default; the seven stages can be put in any order. A new order is
// applied by dipping the chain to silence over 20 ms, swapping, and fading back,
// so reordering never clicks.)
// Every dry/wet is equal-power (the compander is a linear blend) and every
// control is ramped. With all mixes at 0 and feedback at 0 the output is
// bit-identical to the input.
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        sg.prepare (sr);
        ps.prepare (sr);
        fm.prepare (sr);
        gr.prepare (sr);
        dl.prepare (sr);
        rv.prepare (sr);
        cp.prepare (sr);

        for (auto* r : { &sgMix, &psMix, &psFb, &fmMix, &grMix, &dlMix, &dlFb, &rvMix, &cpMix, &cpMakeup, &masterFb })
            r->setup (sr, 0.04);

        for (int c = 0; c < 2; ++c)
        {
            fbDC[c].setup (sr, 15.0);
            fbHP[c].setHighpass (sr, 70.0, 0.707);
            fbLP[c].setup (sr, 14000.0);
        }
        dipStep = (float) (1.0 / (0.020 * sr));
        fbAtk = (float) (1.0 - std::exp (-1.0 / (0.004 * sr)));
        fbRel = (float) (1.0 - std::exp (-1.0 / (0.350 * sr)));
        applyParams();
        snapToParams();
        order = pendingOrder;
        dip = 1.0f;
        prepared = true;
        reset();
    }

    void reset()
    {
        sg.reset(); ps.reset(); fm.reset(); gr.reset(); dl.reset(); rv.reset(); cp.reset();
        for (int c = 0; c < 2; ++c) { fbDC[c].reset(); fbHP[c].reset(); fbLP[c].reset(); fbState[c] = 0.0f; }
        fbEnv = 0.0f;
    }

    void setParams (const EngineParams& p)
    {
        params = p;
        applyParams();
    }

    void setGrainShape (const float* table) noexcept { gr.setShape (table); }

    void setOrder (const StageOrder& o) noexcept
    {
        if (! isValidOrder (o)) return;
        if (! prepared) { order = pendingOrder = o; return; }
        pendingOrder = o;
    }
    const StageOrder& getOrder() const noexcept { return order; }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            // reorder: dip to silence, swap at the bottom, come back up
            if (pendingOrder != order || dip < 1.0f)
            {
                if (pendingOrder != order)
                {
                    dip = std::max (0.0f, dip - dipStep);
                    if (dip <= 0.0f) order = pendingOrder;
                }
                else dip = std::min (1.0f, dip + dipStep);
            }
            const float dg = dip < 1.0f ? smoothstep (dip) : 1.0f;

            const float fb = masterFb.next();
            float l = inL[i] + fb * fbState[0];
            float r = inR[i] + fb * fbState[1];

            for (int st : order)
            {
                if (dg < 1.0f) { l *= dg; r *= dg; }
                runStage (st, l, r);
            }
            if (dg < 1.0f) { l *= dg; r *= dg; }

            // safety: never let a NaN/inf escape (and never let it poison the loop)
            if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0.0f; reset(); }

            outL[i] = l; outR[i] = r;

            // global feedback: DC-blocked, band-limited, soft-saturated so it can sing but never explode
            float f[2] = { l, r };
            for (int c = 0; c < 2; ++c)
                f[c] = fbLP[c].process (fbHP[c].process (fbDC[c].process (f[c])));
            // level-riding compressor in the loop: regeneration stays near -10 dBFS
            const float pk = std::max (std::abs (f[0]), std::abs (f[1]));
            fbEnv += (pk > fbEnv ? fbAtk : fbRel) * (pk - fbEnv);
            fbEnv = flushDenormal (fbEnv);
            const float g = fbEnv > fbThresh ? fbThresh / fbEnv : 1.0f;
            for (int c = 0; c < 2; ++c)
                fbState[c] = softClip (f[c] * g);
        }
    }

    float getQuantisedCents() const noexcept { return quantisedCents; }
    int getQuantisedDegree() const noexcept { return quantisedDegree; }
    int getQuantisedPeriod() const noexcept { return quantisedPeriod; }
    float getFreezeAmount() const noexcept { return gr.getFreezeAmount(); }
    float getCompGainDb() const noexcept { return cp.getGainDb(); }
    float getSpectralFreezeAmount() const noexcept { return sg.getFreezeAmount(); }

private:
    // every stage advances every sample (wherever it sits) so its ramps and tails stay continuous
    void runStage (int st, float& l, float& r) noexcept
    {
        float wl, wr;
        switch (st)
        {
            case stGate:    sg.process (l, r, wl, wr);              mix (sgMix.next(), l, r, wl, wr); break;
            case stPitch:   ps.process (l, r, psFb.next(), wl, wr); mix (psMix.next(), l, r, wl, wr); break;
            case stFormant: fm.process (l, r, wl, wr);              mix (fmMix.next(), l, r, wl, wr); break;
            case stGrains:  gr.process (l, r, wl, wr);              mix (grMix.next(), l, r, wl, wr); break;
            case stDelay:   dl.process (l, r, dlFb.next(), wl, wr); mix (dlMix.next(), l, r, wl, wr); break;
            case stReverb:  rv.process (l, r, wl, wr);              mix (rvMix.next(), l, r, wl, wr); break;
            case stComp:
            default:
            {
                // linear blend: parallel compression stays level-true
                const float m = cpMix.next();
                const float g = cp.computeGain (l, r, cpMakeup.next());
                if (m > 0.0f) { const float k = 1.0f + m * (g - 1.0f); l *= k; r *= k; }
                break;
            }
        }
    }

    void applyParams()
    {
        const auto& p = params;
        sgMix.set (p.sgMix); psMix.set (p.psMix); psFb.set (p.psFeedback);
        fmMix.set (p.fmMix); grMix.set (p.grMix); dlMix.set (p.dlMix); dlFb.set (p.dlFeedback);
        rvMix.set (p.rvMix); cpMix.set (p.cpMix); cpMakeup.set (p.cpMakeupDb);
        masterFb.set (p.masterFb);

        sg.setCutoffDb (p.sgCutoffDb);
        sg.setAttackMs (p.sgAttackMs);
        sg.setSmearSeconds (p.sgSmearS);
        sg.setKnee (p.sgKnee);
        sg.setBlur (p.sgBlur);
        sg.setScatter (p.sgScatter);
        sg.setTiltDb (p.sgTilt);
        sg.setFreeze (p.sgFreeze);

        const auto& scales = getScales();
        const auto& s = scales[(size_t) std::clamp (p.psScale, 0, (int) scales.size() - 1)];
        const auto q = quantise (s, (double) p.psShiftSemis * 100.0);
        quantisedCents = (float) q.cents;
        quantisedDegree = q.degreeIndex;
        quantisedPeriod = q.period;
        ps.setPitchOctaves ((float) (q.cents / 1200.0));
        ps.setGrainSeconds (p.psGrainMs * 0.001f);

        fm.set (p.fmVowel, p.fmShift, p.fmRes);

        gr.setGrainParams (p.grSizeMs, p.grDensity, p.grPosition, p.grSpray, p.grWidth, p.grReverse);
        gr.setFadeTime (p.grFade);
        gr.setOctave ((int) std::lround (p.grOctave), p.grRandOct);
        gr.setFreeze (p.grFreeze);

        // delay taps from the scale, stacked up from the degree the shifter landed on
        if (p.psScale != lastTapScale || q.degreeIndex != lastTapDegree)
        {
            lastTapScale = p.psScale; lastTapDegree = q.degreeIndex;
            const auto tr = delayTapRatios (s, q.degreeIndex);
            for (int k = 0; k < 6; ++k) tapRatio[k] = (float) std::min (4.0, tr.ratio[k]);
        }
        const float T = p.dlTimeMs * 0.001f;
        const float L[3] = { T * tapRatio[0], T * tapRatio[2], T * tapRatio[4] };
        const float R[3] = { T * tapRatio[1], T * tapRatio[3], T * tapRatio[5] };
        dl.setTimes (L, R);
        dl.setTone (p.dlTone);

        rv.setDecay (p.rvDecay);
        rv.setTone (p.rvTone);
        cp.set (p.cpThreshDb, p.cpRatio, p.cpBelow, p.cpAttackMs, p.cpReleaseMs);
    }

    static void mix (float m, float& l, float& r, float wl, float wr) noexcept
    {
        if (m <= 0.0f) return; // exact bypass
        const float a = m * (float) kPi * 0.5f;
        const float dry = std::cos (a), wet = std::sin (a);
        l = dry * l + wet * wl;
        r = dry * r + wet * wr;
    }

    void snapToParams()
    {
        const auto& p = params;
        sgMix.snap (p.sgMix); psMix.snap (p.psMix); psFb.snap (p.psFeedback);
        fmMix.snap (p.fmMix); grMix.snap (p.grMix); dlMix.snap (p.dlMix); dlFb.snap (p.dlFeedback);
        rvMix.snap (p.rvMix); cpMix.snap (p.cpMix); cpMakeup.snap (p.cpMakeupDb);
        masterFb.snap (p.masterFb);
    }

    double sr = 48000.0;
    EngineParams params;
    SpectralGate sg;
    PitchShifter ps;
    FormantFilter fm;
    GrainShifter gr;
    StereoDelay dl;
    FDNReverb rv;
    Compander cp;
    Ramp sgMix, psMix, psFb, fmMix, grMix, dlMix, dlFb, rvMix, cpMix, cpMakeup, masterFb;
    DCBlocker fbDC[2];
    Biquad fbHP[2];
    OnePoleLP fbLP[2];
    float fbState[2] {};
    float fbEnv = 0.0f, fbAtk = 0.01f, fbRel = 0.0001f, fbThresh = 0.3f;
    float quantisedCents = 0.0f;
    int quantisedDegree = 0, quantisedPeriod = 0;
    int lastTapScale = -1, lastTapDegree = -1;
    float tapRatio[6] { 1, 1.125f, 1.25f, 1.333f, 1.5f, 1.667f };
    StageOrder order = defaultOrder(), pendingOrder = defaultOrder();
    float dip = 1.0f, dipStep = 0.001f;
    bool prepared = false;
};
} // namespace fp
