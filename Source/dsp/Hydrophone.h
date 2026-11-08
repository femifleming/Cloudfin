#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <thread>
#include "Utils.h"

namespace fp
{
//==============================================================================
// The hydrophone recording (built into the plugin, Ogg Vorbis), decoded once on a
// background thread and shared by every Cloudfin in the session. Until it's ready
// the player just stays quiet.
struct HydrophoneData
{
    juce::AudioBuffer<float> audio;
    double fileRate = 44100.0;
    std::atomic<bool> ready { false };

    HydrophoneData();
    ~HydrophoneData() { if (loader.joinable()) loader.join(); }

private:
    std::thread loader;
};

// Plays it as an endless texture: loops with a two-second equal-power crossfade so the seam
// never shows, resampled to the host rate, level eased so it never clicks.
class HydrophonePlayer
{
public:
    void prepare (double sampleRate) { sr = sampleRate; gain = 0.0f; }

    // adds the texture (times level) into L/R
    void addTo (float* L, float* R, int n, float level) noexcept
    {
        auto& d = *data;
        const float target = std::clamp (level, 0.0f, 1.0f);
        if (! d.ready.load (std::memory_order_acquire) || (target == 0.0f && gain < 1.0e-5f)) { gain = target == 0.0f ? 0.0f : gain; return; }
        const int len = d.audio.getNumSamples();
        const int xf = (int) std::min ((double) len / 4.0, 2.0 * d.fileRate);
        const double loopLen = (double) (len - xf);
        const double step = d.fileRate / sr;
        const float* a = d.audio.getReadPointer (0);
        const float* b = d.audio.getReadPointer (d.audio.getNumChannels() > 1 ? 1 : 0);
        const float gc = 1.0f - std::exp (-1.0f / (0.05f * (float) sr));
        auto read = [&] (const float* ch, double p)
        {
            const int i = (int) p; const float f = (float) (p - i);
            return ch[i] + f * (ch[std::min (i + 1, len - 1)] - ch[i]);
        };
        for (int s = 0; s < n; ++s)
        {
            gain += gc * (target - gain);
            float l = read (a, pos), r = read (b, pos);
            if (pos >= loopLen)   // the tail fades out as the head fades back in
            {
                const float u = (float) ((pos - loopLen) / (double) xf);
                const float go = std::cos (u * 1.5707963f), gi = std::sin (u * 1.5707963f);
                const double q = pos - loopLen;
                l = l * go + read (a, q) * gi;
                r = r * go + read (b, q) * gi;
            }
            L[s] += l * gain;
            R[s] += r * gain;
            pos += step;
            if (pos >= (double) (len - 1)) pos -= loopLen;
        }
    }

    bool isReady() const noexcept { return data->ready.load(); }

private:
    juce::SharedResourcePointer<HydrophoneData> data;
    double sr = 48000.0, pos = 0.0;
    float gain = 0.0f;
};

//==============================================================================
// Slosh: what you hear while everything's floating in the water. A lowpass and a highpass
// whose cutoffs wander on their own slewed random paths (each side its own, so it washes
// across the stereo field), and on top a layer of white noise through a wandering bandpass,
// swelling and ebbing like waves. 'energy' (0..1) is how hard things are splashing about.
// It fades in and out over about a second; fully out, the signal passes untouched.
class Slosh
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        amt = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            lp[c] = {}; hp[c] = {}; bp[c] = {};
            lpWalk[c].seed (101 + (uint64_t) c * 7); hpWalk[c].seed (203 + (uint64_t) c * 11);
            bpWalk[c].seed (307 + (uint64_t) c * 13); swell[c].seed (409 + (uint64_t) c * 17);
        }
        noise.seed (515);
        counter = 0;
    }

    void process (float* L, float* R, int n, bool on, float energy) noexcept
    {
        const float target = on ? 1.0f : 0.0f;
        if (target == 0.0f && amt < 1.0e-5f) { amt = 0.0f; return; }
        const float ac = 1.0f - std::exp (-1.0f / (0.35f * (float) sr));   // ~1 s to arrive or leave
        energy = std::clamp (energy, 0.0f, 1.0f);
        float* ch[2] = { L, R };
        for (int s = 0; s < n; ++s)
        {
            amt += ac * (target - amt);
            if (counter-- <= 0)
            {
                counter = 32;
                const float rate = 1.1f + 2.5f * energy;   // splashing about stirs the filters faster
                for (int c = 0; c < 2; ++c)
                {
                    lpHz[c] = 450.0f * std::pow (7000.0f / 450.0f, lpWalk[c].advance (32, sr, rate, 0.8f));
                    hpHz[c] = 30.0f * std::pow (650.0f / 30.0f, hpWalk[c].advance (32, sr, rate * 0.8f, 0.8f));
                    bpHz[c] = 220.0f * std::pow (2600.0f / 220.0f, bpWalk[c].advance (32, sr, rate * 1.3f, 0.7f));
                    const float sw = swell[c].advance (32, sr, 0.9f + 1.5f * energy, 0.9f);
                    wave[c] = sw * sw;
                    lpC[c] = design (lpHz[c], 0.9f);
                    hpC[c] = design (hpHz[c], 0.9f);
                    bpC[c] = design (bpHz[c], 0.55f);
                }
            }
            const float a = smoothstep (amt);
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][s];
                float lo, band, high;
                tick (lp[c], lpC[c], x, lo, band, high);
                float y = lo;
                tick (hp[c], hpC[c], y, lo, band, high);
                y = high;
                // the wash: white noise, a wandering band, swelling like waves
                float nl, nb, nh;
                tick (bp[c], bpC[c], noise.bi(), nl, nb, nh);
                y += nb * wave[c] * (0.05f + 0.07f * energy);
                ch[c][s] = x + a * (y - x);
            }
        }
    }

    float amount() const noexcept { return amt; }

private:
    struct Coefs { float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0; };
    struct State { float ic1 = 0, ic2 = 0; };

    Coefs design (float hz, float res) const noexcept
    {
        Coefs c;
        c.g = (float) std::tan (3.14159265 * std::clamp ((double) hz, 10.0, sr * 0.45) / sr);
        c.k = 2.0f - 2.0f * std::clamp (res, 0.0f, 0.98f) + 0.02f;   // res 0.9 = a little peak, sloshy
        c.a1 = 1.0f / (1.0f + c.g * (c.g + c.k)); c.a2 = c.g * c.a1; c.a3 = c.g * c.a2;
        return c;
    }
    static void tick (State& s, const Coefs& c, float x, float& lo, float& band, float& high) noexcept
    {
        const float v3 = x - s.ic2;
        const float v1 = c.a1 * s.ic1 + c.a2 * v3;
        const float v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
        s.ic1 = flushDenormal (2.0f * v1 - s.ic1);
        s.ic2 = flushDenormal (2.0f * v2 - s.ic2);
        lo = v2; band = v1; high = x - c.k * v1 - v2;
    }

    double sr = 48000.0;
    float amt = 0.0f;
    State lp[2], hp[2], bp[2];
    Coefs lpC[2], hpC[2], bpC[2];
    float lpHz[2] { 7000, 7000 }, hpHz[2] { 30, 30 }, bpHz[2] { 800, 800 }, wave[2] {};
    SlewedRandom lpWalk[2], hpWalk[2], bpWalk[2], swell[2];
    Rng noise;
    int counter = 0;
};
} // namespace fp
