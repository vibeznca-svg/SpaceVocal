#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <chrono>
#include <iostream>

using namespace juce;

namespace
{
    struct FakeHost : AudioPlayHead
    {
        double bpm = 120.0;
        Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm (bpm); return i; }
    };

    struct Rig
    {
        SpaceVocalProcessor p;
        FakeHost host;
        Rig (double sr = 48000.0, int block = 512, int channels = 2)
        {
            p.setPlayConfigDetails (channels, channels, sr, block);
            p.setPlayHead (&host);
            p.prepareToPlay (sr, block);
            p.presets.setUserDirectory (File::getSpecialLocation (File::tempDirectory).getChildFile ("sv_test_" + String (Random::getSystemRandom().nextInt())));
        }
        ~Rig() { p.presets.getUserDirectory().deleteRecursively(); }
        void set (const char* id, float real)
        {
            auto* prm = p.apvts.getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 (real));
        }
        float get (const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
        void allOff()
        {
            using namespace sv::id;
            for (auto* i : { compOn, magicOn, revOn, dlyOn, coralOn }) set (i, 0.f);
            set (inGain, 0.f); set (outGain, 0.f); set (bypass, 0.f);
        }
        // processa um sinal inteiro em blocos; devolve saída (canais x amostras)
        AudioBuffer<float> run (const AudioBuffer<float>& in, int block)
        {
            AudioBuffer<float> out (in.getNumChannels(), in.getNumSamples());
            MidiBuffer midi;
            for (int s = 0; s < in.getNumSamples(); s += block)
            {
                const int n = jmin (block, in.getNumSamples() - s);
                AudioBuffer<float> b (in.getNumChannels(), n);
                for (int c = 0; c < in.getNumChannels(); ++c) b.copyFrom (c, 0, in, c, s, n);
                p.processBlock (b, midi);
                for (int c = 0; c < in.getNumChannels(); ++c) out.copyFrom (c, s, b, c, 0, n);
            }
            return out;
        }
    };

    AudioBuffer<float> sine (int ch, int n, double sr, float amp, float f = 220.f)
    {
        AudioBuffer<float> b (ch, n);
        for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) b.setSample (c, i, amp * std::sin (MathConstants<float>::twoPi * f * (float) i / (float) sr));
        return b;
    }
    AudioBuffer<float> impulse (int ch, int n) { AudioBuffer<float> b (ch, n); b.clear(); for (int c = 0; c < ch; ++c) b.setSample (c, 0, 0.5f); return b; }
    bool finite (const AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
        return true;
    }
    float rms (const AudioBuffer<float>& b, int from, int to) { return b.getRMSLevel (0, from, to - from); }
}

struct SpaceVocalTests : UnitTest
{
    SpaceVocalTests() : UnitTest ("SPACE VOCAL", "SpaceVocal") {}

    void runTest() override
    {
        beginTest ("Parametros: defaults e limites");
        {
            Rig r;
            for (auto* prm : r.p.getParameters())
            {
                auto* rp = dynamic_cast<RangedAudioParameter*> (prm);
                expect (rp != nullptr);
                rp->setValueNotifyingHost (0.f);  expect (std::isfinite (rp->convertFrom0to1 (rp->getValue())));
                rp->setValueNotifyingHost (1.f);  expect (std::isfinite (rp->convertFrom0to1 (rp->getValue())));
            }
            Rig r2;
            expectWithinAbsoluteError (r2.get ("inGain"), 0.f, 0.001f);
            expectWithinAbsoluteError (r2.get ("outGain"), 0.f, 0.001f);
            r2.set ("inGain", 100.f);  expectWithinAbsoluteError (r2.get ("inGain"), 24.f, 0.001f);
            r2.set ("outGain", -100.f); expectWithinAbsoluteError (r2.get ("outGain"), -24.f, 0.001f);
        }

        beginTest ("Silencio sem NaN com tudo ligado");
        {
            Rig r; using namespace sv::id;
            for (auto* i : { compOn, magicOn, revOn, dlyOn, coralOn }) r.set (i, 1.f);
            r.set (revAmt, 1.f); r.set (dlyAmt, 1.f); r.set (coralAmt, 1.f);
            AudioBuffer<float> in (2, 48000); in.clear();
            auto out = r.run (in, 480);
            expect (finite (out));
            expect (out.getMagnitude (0, in.getNumSamples()) < 1.0e-4f);
        }

        beginTest ("Passagem transparente com efeitos desligados e 0 dB");
        {
            Rig r; r.allOff();
            auto in = sine (2, 48000, 48000, 0.5f);
            r.run (in, 512);   // estabiliza suavizadores
            auto out = r.run (in, 512);
            float maxDiff = 0; for (int i = 0; i < in.getNumSamples(); ++i) maxDiff = jmax (maxDiff, std::abs (in.getSample (0, i) - out.getSample (0, i)));
            expect (maxDiff < 1.0e-5f, "diferenca " + String (maxDiff));
        }

        beginTest ("Ganhos de entrada e saida");
        {
            Rig r; r.allOff(); r.set ("inGain", 6.f); r.set ("outGain", -6.f);
            auto in = sine (2, 24000, 48000, 0.25f);
            r.run (in, 256); auto out = r.run (in, 256);
            expectWithinAbsoluteError (out.getMagnitude (0, 24000), in.getMagnitude (0, 24000), 0.01f);
            r.set ("inGain", 12.f); r.set ("outGain", 0.f); r.run (in, 256); out = r.run (in, 256);
            expectWithinAbsoluteError (out.getMagnitude (0, 24000), in.getMagnitude (0, 24000) * 3.981f, 0.02f);
        }

        beginTest ("Bypass geral devolve o sinal original");
        {
            Rig r; using namespace sv::id;
            r.set (inGain, 12.f); r.set (compAmt, 1.f); r.set (bypass, 1.f);
            auto in = sine (2, 48000, 48000, 0.3f);
            r.run (in, 512); auto out = r.run (in, 512);
            float maxDiff = 0; for (int i = 0; i < in.getNumSamples(); ++i) maxDiff = jmax (maxDiff, std::abs (in.getSample (0, i) - out.getSample (0, i)));
            expect (maxDiff < 1.0e-5f, "diferenca " + String (maxDiff));
        }

        beginTest ("Compressor: reduz ganho e e estavel");
        {
            Rig r; r.allOff(); using namespace sv::id;
            r.set (compOn, 1.f); r.set (compAmt, 1.f); r.set (compThresh, -30.f); r.set (compRatio, 8.f); r.set (compMakeup, 0.f);
            auto in = sine (2, 48000, 48000, 0.8f, 440.f);
            auto out = r.run (in, 128);
            expect (finite (out));
            expect (r.p.meters.gr.load() > 3.0f, "GR " + String (r.p.meters.gr.load()));
            expect (rms (out, 24000, 48000) < rms (in, 24000, 48000) * 1.5f);  // sem explosao de nivel (inclui makeup automatico)
            // amount 0 => sem compressao
            r.set (compAmt, 0.f); r.run (in, 128); r.run (in, 128);
            expect (r.p.meters.gr.load() < 0.3f);
        }

        beginTest ("Magic: 0% nao altera; 100% e estavel e sem clipping em -6 dBFS");
        {
            Rig r; r.allOff(); using namespace sv::id;
            r.set (magicOn, 1.f); r.set (magicAmt, 0.f);
            auto in = sine (2, 48000, 48000, 0.5f, 1000.f);
            r.run (in, 512); auto out = r.run (in, 512);
            float d = 0; for (int i = 0; i < 48000; ++i) d = jmax (d, std::abs (in.getSample (0, i) - out.getSample (0, i)));
            expect (d < 1.0e-5f);
            r.set (magicAmt, 1.f); r.set (magicDrive, 1.f); r.set (magicHarm, 1.f); r.set (magicPres, 1.f); r.set (magicTone, 1.f);
            r.run (in, 512); out = r.run (in, 512);
            expect (finite (out)); expect (out.getMagnitude (0, 48000) < 1.0f, "pico " + String (out.getMagnitude (0, 48000)));
        }

        beginTest ("Reverb: cauda existe, decai e HALL > ROOM");
        {
            double tail[3] = {};
            for (int t = 0; t < 3; ++t)
            {
                Rig r; r.allOff(); using namespace sv::id;
                r.set (revOn, 1.f); r.set (revAmt, 1.f); r.set (revType, (float) t); r.set (revDecay, 2.0f);
                auto in = impulse (2, 48000 * 6);
                auto out = r.run (in, 480);
                expect (finite (out));
                tail[t] = rms (out, 48000, 96000);
                expect (tail[t] > 1.0e-5f, "sem cauda no tipo " + String (t));
                expect (rms (out, 48000 * 5, 48000 * 6) < rms (out, 4800, 24000), "nao decai");
            }
            expect (tail[1] > tail[0], "HALL deveria ter cauda mais longa que ROOM");
        }

        beginTest ("Delay: sincroniza com BPM da DAW");
        {
            struct Case { float sync; double bpm; double expectMs; } cases[] = { { 0, 120, 125 }, { 1, 120, 250 }, { 2, 120, 500 }, { 1, 90, 333.3 }, { 2, 150, 400 } };
            for (auto& c : cases)
            {
                Rig r; r.allOff(); using namespace sv::id;
                r.host.bpm = c.bpm;
                r.set (dlyOn, 1.f); r.set (dlyAmt, 1.f); r.set (dlySync, c.sync); r.set (dlyPing, 0.f); r.set (dlyFeedback, 0.f);
                r.set (dlyLow, 20.f); r.set (dlyHigh, 20000.f); r.set (dlyHostSync, 1.f);
                auto in = impulse (2, 48000);
                r.run (impulse (2, 4800), 480);          // deixa o tempo estabilizar
                auto out = r.run (in, 480);
                int peak = 0; float pv = 0;
                for (int i = 2400; i < 48000; ++i) if (std::abs (out.getSample (0, i)) > pv) { pv = std::abs (out.getSample (0, i)); peak = i; }
                const double ms = peak / 48.0;
                expect (std::abs (ms - c.expectMs) < 3.0, "esperado " + String (c.expectMs) + " ms, obtido " + String (ms));
            }
        }

        beginTest ("Delay: feedback maximo nao diverge");
        {
            Rig r; r.allOff(); using namespace sv::id;
            r.set (dlyOn, 1.f); r.set (dlyAmt, 1.f); r.set (dlyFeedback, 1.f); r.set (dlySync, 0.f);
            auto in = impulse (2, 48000 * 20);
            auto out = r.run (in, 512);
            expect (finite (out)); expect (out.getMagnitude (0, in.getNumSamples()) < 1.0f);
            expect (rms (out, 48000 * 19, 48000 * 20) < rms (out, 0, 48000 * 2));
        }

        beginTest ("Mono, taxas de amostragem e tamanhos de bloco");
        {
            for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
                for (int block : { 1, 32, 100, 512, 4096 })
                    for (int ch : { 1, 2 })
                    {
                        Rig r (sr, 512, ch); using namespace sv::id;
                        for (auto* i : { compOn, magicOn, revOn, dlyOn, coralOn }) r.set (i, 1.f);
                        r.set (revAmt, .5f); r.set (dlyAmt, .5f); r.set (coralAmt, 1.f); r.set (inGain, 12.f);
                        auto in = sine (ch, (int) (sr * 0.25), sr, 0.9f);
                        auto out = r.run (in, block);
                        expect (finite (out), "NaN/inf sr=" + String (sr) + " blk=" + String (block));
                        expect (out.getMagnitude (0, out.getNumSamples()) < 20.f);
                    }
        }

        beginTest ("Mudancas de parametros durante o processamento sem NaN");
        {
            Rig r; Random rnd (42);
            auto in = sine (2, 480, 48000, 0.5f);
            for (int k = 0; k < 400; ++k)
            {
                for (auto* prm : r.p.getParameters()) if (rnd.nextInt (8) == 0) prm->setValueNotifyingHost (rnd.nextFloat());
                auto out = r.run (in, 480);
                expect (finite (out));
            }
        }

        beginTest ("Presets de fabrica carregam valores reais");
        {
            Rig r; using namespace sv::id;
            auto names = r.p.presets.getNames();
            expect (r.p.presets.getNumFactory() >= 11);
            for (int i = 0; i < r.p.presets.getNumFactory(); ++i)
            {
                expect (r.p.presets.load (names[i]).wasOk(), names[i]);
                expectEquals (r.p.presets.getCurrent(), names[i]);
                auto out = r.run (sine (2, 4800, 48000, 0.3f), 480); expect (finite (out));
            }
            r.p.presets.load ("Rap / Trap"); expectWithinAbsoluteError (r.get (compRatio), 4.f, 0.01f); expect (r.get (revType) < 0.5f);
            r.p.presets.load ("Coral"); expect (r.get (coralOn) > 0.5f); expect (r.get (coralAmt) > 0.5f);
            r.p.presets.load ("Default"); expect (r.get (coralOn) < 0.5f);
            expect (r.p.presets.saveUser ("Default", true).failed());   // não substitui presets de fábrica
        }

        beginTest ("Presets de utilizador: guardar, carregar, substituir, eliminar, invalidos");
        {
            Rig r; using namespace sv::id;
            r.set (inGain, 7.f); r.set (revAmt, .77f);
            expect (r.p.presets.saveUser ("Meu Preset", false).wasOk());
            expect (r.p.presets.saveUser ("Meu Preset", false).failed());
            expect (r.p.presets.saveUser ("Meu Preset", true).wasOk());
            expect (r.p.presets.getNames().contains ("Meu Preset"));
            expect (! r.p.presets.isFactory ("Meu Preset"));
            r.p.presets.load ("Clean"); r.set (inGain, 0.f);
            expect (r.p.presets.load ("Meu Preset").wasOk());
            expectWithinAbsoluteError (r.get (inGain), 7.f, 0.01f); expectWithinAbsoluteError (r.get (revAmt), .77f, 0.01f);
            // ficheiros inválidos
            auto bad = r.p.presets.getUserDirectory().getChildFile ("lixo.svpreset"); bad.replaceWithText ("isto nao e xml");
            expect (r.p.presets.loadFromFile (bad).failed());
            bad.replaceWithText ("<OutraCoisa version=\"1\"/>"); expect (r.p.presets.loadFromFile (bad).failed());
            bad.replaceWithText ("<SpaceVocalPreset version=\"1\"><P id=\"inGain\" v=\"9999\"/><P id=\"naoExiste\" v=\"1\"/><P id=\"revAmt\" v=\"nan\"/></SpaceVocalPreset>");
            expect (r.p.presets.loadFromFile (bad).wasOk());
            expectWithinAbsoluteError (r.get (inGain), 24.f, 0.01f);   // fixado ao máximo
            expect (r.p.presets.deleteUser ("Meu Preset").wasOk());
            expect (! r.p.presets.getNames().contains ("Meu Preset"));
            expect (r.p.presets.deleteUser ("Default").failed());
        }

        beginTest ("Estado da instancia: guardar e restaurar");
        {
            Rig a; using namespace sv::id;
            a.p.presets.load ("Space"); a.set (outGain, -3.5f); a.set (dlyTimeMs, 777.f); a.set (revType, 0.f);
            MemoryBlock mb; a.p.getStateInformation (mb);
            Rig b; b.p.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (b.p.presets.getCurrent(), String ("Space"));
            for (auto* prm : a.p.getParameters())
            {
                auto* pa = dynamic_cast<RangedAudioParameter*> (prm);
                auto* pb = b.p.apvts.getParameter (pa->paramID);
                expectWithinAbsoluteError (pb->getValue(), pa->getValue(), 1.0e-4f, pa->paramID);
            }
        }

        beginTest ("Desempenho: CPU em tempo real");
        {
            Rig r (48000.0, 512); using namespace sv::id;
            for (auto* i : { compOn, magicOn, revOn, dlyOn, coralOn }) r.set (i, 1.f);
            r.set (coralAmt, 1.f); r.set (revAmt, .5f); r.set (dlyAmt, .5f);
            auto in = sine (2, 48000 * 10, 48000, 0.5f);
            const auto t0 = std::chrono::steady_clock::now();
            auto out = r.run (in, 512);
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            logMessage ("10 s de audio estereo 48 kHz processados em " + String (sec, 3) + " s  => " + String (sec / 10.0 * 100.0, 2) + " % de 1 core (build atual)");
            expect (finite (out));
            expect (sec < 10.0);
        }
    }
};

static SpaceVocalTests spaceVocalTests;

int main()
{
    ScopedJuceInitialiser_GUI init;
    UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();
    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i) failures += runner.getResult (i)->failures;
    std::cout << (failures == 0 ? "\nTODOS OS TESTES PASSARAM\n" : "\nFALHAS: " + String (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
