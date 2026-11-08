#pragma once
#include "Utils.h"

namespace fp
{
//==============================================================================
// Stereo-linked compander, the last stage before the main feedback tap.
//
//   above threshold : compression, ratio 1:1 .. 20:1             (tames swells)
//   below threshold : "below" knob is bipolar
//       > 0  downward expansion up to 1:8 (quiet tails sink, washes stop building)
//       < 0  upward compression up to 10:1 (quiet tails are lifted toward the
//            threshold, so feedback blooms and sustains)
//
// The upward boost is capped at +30 dB and can never push a signal above the
// threshold, so it accentuates without running away. Peak detector with
// separate attack/release, gain computed in dB.
class Compander
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        set (-18.0f, 2.0f, 0.0f, 10.0f, 250.0f);
        reset();
    }

    void reset() { env = 0.0f; gainDb = 0.0f; }

    void set (float thresholdDb, float ratioAbove, float below, float attackMs, float releaseMs) noexcept
    {
        thr   = thresholdDb;
        ratio = std::max (1.0f, ratioAbove);
        bel   = std::clamp (below, -1.0f, 1.0f);
        if (attackMs != lastAtk)  { lastAtk = attackMs;  atk = (float) (1.0 - std::exp (-1.0 / (std::max (0.05, (double) attackMs) * 0.001 * sr))); }
        if (releaseMs != lastRel) { lastRel = releaseMs; rel = (float) (1.0 - std::exp (-1.0 / (std::max (1.0, (double) releaseMs) * 0.001 * sr))); }
    }

    // returns the gain (linear) to apply; makeup in dB is added on top
    float computeGain (float l, float r, float makeupDb) noexcept
    {
        const float pk = std::max (std::abs (l), std::abs (r));
        env += (pk > env ? atk : rel) * (pk - env);
        env = flushDenormal (env);

        const float x = 20.0f * std::log10 (std::max (env, 1.0e-6f)); // floor -120 dB
        float g = 0.0f;
        if (x > thr)
        {
            g = (thr + (x - thr) / ratio) - x;
        }
        else if (bel > 0.0f)
        {
            const float re = 1.0f + 7.0f * bel;
            g = std::max (-80.0f, (x - thr) * (re - 1.0f));
        }
        else if (bel < 0.0f)
        {
            const float ru = 1.0f + 9.0f * -bel;
            // don't lift the noise floor of true silence
            const float fade = std::clamp ((x + 100.0f) / 20.0f, 0.0f, 1.0f);
            g = std::min (30.0f, (thr - x) * (1.0f - 1.0f / ru)) * fade;
        }
        gainDb = g;
        return std::pow (10.0f, (g + makeupDb) / 20.0f);
    }

    float getGainDb() const noexcept { return gainDb; }

private:
    double sr = 48000.0;
    float thr = -18.0f, ratio = 2.0f, bel = 0.0f;
    float atk = 0.01f, rel = 0.0001f, lastAtk = -1.0f, lastRel = -1.0f;
    float env = 0.0f, gainDb = 0.0f;
};
} // namespace fp
