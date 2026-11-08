// Offline DSP checks for Cloudfin (not shipped).
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/dsp/Engine.h"
#include <chrono>
#include <cstdio>

using namespace fp;

static constexpr double SR = 48000.0;
static constexpr int BLOCK = 128;

struct Stats { double rmsIn = 0, rmsOut = 0, peak = 0, maxD2 = 0; bool finite = true; };

static EngineParams allDry()
{
    EngineParams p;
    p.sgMix = p.psMix = p.fmMix = p.grMix = p.dlMix = p.rvMix = p.cpMix = 0.0f; p.masterFb = 0.0f; p.psFeedback = 0.0f;
    return p;
}

template <typename Gen, typename Mod>
static Stats run (EngineParams p, double seconds, Gen gen, Mod mod, std::vector<float>* capL = nullptr, double skip = 0.5)
{
    Engine e;
    e.setParams (p);
    e.prepare (SR);
    const int total = (int) (seconds * SR);
    std::vector<float> iL (BLOCK), iR (BLOCK), oL (BLOCK), oR (BLOCK);
    Stats s; double sIn = 0, sOut = 0; long cnt = 0;
    float p1 = 0, p2 = 0;
    for (int pos = 0; pos < total; pos += BLOCK)
    {
        mod (p, pos);
        e.setParams (p);
        for (int i = 0; i < BLOCK; ++i) { auto v = gen (pos + i); iL[(size_t) i] = v.first; iR[(size_t) i] = v.second; }
        e.process (iL.data(), iR.data(), oL.data(), oR.data(), BLOCK);
        for (int i = 0; i < BLOCK; ++i)
        {
            const float o = oL[(size_t) i];
            if (! std::isfinite (o) || ! std::isfinite (oR[(size_t) i])) s.finite = false;
            if (capL) capL->push_back (o);
            if (pos + i > skip * SR)
            {
                sIn += iL[(size_t) i] * iL[(size_t) i]; sOut += o * o; ++cnt;
                s.peak = std::max (s.peak, (double) std::abs (o));
                s.maxD2 = std::max (s.maxD2, (double) std::abs (o - 2 * p1 + p2));
            }
            p2 = p1; p1 = o;
        }
    }
    s.rmsIn = std::sqrt (sIn / std::max (1L, cnt)); s.rmsOut = std::sqrt (sOut / std::max (1L, cnt));
    return s;
}

static double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }

int main()
{
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what); if (! ok) ++failures; };

    Rng noiseRng; noiseRng.seed (1234);
    auto noise = [&] (int) { const float v = 0.25f * noiseRng.bi(); return std::make_pair (v, 0.25f * noiseRng.bi()); };
    auto sine  = [] (int n) { const float v = 0.5f * (float) std::sin (kTwoPi * 220.0 * n / SR); return std::make_pair (v, v); };
    auto nomod = [] (EngineParams&, int) {};

    // 1. transparency
    {
        std::puts ("1. transparency (all mixes 0, feedback 0)");
        Engine e; auto p = allDry(); e.setParams (p); e.prepare (SR);
        std::vector<float> iL (BLOCK), iR (BLOCK), oL (BLOCK), oR (BLOCK);
        double maxErr = 0;
        for (int pos = 0; pos < (int) (5 * SR); pos += BLOCK)
        {
            for (int i = 0; i < BLOCK; ++i) { auto v = noise (0); iL[(size_t) i] = v.first; iR[(size_t) i] = v.second; }
            e.process (iL.data(), iR.data(), oL.data(), oR.data(), BLOCK);
            for (int i = 0; i < BLOCK; ++i)
                maxErr = std::max ({ maxErr, (double) std::abs (oL[(size_t) i] - iL[(size_t) i]), (double) std::abs (oR[(size_t) i] - iR[(size_t) i]) });
        }
        std::printf ("     max error = %g\n", maxErr);
        check (maxErr == 0.0, "bit-exact passthrough");
    }

    // 2. wet levels for each effect (100% wet, noise in)
    std::puts ("2. wet levels (noise in, 100% wet), want within about +-6 dB");
    {
        struct Case { const char* name; std::function<void (EngineParams&)> set; };
        std::vector<Case> cases {
            { "spectral gate", [] (EngineParams& p) { p.sgMix = 1; p.sgCutoffDb = -60; } },
            { "pitch +7 (fb 0)", [] (EngineParams& p) { p.psMix = 1; p.psShiftSemis = 7; } },
            { "pitch +7 (fb .9)", [] (EngineParams& p) { p.psMix = 1; p.psShiftSemis = 7; p.psFeedback = 0.9f; } },
            { "grains", [] (EngineParams& p) { p.grMix = 1; } },
            { "grains dense bell", [] (EngineParams& p) { p.grMix = 1; p.grDensity = 20; p.grSizeMs = 400; } },
            { "grains sparse", [] (EngineParams& p) { p.grMix = 1; p.grDensity = 1.5f; p.grSizeMs = 40; } },
            { "pitch grain 8 ms", [] (EngineParams& p) { p.psMix = 1; p.psGrainMs = 8; } },
            { "pitch grain 300 ms", [] (EngineParams& p) { p.psMix = 1; p.psGrainMs = 300; } },
            { "formant i, res x4", [] (EngineParams& p) { p.fmMix = 1; p.fmVowel = 2; p.fmRes = 4; } },
            { "formant o, -12 st", [] (EngineParams& p) { p.fmMix = 1; p.fmVowel = 3; p.fmShift = -12; } },
            { "delay fb .9", [] (EngineParams& p) { p.dlMix = 1; p.dlFeedback = 0.9f; } },
            { "smear blur+scatter", [] (EngineParams& p) { p.sgMix = 1; p.sgCutoffDb = -60; p.sgBlur = 1; p.sgScatter = 1; } },
            { "grains all reverse", [] (EngineParams& p) { p.grMix = 1; p.grReverse = 1; } },
            { "reverb tone lp", [] (EngineParams& p) { p.rvMix = 1; p.rvTone = -0.35f; } },
            { "reverb tone hp", [] (EngineParams& p) { p.rvMix = 1; p.rvTone = 0.5f; } },
            { "spectral freeze", [] (EngineParams& p) { p.sgMix = 1; p.sgCutoffDb = -60; p.sgFreeze = true; } },
            { "compander lift", [] (EngineParams& p) { p.cpMix = 1; p.cpThreshDb = -30; p.cpBelow = -1; } },
            { "reverb 4s", [] (EngineParams& p) { p.rvMix = 1; p.rvDecay = 4; } },
            { "reverb 20s", [] (EngineParams& p) { p.rvMix = 1; p.rvDecay = 20; } },
        };
        for (auto& c : cases)
        {
            auto p = allDry(); c.set (p);
            noiseRng.seed (77);
            auto s = run (p, 12.0, noise, nomod, nullptr, 5.0);
            std::printf ("     %-18s in %6.1f dB  out %6.1f dB  (diff %+5.1f)  peak %.2f\n", c.name, db (s.rmsIn), db (s.rmsOut), db (s.rmsOut) - db (s.rmsIn), s.peak);
            check (s.finite && std::abs (db (s.rmsOut) - db (s.rmsIn)) < 9.0, c.name);
        }
    }

    // 3. clicks: pure sine through each wet effect while parameters move/randomise
    std::puts ("3. click detection (220 Hz sine, wet only, params automated)");
    {
        struct Case { const char* name; std::function<void (EngineParams&)> set; std::function<void (EngineParams&, int)> mod; double limit; };
        const double tone = 0.5 * std::pow (kTwoPi * 220.0 / SR, 2.0);
        std::vector<Case> cases {
            { "grains live, random octave", [] (EngineParams& p) { p.grMix = 1; p.grRandOct = true; }, [] (EngineParams&, int) {}, 0.0 },
            { "grains freeze toggling", [] (EngineParams& p) { p.grMix = 1; p.grRandOct = true; p.grFade = 0.3f; },
              [] (EngineParams& p, int pos) { p.grFreeze = ((pos / (int) (1.7 * SR)) % 2) == 1; }, 0.0 },
            { "grains freeze fast fade", [] (EngineParams& p) { p.grMix = 1; p.grRandOct = false; p.grOctave = 1; p.grFade = 0.01f; },
              [] (EngineParams& p, int pos) { p.grFreeze = ((pos / (int) (0.9 * SR)) % 2) == 1; }, 0.0 },
            { "pitch knob sweeping", [] (EngineParams& p) { p.psMix = 1; p.psFeedback = 0.0f; },
              [] (EngineParams& p, int pos) { p.psShiftSemis = (float) (24.0 * std::sin (pos / SR * 0.7)); }, 0.0 },
            { "scale switching", [] (EngineParams& p) { p.psMix = 1; p.psShiftSemis = 5.3f; },
              [] (EngineParams& p, int pos) { p.psScale = (pos / (int) (0.5 * SR)) % (int) getScales().size(); }, 0.0 },
            { "mix knobs moving", [] (EngineParams& p) { p.rvDecay = 3; },
              [] (EngineParams& p, int pos) { const float t = (float) (pos / SR);
                  p.sgMix = 0.5f + 0.5f * std::sin (t * 1.3f); p.psMix = 0.5f + 0.5f * std::sin (t * 2.1f);
                  p.grMix = 0.5f + 0.5f * std::sin (t * 0.9f); p.rvMix = 0.5f + 0.5f * std::sin (t * 1.7f); }, 0.0 },
            { "pitch GRAIN sweeping", [] (EngineParams& p) { p.psMix = 1; p.psFeedback = 0.5f; },
              [] (EngineParams& p, int pos) { p.psGrainMs = (float) (205.0 + 195.0 * std::sin (pos / SR * 1.9)); }, 0.0 },
            { "pitch grain stepping", [] (EngineParams& p) { p.psMix = 1; },
              [] (EngineParams& p, int pos) { p.psGrainMs = ((pos / (int) (0.23 * SR)) % 2) ? 12.0f : 350.0f; }, 0.0 },
            { "delay time sweeping", [] (EngineParams& p) { p.dlMix = 1; p.dlFeedback = 0.7f; },
              [] (EngineParams& p, int pos) { p.dlTimeMs = (float) (1000.0 + 980.0 * std::sin (pos / SR * 1.3)); }, 0.0 },
            { "delay scale switching", [] (EngineParams& p) { p.dlMix = 1; p.dlFeedback = 0.6f; },
              [] (EngineParams& p, int pos) { p.psScale = (pos / (int) (0.4 * SR)) % (int) getScales().size(); }, 0.0 },
            { "grain size/dens/pos moving", [] (EngineParams& p) { p.grMix = 1; },
              [] (EngineParams& p, int pos) { const double t = pos / SR;
                  p.grSizeMs = (float) (500 + 480 * std::sin (t * 2.3)); p.grDensity = (float) (12 + 11 * std::sin (t * 1.1));
                  p.grPosition = (float) (0.5 + 0.5 * std::sin (t * 3.7)); p.grSpray = (float) (0.5 + 0.5 * std::sin (t * 0.7)); }, 0.0 },
            { "formant sweeping", [] (EngineParams& p) { p.fmMix = 1; p.fmRes = 2; },
              [] (EngineParams& p, int pos) { const double t = pos / SR;
                  p.fmVowel = (float) (2 + 2 * std::sin (t * 2.0)); p.fmShift = (float) (12 * std::sin (t * 0.8)); }, 0.0 },
            { "compander moving", [] (EngineParams& p) { p.cpMix = 1; p.cpAttackMs = 0.5f; p.cpReleaseMs = 20; },
              [] (EngineParams& p, int pos) { const double t = pos / SR;
                  p.cpThreshDb = (float) (-30 + 25 * std::sin (t * 1.5)); p.cpBelow = (float) std::sin (t * 0.6);
                  p.cpRatio = (float) (10 + 9 * std::sin (t * 2.2)); }, 0.0 },
            { "smear params moving", [] (EngineParams& p) { p.sgMix = 1; p.sgCutoffDb = -40; },
              [] (EngineParams& p, int pos) { const double t = pos / SR;
                  p.sgBlur = (float) (0.5 + 0.5 * std::sin (t * 1.2)); p.sgTilt = (float) (6 * std::sin (t * 0.9));
                  p.sgKnee = (float) (4.5 + 3.5 * std::sin (t * 2.0)); p.sgSmearS = (float) (5 + 4.9 * std::sin (t * 0.5)); }, 0.0 },
            { "reverse grains, random oct", [] (EngineParams& p) { p.grMix = 1; p.grReverse = 0.5f; p.grRandOct = true; },
              [] (EngineParams& p, int pos) { p.grFreeze = ((pos / (int) (1.3 * SR)) % 2) == 1; }, 0.0 },
            { "reverb tone sweeping", [] (EngineParams& p) { p.rvMix = 1; },
              [] (EngineParams& p, int pos) { p.rvTone = (float) std::sin (pos / SR * 0.9); }, 0.0 },
            { "spectral freeze toggling", [] (EngineParams& p) { p.sgMix = 1; p.sgCutoffDb = -50; },
              [] (EngineParams& p, int pos) { p.sgFreeze = ((pos / (int) (0.8 * SR)) % 2) == 1; }, 0.0 },
            { "reverb decay sweeping", [] (EngineParams& p) { p.rvMix = 1; },
              [] (EngineParams& p, int pos) { p.rvDecay = (float) (10.1 + 9.9 * std::sin (pos / SR * 0.5)); }, 0.0 },
        };
        for (auto& c : cases)
        {
            auto p = allDry(); c.set (p);
            auto s = run (p, 20.0, sine, c.mod, nullptr, 1.0);
            // with octaves up to +2, a clean tone can have up to 16x the base 2nd difference
            const double limit = tone * 16.0 * 3.0 + 0.004;
            std::printf ("     %-28s max|d2| = %.5f (limit %.5f)  rms %.1f dB  peak %.2f\n", c.name, s.maxD2, limit, db (s.rmsOut), s.peak);
            check (s.finite && s.maxD2 < limit, c.name);
        }
    }

    // 3a. module order: transparency survives any order, and shuffling mid-stream never clicks
    std::puts ("3a. module ordering");
    {
        Engine e; auto p = allDry(); e.setParams (p);
        StageOrder o { stComp, stReverb, stGrains, stGate, stDelay, stFormant, stPitch };
        e.setOrder (o); e.prepare (SR);
        std::vector<float> iL (BLOCK), iR (BLOCK), oL (BLOCK), oR (BLOCK);
        double maxErr = 0;
        for (int pos = 0; pos < (int) (3 * SR); pos += BLOCK)
        {
            for (int i = 0; i < BLOCK; ++i) { auto v = noise (0); iL[(size_t) i] = v.first; iR[(size_t) i] = v.second; }
            e.process (iL.data(), iR.data(), oL.data(), oR.data(), BLOCK);
            for (int i = 0; i < BLOCK; ++i)
                maxErr = std::max (maxErr, (double) std::abs (oL[(size_t) i] - iL[(size_t) i]));
        }
        check (maxErr == 0.0, "bit-exact passthrough in a shuffled order");

        // everything wet, a sine in, shuffle the order every 0.35 s
        // sine-preserving settings (no pitch shift, fixed octave) so any click would stand out
        EngineParams q; q.sgMix = q.psMix = q.fmMix = q.grMix = q.dlMix = q.rvMix = 0.6f; q.masterFb = 0.5f;
        q.psShiftSemis = 0.0f; q.grRandOct = false; q.grOctave = 0.0f;
        Engine f; f.setParams (q); f.prepare (SR);
        Rng orng; orng.seed (99);
        double maxD2 = 0, peak = 0; float p1 = 0, p2 = 0; bool finite = true; int swaps = 0;
        for (int pos = 0; pos < (int) (20 * SR); pos += BLOCK)
        {
            if (pos % (int) (0.35 * SR) < BLOCK)
            {
                StageOrder so = defaultOrder();
                for (int i = numStages - 1; i > 0; --i) std::swap (so[(size_t) i], so[(size_t) orng.integer (i + 1)]);
                f.setOrder (so); ++swaps;
            }
            for (int i = 0; i < BLOCK; ++i) { auto v = sine (pos + i); iL[(size_t) i] = v.first; iR[(size_t) i] = v.second; }
            f.process (iL.data(), iR.data(), oL.data(), oR.data(), BLOCK);
            for (int i = 0; i < BLOCK; ++i)
            {
                const float v = oL[(size_t) i];
                if (! std::isfinite (v)) finite = false;
                if (pos + i > SR) { maxD2 = std::max (maxD2, (double) std::abs (v - 2 * p1 + p2)); peak = std::max (peak, (double) std::abs (v)); }
                p2 = p1; p1 = v;
            }
        }
        const double tone = 0.5 * std::pow (kTwoPi * 220.0 / SR, 2.0);
        const double limit = tone * 16.0 * 3.0 + 0.004;
        std::printf ("     %d reorders: max|d2| = %.5f (limit %.5f)  peak %.2f\n", swaps, maxD2, limit, peak);
        check (finite && maxD2 < limit && peak < 4.0, "reordering is click-free and bounded");
    }

    // 3b. seamless grain size: with shift = 0 the output pitch must stay put while the grain sweeps
    std::puts ("3b. pitch grain sweep must not bend pitch (shift 0, 220 Hz in)");
    {
        auto p = allDry(); p.psMix = 1; p.psShiftSemis = 0;
        std::vector<float> cap;
        run (p, 20.0, sine, [] (EngineParams& q, int pos) { q.psGrainMs = (float) (205.0 + 195.0 * std::sin (pos / SR * 1.9)); }, &cap, 0.0);
        // average frequency over windows of 20 rising zero crossings
        std::vector<double> zc;
        for (size_t i = (size_t) SR; i + 1 < cap.size(); ++i)
            if (cap[i] <= 0.0f && cap[i + 1] > 0.0f) zc.push_back ((double) i + cap[i] / (cap[i] - cap[i + 1]));
        double worst = 0.0, sum = 0.0; int nw = 0;
        for (size_t i = 0; i + 20 < zc.size(); i += 20)
        {
            const double f = 20.0 * SR / (zc[i + 20] - zc[i]);
            worst = std::max (worst, std::abs (f / 220.0 - 1.0));
            sum += std::abs (f / 220.0 - 1.0); ++nw;
        }
        std::printf ("     mean deviation %.3f %%, worst window %.2f %%  (a gliding delay would bend ~18 %%)\n",
                     100.0 * sum / std::max (1, nw), 100.0 * worst);
        check (worst < 0.03, "grain size changes crossfade, no pitch bend");
    }

    // 4. feedback stability
    std::puts ("4. main feedback stability (60 s, feedback 100%)");
    {
        auto p = allDry(); p.masterFb = 1.0f;
        auto s = run (p, 60.0, noise, nomod);
        std::printf ("     all dry   : peak %.2f rms %.1f dB\n", s.peak, db (s.rmsOut));
        check (s.finite && s.peak < 4.0, "bounded with dry chain");

        EngineParams q; q.masterFb = 1.0f; q.psFeedback = 0.95f; q.sgMix = 0.8f; q.psMix = 0.8f; q.grMix = 0.8f; q.rvMix = 0.8f; q.rvDecay = 20;
        q.fmMix = 0.8f; q.fmRes = 4; q.dlMix = 0.8f; q.dlFeedback = 0.95f; q.cpMix = 0;
        auto s2 = run (q, 60.0, noise, nomod);
        std::printf ("     all wet   : peak %.2f rms %.1f dB\n", s2.peak, db (s2.rmsOut));
        check (s2.finite && s2.peak < 4.0, "bounded with everything maxed");

        q.cpMix = 1; q.cpThreshDb = -40; q.cpBelow = -1; q.cpRatio = 1; // compander lifting everything into the loop
        auto s3 = run (q, 60.0, noise, nomod);
        std::printf ("     + lift    : peak %.2f rms %.1f dB\n", s3.peak, db (s3.rmsOut));
        check (s3.finite && s3.peak < 4.0, "bounded with compander accentuating feedback");

        q.cpBelow = 1; q.cpRatio = 20; q.cpThreshDb = -30; // compander taming
        auto s4 = run (q, 60.0, noise, nomod);
        std::printf ("     + tame    : peak %.2f rms %.1f dB\n", s4.peak, db (s4.rmsOut));
        check (s4.finite && s4.rmsOut < s3.rmsOut, "compander taming reduces feedback level");
    }

    // 5. CPU
    std::puts ("5. CPU (defaults, everything on)");
    {
        EngineParams p; p.masterFb = 0.4f; p.sgMix = 0.5f; p.fmMix = 0.5f; p.dlMix = 0.5f; p.grDensity = 16; p.grSizeMs = 400;
        auto t0 = std::chrono::high_resolution_clock::now();
        auto s = run (p, 60.0, noise, nomod);
        auto t1 = std::chrono::high_resolution_clock::now();
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("     60 s of stereo audio in %.2f s  ->  %.1f%% of one core (realtime x%.0f)\n", secs, secs / 60.0 * 100.0, 60.0 / secs);
        check (s.finite && secs < 15.0, "fast enough for live use");
    }

    // 6. render a demo file for listening
    {
        std::vector<float> cap;
        EngineParams p; p.masterFb = 0.3f;
        auto pluck = [] (int n) {
            const double t = n / SR; const double ph = std::fmod (t, 1.5);
            const double f = 220.0 * std::pow (2.0, std::floor (t / 1.5) * 0.25 - std::floor (t / 6.0));
            const float v = (float) (0.4 * std::exp (-ph * 3.0) * (std::sin (kTwoPi * f * t) + 0.3 * std::sin (kTwoPi * 2 * f * t)));
            return std::make_pair (v, v); };
        run (p, 20.0, pluck, [] (EngineParams& q, int pos) { q.grFreeze = pos > 10 * SR && pos < 16 * SR; }, &cap, 0.0);
        juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile ("cloudfin_demo.wav");
        out.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (out.createOutputStream().release());
        if (auto* w = wav.createWriterFor (os.get(), SR, 1, 24, {}, 0))
        {
            os.release();
            std::unique_ptr<juce::AudioFormatWriter> writer (w);
            const float* ch[1] = { cap.data() };
            writer->writeFromFloatArrays (ch, 1, (int) cap.size());
            std::printf ("6. wrote %s\n", out.getFullPathName().toRawUTF8());
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL GOOD" : "PROBLEMS", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
