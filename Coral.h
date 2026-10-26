#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace sv
{
// CORAL: espessura/amplitude estéreo criada a partir de UMA única voz.
// 4 "vozes" derivadas da mesma gravação: atrasos de 14-35 ms, modulação lenta (desafinação
// de ~2-5 cents) e panorama estéreo. NÃO cria cantores independentes nem harmonias reais.
// Produz apenas o sinal wet (somado ao seco pelo processador).
class Coral
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        size = (int) (sr * 0.1) + 8;
        buf.assign ((size_t) size, 0.0f);
        pos = 0;
        for (int v = 0; v < V; ++v) phase[v] = 0.25 * v;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) pos] = 0.5f * (inL[i] + inR[i]);
            float l = 0.0f, r = 0.0f;
            for (int v = 0; v < V; ++v)
            {
                phase[v] += rate[v] / sampleRate; if (phase[v] >= 1.0) phase[v] -= 1.0;
                const double dMs = baseMs[v] + depthMs[v] * std::sin (6.283185307179586 * phase[v]);
                double rp = (double) pos - dMs * 0.001 * sampleRate;
                while (rp < 0.0) rp += size;
                const int i0 = (int) rp; const float fr = (float) (rp - i0);
                const float x = buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) ((i0 + 1) % size)] * fr;
                const float a = (pan[v] + 1.0f) * 0.78539816f; // constant-power
                l += x * std::cos (a) * 0.5f; r += x * std::sin (a) * 0.5f;
            }
            pos = (pos + 1) % size;
            wetL[i] = l; wetR[i] = r;
        }
    }

private:
    static constexpr int V = 4;
    const double baseMs[V]  = { 14.0, 21.0, 28.0, 35.0 };
    const double depthMs[V] = { 1.2, 1.6, 1.4, 1.8 };
    const double rate[V]    = { 0.31, 0.43, 0.57, 0.71 };
    const float  pan[V]     = { -0.8f, 0.8f, -0.4f, 0.4f };
    double sampleRate = 44100.0, phase[V] = {};
    int size = 0, pos = 0;
    std::vector<float> buf;
};
} // namespace sv
