#pragma once
#include "Utils.h"

namespace fp
{
//==============================================================================
// Vowel formant filter: five parallel bandpass resonators (TPT state-variable)
// tuned to sung-vowel formants. The vowel knob morphs continuously a > e > i > o > u
// (frequencies interpolated in log-frequency). Shift moves every formant up/down
// (bigger/smaller "throat"), resonance narrows or widens the bands.
// An RMS level matcher keeps the output at the input's loudness, so the stage
// can be swept without jumps in level.
class FormantFilter
{
public:
    static constexpr int F = 5, V = 5;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        vowel.setup (sr, 0.03); shift.setup (sr, 0.03); res.setup (sr, 0.03);
        vowel.snap (0.0f); shift.snap (0.0f); res.snap (1.0f);
        powCoef  = (float) (1.0 - std::exp (-1.0 / (0.25 * sr)));
        gainCoef = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
        reset();
    }

    void reset()
    {
        for (auto& c : bp) for (auto& f : c) f.reset();
        pIn = pOut = 0.0f; gain = 1.0f; counter = 0;
        updateCoefs (vowel.cur, shift.cur, res.cur);
    }

    void set (float vowelPos, float shiftSemis, float resonance) noexcept
    {
        vowel.set (std::clamp (vowelPos, 0.0f, (float) (V - 1)));
        shift.set (std::clamp (shiftSemis, -24.0f, 24.0f));
        res.set (std::clamp (resonance, 0.1f, 8.0f));
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float v = vowel.next(), s = shift.next(), q = res.next();
        if (++counter >= 16) { counter = 0; updateCoefs (v, s, q); }

        const float in[2] = { inL, inR };
        float out[2];
        for (int c = 0; c < 2; ++c)
        {
            float acc = 0.0f;
            for (int f = 0; f < F; ++f)
                acc += amp[f] * bp[c][f].process (in[c]);
            out[c] = acc;
        }

        // RMS level match (bounded, so silence or a filtered-away sine can't explode it)
        pIn  = flushDenormal (pIn  + powCoef * (in[0] * in[0] + in[1] * in[1] - pIn));
        pOut = flushDenormal (pOut + powCoef * (out[0] * out[0] + out[1] * out[1] - pOut));
        const float target = std::clamp (std::sqrt ((pIn + 1.0e-9f) / (pOut + 1.0e-9f)), 0.1f, 32.0f);
        gain += gainCoef * (target - gain);

        outL = out[0] * gain;
        outR = out[1] * gain;
    }

    // which vowel the knob is on, for the UI
    static const char* vowelName (int i) noexcept
    {
        static const char* n[V] = { "a", "e", "i", "o", "u" };
        return n[std::clamp (i, 0, V - 1)];
    }

private:
    void updateCoefs (float v, float s, float q) noexcept
    {
        // tenor formants (Hz), relative levels (dB) and bandwidths (Hz)
        static const float fr[V][F] = { { 650, 1080, 2650, 2900, 3250 },   // a
                                        { 400, 1700, 2600, 3200, 3580 },   // e
                                        { 290, 1870, 2800, 3250, 3540 },   // i
                                        { 400,  800, 2600, 2800, 3000 },   // o
                                        { 350,  600, 2700, 2900, 3300 } }; // u
        static const float db[V][F] = { { 0,  -6,  -7,  -8, -22 },
                                        { 0, -14, -12, -14, -20 },
                                        { 0, -15, -18, -20, -30 },
                                        { 0, -10, -12, -12, -26 },
                                        { 0, -20, -17, -14, -26 } };
        static const float bw[V][F] = { { 80, 90, 120, 130, 140 },
                                        { 70, 80, 100, 120, 120 },
                                        { 40, 90, 100, 120, 120 },
                                        { 40, 80, 100, 120, 120 },
                                        { 40, 60, 100, 120, 120 } };

        const int i0 = std::clamp ((int) v, 0, V - 1);
        const int i1 = std::min (i0 + 1, V - 1);
        const float t = std::clamp (v - (float) i0, 0.0f, 1.0f);
        const float mul = std::exp2 (s / 12.0f);

        for (int f = 0; f < F; ++f)
        {
            const float hz = std::exp (std::log (fr[i0][f]) + t * (std::log (fr[i1][f]) - std::log (fr[i0][f]))) * mul;
            const float b  = (bw[i0][f] + t * (bw[i1][f] - bw[i0][f])) * std::sqrt (mul) / q;
            const float a  = std::pow (10.0f, (db[i0][f] + t * (db[i1][f] - db[i0][f])) / 20.0f);
            amp[f] = a;
            for (int c = 0; c < 2; ++c)
                bp[c][f].set (sr, (double) hz, std::clamp ((double) hz / (double) b, 0.5, 120.0));
        }
    }

    double sr = 48000.0;
    SVFBandpass bp[2][F];
    float amp[F] {};
    Ramp vowel, shift, res;
    float pIn = 0.0f, pOut = 0.0f, gain = 1.0f, powCoef = 0.001f, gainCoef = 0.001f;
    int counter = 0;
};
} // namespace fp
