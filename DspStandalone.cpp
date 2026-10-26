// Teste DSP sem dependências (nem JUCE): g++ -std=c++17 -O2 -ISource/DSP Tests/DspStandalone.cpp
#include "Compressor.h"
#include "Enhancer.h"
#include "SpaceReverb.h"
#include "TempoDelay.h"
#include "Coral.h"
#include <vector>
#include <cstdio>
#include <cmath>
#include <chrono>
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FALHA: " #c " | "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
static bool fin(const std::vector<float>& v) { for (float x : v) if (!std::isfinite(x)) return false; return true; }
static float rmsOf(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return (float) std::sqrt(s / (b - a)); }
static float peakOf(const std::vector<float>& v) { float m = 0; for (float x : v) m = std::max(m, std::abs(x)); return m; }

int main()
{
    for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const int N = (int) (sr * 6);
        // ---- Compressor
        { sv::Compressor c; c.prepare(sr); c.setParams(-30, 8, 5, 100, 6, 0);
          std::vector<float> l(N), r(N); for (int i = 0; i < N; ++i) l[i] = r[i] = 0.8f * std::sin(6.2831853f * 440.f * i / (float) sr);
          auto in = l; c.process(l.data(), r.data(), N);
          CHECK(fin(l), "comp nan"); CHECK(c.getGainReductionDb() > 5.f, "GR=%f sr=%f", c.getGainReductionDb(), sr);
          CHECK(rmsOf(l, N/2, N) < rmsOf(in, N/2, N) * 0.6f, "nivel nao reduziu");
          // sinal abaixo do threshold: transparente
          sv::Compressor q; q.prepare(sr); q.setParams(-10, 4, 5, 100, 0, 0);
          std::vector<float> a(N), b(N); for (int i = 0; i < N; ++i) a[i] = b[i] = 0.05f * std::sin(0.05f * i);
          auto ref = a; q.process(a.data(), b.data(), N); float d = 0; for (int i = 0; i < N; ++i) d = std::max(d, std::abs(a[i] - ref[i]));
          CHECK(d < 1e-6f, "abaixo do threshold alterou %g", d); }
        // ---- Enhancer
        { sv::Enhancer e; e.prepare(sr);
          std::vector<float> l(N), r(N); for (int i = 0; i < N; ++i) l[i] = r[i] = 0.5f * std::sin(6.2831853f * 1000.f * i / (float) sr);
          auto ref = l; e.setParams(0, .5f, .5f, .5f, .5f, 1); e.process(l.data(), r.data(), N);
          float d = 0; for (int i = 0; i < N; ++i) d = std::max(d, std::abs(l[i] - ref[i])); CHECK(d == 0.f, "amount 0 alterou");
          l = ref; r = ref; e.setParams(1, 1, 1, 1, 1, 1); e.process(l.data(), r.data(), N);
          CHECK(fin(l) && peakOf(l) < 1.0f, "enhancer pico %f", peakOf(l)); }
        // ---- Reverb: 3 tipos
        { float tail[3];
          for (int t = 0; t < 3; ++t)
          { sv::SpaceReverb rv; rv.prepare(sr); rv.setParams(t, 2.0f, 20, .5f, .5f, 1);
            std::vector<float> in(N, 0.f), wl(N), wr(N); in[0] = 0.5f;
            for (int s = 0; s < N; s += 480) { int n = std::min(480, N - s); rv.process(in.data() + s, in.data() + s, wl.data() + s, wr.data() + s, n); }
            CHECK(fin(wl) && fin(wr), "reverb nan t=%d", t);
            tail[t] = rmsOf(wl, (size_t) (0.5 * sr), (size_t) sr);
            CHECK(tail[t] > 2e-5f, "sem cauda t=%d sr=%f", t, sr);
            CHECK(rmsOf(wl, (size_t) (5 * sr), N) < rmsOf(wl, (size_t) (0.1 * sr), (size_t) (0.5 * sr)) * 0.5f, "nao decai t=%d", t);
            CHECK(peakOf(wl) < 2.f, "pico reverb %f", peakOf(wl)); }
          CHECK(tail[1] > tail[0], "HALL (%g) deve ter cauda > ROOM (%g)", tail[1], tail[0]);
          std::printf("sr=%6.0f  reverb RMS 0.5-1s  ROOM=%.5f HALL=%.5f PLATE=%.5f\n", sr, tail[0], tail[1], tail[2]); }
        // ---- Reverb: mudar de tipo a meio nao gera NaN nem saltos grandes
        { sv::SpaceReverb rv; rv.prepare(sr); std::vector<float> in(N), wl(N), wr(N); for (int i = 0; i < N; ++i) in[i] = 0.3f * std::sin(0.03f * i);
          for (int s = 0, k = 0; s < N; s += 480, ++k) { int n = std::min(480, N - s); rv.setParams(k % 3, .3f + (k % 7), (float) (k % 100), .5f, .5f, 1);
            rv.process(in.data() + s, in.data() + s, wl.data() + s, wr.data() + s, n); }
          CHECK(fin(wl) && peakOf(wl) < 5.f, "troca de tipo instavel"); }
        // ---- Delay: tempo exacto e feedback limitado
        { for (double ms : { 125.0, 250.0, 500.0, 333.3 })
          { sv::TempoDelay d; d.prepare(sr); d.setParams((float) ms, 0, 20, 20000, false);
            std::vector<float> in((size_t) sr, 0.f), wl(in.size()), wr(in.size()); in[0] = 0.5f;
            d.process(in.data(), in.data(), wl.data(), wr.data(), (int) in.size());
            int pk = 0; float pv = 0; for (size_t i = 100; i < wl.size(); ++i) if (std::abs(wl[i]) > pv) { pv = std::abs(wl[i]); pk = (int) i; }
            CHECK(std::abs(pk * 1000.0 / sr - ms) < 1.5, "delay %g ms obtido %g", ms, pk * 1000.0 / sr); }
          sv::TempoDelay d; d.prepare(sr); d.setParams(80, 1.0f, 100, 8000, true);
          std::vector<float> in((size_t) (sr * 30), 0.f), wl(in.size()), wr(in.size()); in[0] = 0.5f;
          for (size_t s = 0; s < in.size(); s += 512) { int n = (int) std::min<size_t>(512, in.size() - s); d.process(in.data() + s, in.data() + s, wl.data() + s, wr.data() + s, n); }
          CHECK(fin(wl) && peakOf(wl) < 1.f, "feedback divergiu"); CHECK(rmsOf(wl, in.size() - (size_t) sr, in.size()) < 0.05f, "feedback nao decai"); }
        // ---- Coral
        { sv::Coral c; c.prepare(sr); std::vector<float> in((size_t) sr, 0.f), wl(in.size()), wr(in.size()); for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * std::sin(0.05f * i);
          c.process(in.data(), in.data(), wl.data(), wr.data(), (int) in.size());
          CHECK(fin(wl) && fin(wr) && peakOf(wl) < 1.f, "coral"); CHECK(rmsOf(wl, in.size()/2, in.size()) > 0.01f, "coral mudo");
          double diff = 0; for (size_t i = in.size()/2; i < in.size(); ++i) diff += std::abs(wl[i] - wr[i]); CHECK(diff > 1.0, "coral sem largura estereo"); }
    }
    // ---- CPU
    { const double sr = 48000; const int N = (int) sr * 10;
      sv::Compressor c; sv::Enhancer e; sv::SpaceReverb rv; sv::TempoDelay d; sv::Coral co;
      c.prepare(sr); e.prepare(sr); rv.prepare(sr); d.prepare(sr); co.prepare(sr);
      c.setParams(-20, 4, 8, 120, 6, 0); e.setParams(.5f, .5f, .5f, .5f, .5f, 1); rv.setParams(2, 2, 20, .5f, .5f, 1); d.setParams(250, .4f, 120, 6000, true);
      std::vector<float> l(N), r(N), a(N), b(N), x(N), y(N); for (int i = 0; i < N; ++i) l[i] = r[i] = 0.4f * std::sin(0.04f * i);
      auto t0 = std::chrono::steady_clock::now();
      for (int s = 0; s < N; s += 512) { int n = std::min(512, N - s); c.process(l.data() + s, r.data() + s, n); e.process(l.data() + s, r.data() + s, n);
        rv.process(l.data() + s, r.data() + s, a.data() + s, b.data() + s, n); d.process(l.data() + s, r.data() + s, x.data() + s, y.data() + s, n);
        co.process(l.data() + s, r.data() + s, x.data() + s, y.data() + s, n); }
      double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      std::printf("CPU: 10 s de audio em %.3f s => %.2f %% de 1 core (cadeia DSP completa, -O2)\n", sec, sec * 10.0); }
    std::printf(fails == 0 ? "DSP: TODOS OS TESTES PASSARAM\n" : "DSP: %d FALHAS\n", fails);
    return fails ? 1 : 0;
}
