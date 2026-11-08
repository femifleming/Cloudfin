#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace fp
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

//==============================================================================
// 4-point, 3rd-order Hermite interpolation (low aliasing, cheap)
inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

// smooth "S" curve 0..1 -> 0..1 with zero slope at the ends (click-free slews)
inline float smoothstep (float x) noexcept
{
    x = std::clamp (x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// gentle saturator used only inside feedback loops (keeps loops bounded)
inline float softClip (float x) noexcept
{
    return std::tanh (x);
}

inline float flushDenormal (float x) noexcept
{
    return (std::abs (x) < 1.0e-20f) ? 0.0f : x;
}

//==============================================================================
// small, fast, real-time safe random generator (xorshift64*)
struct Rng
{
    uint64_t s = 0x9E3779B97F4A7C15ull;

    void seed (uint64_t v) noexcept { s = v ? v : 0x9E3779B97F4A7C15ull; }

    uint64_t next() noexcept
    {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return s * 2685821657736338717ull;
    }

    float uni() noexcept { return (float) ((next() >> 40) * (1.0 / 16777216.0)); } // [0,1)
    float range (float lo, float hi) noexcept { return lo + (hi - lo) * uni(); }
    float bi() noexcept { return uni() * 2.0f - 1.0f; }
    int   integer (int n) noexcept { return (int) (uni() * (float) n) % n; }
};

//==============================================================================
// Transposed direct form II biquad (RBJ cookbook)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }

    void setLowpass (double sr, double f, double q) noexcept
    {
        const double w = kTwoPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }

    void setHighpass (double sr, double f, double q) noexcept
    {
        const double w = kTwoPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
};

// 8th-order Butterworth lowpass (4 cascaded biquads) - used as anti-alias filter
struct Butter8LP
{
    Biquad s[4];
    void setup (double sr, double f)
    {
        static const double qs[4] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        for (int i = 0; i < 4; ++i) s[i].setLowpass (sr, f, qs[i]);
    }
    void reset() { for (auto& b : s) b.reset(); }
    float process (float x) noexcept
    {
        for (auto& b : s) x = b.process (x);
        return x;
    }
};

// one-pole DC blocker
struct DCBlocker
{
    float r = 0.9995f, x1 = 0, y1 = 0;
    void setup (double sr, double hz = 10.0) { r = (float) std::exp (-kTwoPi * hz / sr); }
    void reset() { x1 = y1 = 0; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x; y1 = flushDenormal (y);
        return y1;
    }
};

// one-pole lowpass
struct OnePoleLP
{
    float a = 1.0f, z = 0.0f;
    void setup (double sr, double hz) { a = (float) (1.0 - std::exp (-kTwoPi * hz / sr)); }
    void reset() { z = 0; }
    float process (float x) noexcept { z += a * (x - z); z = flushDenormal (z); return z; }
};

//==============================================================================
// Linear parameter ramp (per-sample), used for every user-facing control so
// knob moves are always click-free.
struct Ramp
{
    float cur = 0, target = 0, step = 0;
    int   left = 0, len = 1;

    void setup (double sr, double seconds) { len = std::max (1, (int) (sr * seconds)); }
    void snap (float v) { cur = target = v; left = 0; step = 0; }
    void set (float v)
    {
        if (v == target) return;
        target = v; left = len; step = (target - cur) / (float) len;
    }
    float next() noexcept
    {
        if (left > 0) { cur += step; if (--left == 0) cur = target; }
        return cur;
    }
};

//==============================================================================
// A value in 0..1 that wanders to random targets at random times, gliding with a
// random slew time and an S-curve so the motion never has corners.
struct RandomWalk
{
    float value = 0.5f, from = 0.5f, to = 0.5f, phase = 1.0f, inc = 0.0f;
    int   hold = 0;
    double sr = 48000.0;
    float minHold = 0.15f, maxHold = 2.5f, minSlew = 0.03f, maxSlew = 2.0f;
    Rng*  rng = nullptr;

    void prepare (double sampleRate, Rng& r, float minH, float maxH, float minS, float maxS)
    {
        sr = sampleRate; rng = &r; minHold = minH; maxHold = maxH; minSlew = minS; maxSlew = maxS;
        value = from = to = rng->uni(); phase = 1.0f; hold = (int) (sr * rng->range (minHold, maxHold));
    }

    float next() noexcept
    {
        if (phase < 1.0f)
        {
            phase = std::min (1.0f, phase + inc);
            value = from + (to - from) * smoothstep (phase);
        }
        else if (--hold <= 0)
        {
            from  = value;
            to    = rng->uni();
            // random slew, skewed so short and long glides both happen
            const float u = rng->uni();
            const float slew = minSlew * std::pow (maxSlew / minSlew, u);
            inc   = 1.0f / (float) std::max (1.0, slew * sr);
            phase = 0.0f;
            hold  = (int) (sr * (slew + rng->range (minHold, maxHold)));
        }
        return value;
    }
};

//==============================================================================
// Fractional circular delay line (power-of-two size) with Hermite reads.
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int minSize)
    {
        int n = 1;
        while (n < minSize + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

    void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // delay in samples, measured from the most recently pushed sample (must be >= 1)
    float read (float delay) const noexcept
    {
        const float p  = (float) (w - 1) - delay;
        const float fl = std::floor (p);
        const int   i  = (int) fl;
        const float t  = p - fl;
        return hermite (buf[(size_t) ((i - 1) & mask)], buf[(size_t) (i & mask)],
                        buf[(size_t) ((i + 1) & mask)], buf[(size_t) ((i + 2) & mask)], t);
    }

    float readInt (int delay) const noexcept { return buf[(size_t) ((w - 1 - delay) & mask)]; }
};

//==============================================================================
// A read position on a DelayLine that never glides. When the wanted delay moves,
// a second read head opens at the new delay and the two are crossfaded
// (sin^2 / cos^2, gains sum to 1), so changing a delay time never bends the pitch.
// Any further change waits for the current fade to finish, then fades again.
struct SeamlessTap
{
    float cur = 1.0f, nxt = 1.0f, x = 1.0f, inc = 0.001f;
    bool fading = false;

    void setup (double sr, double fadeSeconds) { inc = 1.0f / (float) std::max (1.0, fadeSeconds * sr); }
    void snap (float d) { cur = nxt = d; fading = false; x = 1.0f; }

    float read (const DelayLine& line, float target) noexcept
    {
        if (! fading && std::abs (target - cur) > std::max (0.75f, cur * 0.002f))
        {
            nxt = target; fading = true; x = 0.0f;
        }
        if (! fading)
            return line.read (cur);

        x = std::min (1.0f, x + inc);
        const float s = std::sin (x * (float) kPi * 0.5f);
        const float gNew = s * s;
        const float y = (1.0f - gNew) * line.read (cur) + gNew * line.read (nxt);
        if (x >= 1.0f) { cur = nxt; fading = false; }
        return y;
    }
};

//==============================================================================
// Topology-preserving state variable filter (Zavalishin / Cytomic), bandpass
// output normalised to 0 dB at the centre frequency.
struct SVFBandpass
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0;

    void set (double sr, double f, double q) noexcept
    {
        const double g = std::tan (kPi * std::clamp (f, 10.0, sr * 0.45) / sr);
        k  = (float) (1.0 / std::max (0.05, q));
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) g * a1;
        a3 = (float) g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0; }
    float process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal (2.0f * v1 - ic1);
        ic2 = flushDenormal (2.0f * v2 - ic2);
        return k * v1;
    }
};

//==============================================================================
// Unipolar random "control voltage" in 0..1. At a jittered rate it picks a new
// random target and glides there with an S-curve; slew (0..1) is the fraction of
// each interval spent gliding (0 = stepped sample & hold, 1 = always moving).
// Advanced in chunks so it can run at control rate.
struct SlewedRandom
{
    float value = 0.5f, from = 0.5f, to = 0.5f, phase = 1.0f, inc = 0.0f;
    double hold = 0.0;
    Rng rng;

    void seed (uint64_t s) noexcept { rng.seed (s); value = from = to = rng.uni(); phase = 1.0f; hold = 0.0; }

    float advance (int samples, double sr, float rateHz, float slew) noexcept
    {
        if (phase < 1.0f)
        {
            phase = std::min (1.0f, phase + inc * (float) samples);
            value = from + (to - from) * smoothstep (phase);
        }
        hold -= samples;
        if (hold <= 0.0)
        {
            const double interval = (1.0 / std::max (0.001f, rateHz)) * (double) rng.range (0.55f, 1.45f);
            const double glide = std::max (0.004, interval * (double) std::clamp (slew, 0.0f, 1.0f));
            from  = value;
            to    = rng.uni();
            phase = 0.0f;
            inc   = (float) (1.0 / (glide * sr));
            hold  = interval * sr;
        }
        return value;
    }
};
} // namespace fp
