// Runs the real plugin processor + editor headlessly: odd block sizes, sample rates,
// state round trip, and renders a screenshot of the UI.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

using namespace juce;

// steps the editor's animation clock by hand (no message loop in this test)
struct CloudfinTestAccess
{
    static void farAt1x (CloudfinEditor& e, bool on) { e.composeFarAt1x = on; }
    static void run (CloudfinEditor& e, bool gravity, int frames)
    {
        if (gravity) *e.proc.apvts.getRawParameterValue (ids::gravity) = 1.0f;   // the editor follows the parameter
        for (int i = 0; i < frames; ++i)
        {
            e.lastTick -= 1.0 / 30.0; // pretend a frame passed
            e.timerCallback();
        }
    }
};

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    int fails = 0;

    // --fb: does the main feedback self-resonate? (fresh plugin, a short burst, then silence)
    if (argc > 1 && String (argv[1]) == "--fb")
    {
        for (float fb : { 0.6f, 0.8f, 0.9f, 1.0f })
        {
            CloudfinProcessor p;
            p.setPlayConfigDetails (2, 2, 48000, 512);
            p.prepareToPlay (48000, 512);
            *p.apvts.getRawParameterValue (ids::masterFb) = fb;
            AudioBuffer<float> b (2, 512); MidiBuffer mm; Random r (3);
            std::printf ("fb %.1f:", fb);
            for (int k = 0; k < (int) (10.0 * 48000 / 512); ++k)
            {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, (argc > 2 && k < 20) ? 0.2f * (r.nextFloat() * 2 - 1) : 0.0f);
                p.processBlock (b, mm);
                if (k % 94 == 93) std::printf (" %.4f", b.getRMSLevel (0, 0, 512));
            }
            std::printf ("\n");
        }
        return 0;
    }

    // --perf: just render frames at 2x for a while (for a profiler to watch)
    if (argc > 1 && String (argv[1]) == "--perf")
    {
        CloudfinProcessor p;
        p.prepareToPlay (48000, 512);
        std::unique_ptr<CloudfinEditor> ed (dynamic_cast<CloudfinEditor*> (p.createEditor()));
        ed->setSize (CloudfinEditor::designW, CloudfinEditor::designH);
        const bool grav = argc > 2;
        CloudfinTestAccess::run (*ed, grav, 30);
        for (int i = 0; i < 300; ++i) { CloudfinTestAccess::run (*ed, false, 1); ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f); }
        return 0;
    }

    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        CloudfinProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        *p.apvts.getRawParameterValue (ids::masterFb) = 0.6f;
        Random rnd (42);
        AudioBuffer<float> buf (2, 4096);
        MidiBuffer midi;
        double peak = 0; bool finite = true;
        const int sizes[] = { 1, 7, 64, 333, 512, 1024, 4096, 13 };
        for (int it = 0; it < 600; ++it)
        {
            const int n = sizes[it % 8];
            buf.setSize (2, n, false, false, true);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) buf.setSample (c, i, rnd.nextFloat() * 0.4f - 0.2f);
            if (it % 50 == 0) *p.apvts.getRawParameterValue (ids::grFreeze) = (it / 50) % 2 ? 1.0f : 0.0f;
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i)
            { const float v = buf.getSample (c, i); if (! std::isfinite (v)) finite = false; peak = jmax (peak, (double) std::abs (v)); }
        }
        std::printf ("sr %6.0f: finite=%d peak=%.2f\n", sr, finite, peak);
        if (! finite || peak > 4.0) ++fails;
    }

    // mono in / stereo out
    {
        CloudfinProcessor p;
        AudioProcessor::BusesLayout l; l.inputBuses.add (AudioChannelSet::mono()); l.outputBuses.add (AudioChannelSet::stereo());
        const bool ok = p.setBusesLayout (l);
        p.prepareToPlay (48000, 256);
        AudioBuffer<float> buf (2, 256); buf.clear(); MidiBuffer m;
        for (int i = 0; i < 100; ++i) { buf.setSample (0, 0, 0.5f); p.processBlock (buf, m); }
        std::printf ("mono->stereo layout supported: %d\n", ok);
        if (! ok) ++fails;
    }

    // state round trip (params, module order, theme)
    {
        CloudfinProcessor a, b;
        a.apvts.getParameter (ids::psScale)->setValueNotifyingHost (0.5f);
        a.apvts.getParameter (ids::rvDecay)->setValueNotifyingHost (0.8f);
        a.setOrder ({ fp::stReverb, fp::stGrains, fp::stComp, fp::stGate, fp::stDelay, fp::stPitch, fp::stFormant });
        a.themeMode = 2;
        MemoryBlock mb; a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool same = std::abs (a.apvts.getParameter (ids::psScale)->getValue() - b.apvts.getParameter (ids::psScale)->getValue()) < 1e-6
                       && std::abs (a.apvts.getParameter (ids::rvDecay)->getValue() - b.apvts.getParameter (ids::rvDecay)->getValue()) < 1e-6
                       && a.getOrder() == b.getOrder() && b.themeMode.load() == 2;
        std::printf ("state round trip (incl. order + theme): %d\n", same);
        if (! same) ++fails;
    }

    // the main feedback sings on its own: from total silence, turned up, it starts ringing; turned down, true silence
    {
        auto runSilent = [] (float fb, double secs)
        {
            CloudfinProcessor p;
            p.setPlayConfigDetails (2, 2, 48000, 512);
            p.prepareToPlay (48000, 512);
            *p.apvts.getRawParameterValue (ids::masterFb) = fb;
            AudioBuffer<float> b (2, 512); MidiBuffer mm;
            float pk = 0.0f;
            for (int k = 0; k < (int) (secs * 48000 / 512); ++k) { b.clear(); p.processBlock (b, mm); pk = b.getMagnitude (0, 512); }
            return pk;   // the last block's peak
        };
        const float off = runSilent (0.0f, 3.0), ringing = runSilent (0.9f, 4.0);
        std::printf ("main feedback from silence: off -> peak %.6f, at 0.9 after 4 s -> peak %.3f\n", off, ringing);
        if (off != 0.0f || ringing < 0.05f) ++fails;
    }

    // the main output never clips: hot noise into full feedback, and twenty random patches, all stay under the ceiling
    {
        CloudfinProcessor p;
        p.setPlayConfigDetails (2, 2, 48000, 512);
        p.prepareToPlay (48000, 512);
        AudioBuffer<float> b (2, 512); MidiBuffer mm; Random r (5);
        float worst = 0.0f;
        auto play = [&] (double secs, float level)
        {
            for (int k = 0; k < (int) (secs * 48000 / 512); ++k)
            {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, level * (r.nextFloat() * 2 - 1));
                p.processBlock (b, mm);
                worst = jmax (worst, b.getMagnitude (0, 0, 512), b.getMagnitude (1, 0, 512));
            }
        };
        *p.apvts.getRawParameterValue (ids::masterFb) = 1.0f;
        *p.apvts.getRawParameterValue (ids::cpMakeup) = 18.0f;
        play (4.0, 4.0f);
        const float hot = worst;
        bool changed = false;
        const float before = *p.apvts.getRawParameterValue (ids::psShift);
        for (int k = 0; k < 20; ++k) { p.randomize(); play (0.6, 0.5f); changed |= *p.apvts.getRawParameterValue (ids::psShift) != before; }
        std::printf ("limiter: hot noise + full feedback + 18 dB makeup peak %.4f; 20 random patches worst peak %.4f (ceiling %.4f); randomize changes things %d\n",
                     hot, worst, fp::PeakLimiter::ceiling, changed);
        if (worst > fp::PeakLimiter::ceiling || ! changed) ++fails;
    }

    // slosh, the hydrophone, presets
    {
        auto makeP = [] { auto p = std::make_unique<CloudfinProcessor>(); p->setPlayConfigDetails (2, 2, 48000, 512); p->prepareToPlay (48000, 512); return p; };
        AudioBuffer<float> b (2, 512); MidiBuffer mm;
        auto runFor = [&] (CloudfinProcessor& p, double secs, float inLevel, double* rmsOut = nullptr, float* peakOut = nullptr, uint64_t seed = 9)
        {
            Random r ((int64) seed);
            double ss = 0; float pk = 0; long cnt = 0;
            for (int k = 0; k < (int) (secs * 48000 / 512); ++k)
            {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, inLevel * (r.nextFloat() * 2.0f - 1.0f));
                p.processBlock (b, mm);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float v = b.getSample (c, i); ss += v * v; pk = jmax (pk, std::abs (v)); ++cnt; }
            }
            if (rmsOut) *rmsOut = std::sqrt (ss / jmax (1L, cnt));
            if (peakOut) *peakOut = pk;
        };

        // slosh: fades in with gravity, washes even a silent input, and leaves without a trace
        {
            auto p = makeP();
            double quiet, washing, after;
            runFor (*p, 1.0, 0.0f, &quiet);
            *p->apvts.getRawParameterValue (ids::gravity) = 1.0f;
            p->sloshEnergy = 0.5f;
            runFor (*p, 0.1, 0.0f);
            const float early = p->sloshAmount.load();
            runFor (*p, 2.5, 0.0f, &washing);
            const float full = p->sloshAmount.load();
            *p->apvts.getRawParameterValue (ids::gravity) = 0.0f;
            runFor (*p, 4.0, 0.0f);
            runFor (*p, 1.0, 0.0f, &after);
            std::printf ("slosh: amount %.2f after 0.1 s, %.2f after 2.6 s, %.2f after gravity off; silent-input rms %.5f -> %.4f -> %.5f\n",
                         early, full, p->sloshAmount.load(), quiet, washing, after);
            if (early > 0.4f || full < 0.95f || p->sloshAmount.load() > 0.0f || washing < 0.003 || after > quiet + 1.0e-4) ++fails;

            // and it moves the filters: the same noise comes out differently each half second
            auto q = makeP();
            *q->apvts.getRawParameterValue (ids::gravity) = 1.0f;
            runFor (*q, 2.0, 0.2f);
            double r1, r2, r3;
            runFor (*q, 0.5, 0.2f, &r1, nullptr, 1); runFor (*q, 0.5, 0.2f, &r2, nullptr, 1); runFor (*q, 0.5, 0.2f, &r3, nullptr, 1);
            std::printf ("slosh on noise: three half-seconds of the same noise come out at rms %.4f %.4f %.4f\n", r1, r2, r3);
            if (std::abs (r1 - r2) + std::abs (r2 - r3) < 0.002) ++fails;
        }

        // the hydrophone: decoded in the background, heard on the output or through the effects
        {
            auto p = makeP();
            const double t0 = Time::getMillisecondCounterHiRes();
            while (! p->hydrophoneReady() && Time::getMillisecondCounterHiRes() - t0 < 15000.0) Thread::sleep (20);
            const double loadMs = Time::getMillisecondCounterHiRes() - t0;
            double off, onOut, onIn;
            runFor (*p, 0.5, 0.0f, &off);
            *p->apvts.getRawParameterValue (ids::hydroLevel) = 0.6f;
            runFor (*p, 0.3, 0.0f); runFor (*p, 1.0, 0.0f, &onOut);
            *p->apvts.getRawParameterValue (ids::hydroInto) = 1.0f;
            runFor (*p, 0.5, 0.0f); runFor (*p, 1.0, 0.0f, &onIn);
            std::printf ("hydrophone: ready %d (%.0f ms), silent input rms %.5f; on the output %.4f; into the input %.4f\n",
                         p->hydrophoneReady(), loadMs, off, onOut, onIn);
            if (! p->hydrophoneReady() || onOut < 0.01 || onIn < 0.005 || off > 0.001) ++fails;
        }

        // presets: all load and stay sane; the feedback ones keep going after the input stops
        {
            auto p = makeP();
            for (int i = 0; i < p->getNumPrograms(); ++i)
            {
                p->setCurrentProgram (i);
                p->prepareToPlay (48000, 512);
                float pk1, pk2; double tail;
                runFor (*p, 1.0, 0.25f, nullptr, &pk1);
                runFor (*p, 3.0, 0.0f, nullptr, &pk2);
                runFor (*p, 1.0, 0.0f, &tail);
                const bool fin = std::isfinite (pk1) && std::isfinite (pk2) && std::isfinite (tail);
                bool ok = fin && jmax (pk1, pk2) <= fp::PeakLimiter::ceiling;
                if (p->getProgramGroup (i) == "feedback") ok = ok && tail > 0.003;
                std::printf ("preset %-18s %-8s peak %.2f, rms 3-4 s after the input stops %.4f%s\n", p->getProgramName (i).toRawUTF8(),
                             p->getProgramGroup (i).toRawUTF8(), jmax (pk1, pk2), tail, ok ? "" : "  <-- FAIL");
                if (! ok) ++fails;
            }
        }
    }

    // UI screenshots: dusk (default), day, and with gravity on after ~2.5 s of falling
    {
        const String base (argc > 1 ? argv[1] : "cloudfin_ui");
        auto shoot = [&] (int theme, bool gravity, const String& suffix)
        {
            CloudfinProcessor p;
            p.themeMode = theme;
            p.prepareToPlay (48000, 512);
            std::unique_ptr<CloudfinEditor> ed (dynamic_cast<CloudfinEditor*> (p.createEditor()));
            ed->setSize (CloudfinEditor::designW, CloudfinEditor::designH);
            CloudfinTestAccess::run (*ed, gravity, 75);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            File out (base + suffix + ".png");
            out.deleteFile();
            FileOutputStream os (out);
            PNGImageFormat().writeImageToStream (img, os);
            std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
        };
        shoot (1, false, "_dusk");
        shoot (0, false, "_day");
        shoot (2, false, "_night");
        shoot (2, true, "_gravity");
    }

    // frame cost: render the whole window as a timer tick would, at 1x and 2x (retina)
    {
        CloudfinProcessor p;
        p.prepareToPlay (48000, 512);
        std::unique_ptr<CloudfinEditor> ed (dynamic_cast<CloudfinEditor*> (p.createEditor()));
        ed->setSize (CloudfinEditor::designW, CloudfinEditor::designH);
        for (bool farAt1x : { true, false })
        for (bool grav : { false, true })
            for (float scale : { 1.0f, 2.0f })
            {
                CloudfinTestAccess::farAt1x (*ed, farAt1x);
                *p.apvts.getRawParameterValue (ids::gravity) = grav ? 1.0f : 0.0f;
                CloudfinTestAccess::run (*ed, false, grav ? 60 : 90);   // let it fall (or float home)
                ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);   // warm the caches
                // best of five batches (the machine's other work makes single runs noisy)
                double ms = 1.0e9;
                for (int batch = 0; batch < 5; ++batch)
                {
                    const double t0 = Time::getMillisecondCounterHiRes();
                    const int N = 12;
                    for (int i = 0; i < N; ++i)
                    {
                        CloudfinTestAccess::run (*ed, false, 1);
                        ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
                    }
                    ms = jmin (ms, (Time::getMillisecondCounterHiRes() - t0) / N);
                }
                std::printf ("frame cost %s at %.0fx%s: %.1f ms (%.0f fps possible)\n", grav ? "gravity" : "normal ", scale, farAt1x ? "" : " (far drawn direct)", ms, 1000.0 / ms);
            }
    }

    std::printf (fails ? "HOST TEST PROBLEMS\n" : "HOST TEST OK\n");
    return fails;
}
