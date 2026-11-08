#pragma once
#include "Utils.h"

namespace fp
{
//==============================================================================
// Two hard-panned multi-tap delays (left only / right only), three taps each.
// Both are fed the mono sum, so every echo lands fully on one side.
// Tap times are set from outside (the Engine derives them from just-intonation
// ratios of the pitch shifter's scale). Every tap is a SeamlessTap, so moving the
// time or switching the scale crossfades between echoes instead of warbling.
// Each side regenerates from its longest tap through a tone lowpass and a
// soft clipper.
class StereoDelay
{
public:
    static constexpr int T = 3;
    static constexpr double maxSeconds = 9.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        maxDelay = (float) (maxSeconds * sr);
        for (int c = 0; c < 2; ++c)
        {
            line[c].prepare ((int) maxDelay + 8);
            for (auto& t : tap[c]) { t.setup (sr, 0.08); t.snap (target[c][0]); }
            dc[c].setup (sr, 10.0);
        }
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            line[c].reset(); dc[c].reset(); toneLP[c].reset(); wetLP[c].reset();
            for (int k = 0; k < T; ++k) tap[c][k].snap (target[c][k]);
        }
    }

    // seconds for [side][tap]
    void setTimes (const float left[T], const float right[T]) noexcept
    {
        for (int k = 0; k < T; ++k)
        {
            target[0][k] = std::clamp (left[k]  * (float) sr, 2.0f, maxDelay);
            target[1][k] = std::clamp (right[k] * (float) sr, 2.0f, maxDelay);
        }
    }

    void setTone (float hz) noexcept
    {
        if (hz == lastTone) return;
        lastTone = hz;
        for (int c = 0; c < 2; ++c) { toneLP[c].setup (sr, hz); wetLP[c].setup (sr, std::min (20000.0f, hz * 1.5f)); }
    }

    void process (float inL, float inR, float feedback, float& outL, float& outR) noexcept
    {
        const float mono = 0.5f * (inL + inR);
        float out[2];
        for (int c = 0; c < 2; ++c)
        {
            float y[T];
            for (int k = 0; k < T; ++k) y[k] = tap[c][k].read (line[c], target[c][k]);
            out[c] = wetLP[c].process (0.62f * (y[0] + 0.8f * y[1] + 0.64f * y[2]));
            const float regen = softClip (toneLP[c].process (dc[c].process (y[T - 1])) * feedback);
            line[c].push (flushDenormal (mono + regen));
        }
        outL = out[0];
        outR = out[1];
    }

private:
    double sr = 48000.0;
    float maxDelay = 1000.0f, lastTone = -1.0f;
    DelayLine line[2];
    SeamlessTap tap[2][T];
    float target[2][T] { { 4800, 6000, 7200 }, { 5400, 6400, 8100 } };
    OnePoleLP toneLP[2], wetLP[2];
    DCBlocker dc[2];
};
} // namespace fp
