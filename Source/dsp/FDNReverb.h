#pragma once
#include "Utils.h"

namespace fp
{
//==============================================================================
// 8-line feedback delay network reverb.
// * 4th-order Butterworth high-pass on the input (500 Hz at centre tone: the "high pass" verb)
//   and a 4th-order low-pass. The bipolar tone knob is a DJ filter: left pulls the
//   low-pass down to ~300 Hz (and lets the high-pass down to 50 Hz so the low end
//   fills in), right pushes the high-pass up to 5 kHz.
// * stereo input diffusion (allpass chains)
// * Hadamard mixing matrix (lossless), per-line gain set from the T60 decay time
// * gentle HF damping and slow, decorrelated delay modulation to avoid metallic ringing
class FDNReverb
{
public:
    static constexpr int L = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double scale = sr / 1000.0;
        static const double ms[L] = { 43.1, 51.7, 59.3, 67.9, 76.3, 83.9, 94.7, 103.1 };
        for (int i = 0; i < L; ++i)
        {
            len[i] = (float) (ms[i] * scale);
            lines[i].prepare ((int) (len[i] + 64));
            damp[i].setup (sr, 7500.0);
            modRate[i] = (float) (kTwoPi * (0.07 + 0.053 * i) / sr);
            modPhase[i] = (float) (i * 0.77);
        }
        modDepth = (float) (0.00028 * sr); // ~0.28 ms

        static const double apMs[2][4] = { { 4.77, 3.59, 12.73, 9.31 }, { 5.21, 3.97, 11.83, 8.71 } };
        tone.setup (sr, 0.05);
        tone.snap (tone.target);
        lastTone = -9.0f; // force coefficients for this sample rate
        updateTone (tone.cur);
        for (int c = 0; c < 2; ++c)
        {
            for (int a = 0; a < 4; ++a)
            {
                apLen[c][a] = std::max (1, (int) (apMs[c][a] * scale));
                ap[c][a].prepare (apLen[c][a] + 2);
            }
        }
        decay.setup (sr, 0.15);
        decay.snap (3.0f);
        updateGains (3.0f);
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& d : damp) d.reset();
        for (int c = 0; c < 2; ++c) { for (auto& h : hp[c]) h.reset(); for (auto& h : lp[c]) h.reset(); for (auto& a : ap[c]) a.reset(); }
        counter = 0;
    }

    void setDecay (float seconds) noexcept { decay.set (seconds); }
    void setTone (float t) noexcept { tone.set (std::clamp (t, -1.0f, 1.0f)); }

    // cutoffs for a tone value (also used by the UI readout)
    static float highpassHz (float t) noexcept { return 500.0f * std::pow (10.0f, t); } // 50 Hz .. 500 Hz .. 5 kHz
    static float lowpassHz (float t) noexcept  { return t < 0.0f ? 20000.0f * std::pow (10.0f, 1.82f * t) : 20000.0f; }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float t60 = decay.next();
        const float tn = tone.next();
        if (++counter >= 16) { counter = 0; updateGains (t60); updateTone (tn); }

        // high-pass + diffusion
        float in[2] = { inL, inR };
        for (int c = 0; c < 2; ++c)
        {
            float s = lp[c][1].process (lp[c][0].process (hp[c][1].process (hp[c][0].process (in[c]))));
            for (int a = 0; a < 4; ++a)
            {
                const float dly = ap[c][a].readInt (apLen[c][a] - 1);
                const float v = s + 0.62f * dly;
                ap[c][a].push (v);
                s = dly - 0.62f * v;
            }
            in[c] = s;
        }

        // read delay lines (modulated)
        float y[L];
        for (int i = 0; i < L; ++i)
        {
            modPhase[i] += modRate[i];
            if (modPhase[i] > (float) kTwoPi) modPhase[i] -= (float) kTwoPi;
            const float d = len[i] + modDepth * std::sin (modPhase[i]);
            y[i] = damp[i].process (lines[i].read (d)) * gain[i];
        }

        // outputs (decorrelated sign patterns)
        outL = 0.55f * ( y[0] - y[1] + y[2] + y[3] - y[4] + y[5] - y[6] + y[7]);
        outR = 0.55f * ( y[0] + y[1] - y[2] + y[3] + y[4] - y[5] - y[6] - y[7]);

        // Hadamard 8x8 (normalised) via butterflies
        hadamard (y);

        for (int i = 0; i < L; ++i)
        {
            const float inj = (i & 1) ? in[1] : in[0];
            lines[i].push (flushDenormal (y[i] + inj * 0.5f));
        }
    }

private:
    static void hadamard (float* v) noexcept
    {
        for (int h = 1; h < L; h <<= 1)
            for (int i = 0; i < L; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b; v[j + h] = a - b;
                }
        const float n = 0.35355339f; // 1/sqrt(8)
        for (int i = 0; i < L; ++i) v[i] *= n;
    }

    void updateTone (float t) noexcept
    {
        if (t == lastTone) return;
        lastTone = t;
        const double h = highpassHz (t), l = std::min ((double) lowpassHz (t), sr * 0.45);
        for (int c = 0; c < 2; ++c)
        {
            hp[c][0].setHighpass (sr, h, 0.54119610);
            hp[c][1].setHighpass (sr, h, 1.30656296);
            lp[c][0].setLowpass (sr, l, 0.54119610);
            lp[c][1].setLowpass (sr, l, 1.30656296);
        }
    }

    void updateGains (float t60) noexcept
    {
        for (int i = 0; i < L; ++i)
            gain[i] = (float) std::pow (10.0, -3.0 * (double) len[i] / ((double) t60 * sr));
    }

    double sr = 48000.0;
    DelayLine lines[L];
    OnePoleLP damp[L];
    float len[L] {}, gain[L] {}, modRate[L] {}, modPhase[L] {};
    float modDepth = 10.0f;
    Biquad hp[2][2], lp[2][2];
    Ramp tone;
    float lastTone = -9.0f;
    DelayLine ap[2][4];
    int apLen[2][4] {};
    Ramp decay;
    int counter = 0;
};
} // namespace fp
