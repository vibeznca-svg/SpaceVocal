#pragma once
#include "Biquad.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace sv
{
// Reverb FDN de 8 linhas (matriz de Hadamard ortonormal, perdas por linha e amortecimento).
// Produz APENAS o sinal wet. ROOM / HALL / PLATE diferem em comprimentos das linhas,
// multiplicador de decay, amortecimento, difusão de entrada e profundidade de modulação.
class SpaceReverb
{
public:
    enum Type { Room = 0, Hall = 1, Plate = 2 };

    void prepare (double sr)
    {
        sampleRate = sr;
        const int maxLine = (int) (sr * 0.3) + 8;
        for (auto& d : lines) { d.assign ((size_t) maxLine, 0.0f); }
        for (auto& v : lp) v = 0.0f;
        const int maxPre = (int) (sr * 0.2) + 8;
        preL.assign ((size_t) maxPre, 0.0f); preR.assign ((size_t) maxPre, 0.0f);
        preWrite = 0;
        const int apLens[4] = { (int) (sr * 0.0032), (int) (sr * 0.0024), (int) (sr * 0.0036), (int) (sr * 0.0027) };
        for (int i = 0; i < 4; ++i) { ap[i].buf.assign ((size_t) std::max (4, apLens[i]), 0.0f); ap[i].pos = 0; }
        writePos = 0;
        for (int i = 0; i < N; ++i) { lenCur[i] = baseMs[i] * 0.001 * sr * scaleCur; phase[i] = (double) i / N; }
        preCur = preTarget * 0.001 * sr;
        tone.reset();
        scaleCur = scaleTarget = typeScale (Plate);
        for (int i = 0; i < N; ++i) lenCur[i] = baseMs[i] * 0.001 * sr * scaleCur;
    }

    void reset()
    {
        for (auto& d : lines) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (preL.begin(), preL.end(), 0.0f); std::fill (preR.begin(), preR.end(), 0.0f);
        for (auto& a : ap) std::fill (a.buf.begin(), a.buf.end(), 0.0f);
        for (auto& v : lp) v = 0.0f;
        tone.reset();
    }

    void setParams (int type, float decaySec, float preMs, float damp, float toneV, float widthV)
    {
        scaleTarget = typeScale (type);
        decayEff  = std::max (0.1f, decaySec * (type == Room ? 0.6f : type == Hall ? 1.25f : 1.0f));
        dampCoef  = std::min (0.92f, std::max (0.0f, damp * 0.75f + (type == Plate ? -0.15f : type == Hall ? 0.05f : 0.0f)));
        apG       = type == Plate ? 0.7f : type == Hall ? 0.6f : 0.5f;
        modDepth  = type == Plate ? 0.0006 : type == Hall ? 0.0004 : 0.0001;   // segundos
        preTarget = preMs;
        width     = widthV;
        if (toneV != lastTone) { lastTone = toneV; tone.setHighShelf (sampleRate, 4000.0, (toneV - 0.5) * 2.0 * 8.0); }
    }

    // inL/inR -> wetL/wetR (n amostras)
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n)
    {
        // ganhos de feedback por linha (por bloco)
        float g[N];
        for (int i = 0; i < N; ++i)
            g[i] = std::pow (10.0f, -3.0f * (float) (lenCur[i] / sampleRate) / decayEff);

        const int maxLine = (int) lines[0].size();
        const int maxPre  = (int) preL.size();
        const double slew = 1.0 - std::exp (-1.0 / (0.25 * sampleRate));

        for (int s = 0; s < n; ++s)
        {
            // pré-atraso (suavizado)
            preCur += ((double) preTarget * 0.001 * sampleRate - preCur) * slew;
            preL[(size_t) preWrite] = inL[s]; preR[(size_t) preWrite] = inR[s];
            float pl = readFrac (preL, preWrite, preCur, maxPre);
            float pr = readFrac (preR, preWrite, preCur, maxPre);
            preWrite = (preWrite + 1) % maxPre;

            // difusão de entrada
            pl = ap[0].process (ap[1].process (pl, apG), apG);
            pr = ap[2].process (ap[3].process (pr, apG), apG);

            scaleCur += (scaleTarget - scaleCur) * slew;
            float y[N];
            for (int i = 0; i < N; ++i)
            {
                lenCur[i] = baseMs[i] * 0.001 * sampleRate * scaleCur;
                phase[i] += (0.15 + 0.03 * i) / sampleRate; if (phase[i] >= 1.0) phase[i] -= 1.0;
                const double mod = modDepth * sampleRate * std::sin (6.283185307179586 * phase[i]);
                double d = std::min ((double) (maxLine - 4), std::max (2.0, lenCur[i] + mod));
                const float raw = readFrac (lines[i], writePos, d, maxLine);
                lp[i] = (1.0f - dampCoef) * raw + dampCoef * lp[i];
                y[i] = lp[i];
            }
            // saídas estéreo
            float outL = (y[0] - y[2] + y[4] - y[6]) * 1.0f;
            float outR = (y[1] - y[3] + y[5] - y[7]) * 1.0f;

            // Hadamard 8x8 (ortonormal)
            for (int h = 1; h < N; h <<= 1)
                for (int i = 0; i < N; i += h << 1)
                    for (int j = i; j < i + h; ++j) { const float a = y[j], b = y[j + h]; y[j] = a + b; y[j + h] = a - b; }
            const float hn = 0.35355339f;
            for (int i = 0; i < N; ++i)
            {
                const float in = ((i & 1) ? pr : pl) * 0.35f;
                lines[i][(size_t) writePos] = in + y[i] * hn * g[i];
            }
            writePos = (writePos + 1) % maxLine;

            // tom + largura
            outL = tone.process (outL, 0); outR = tone.process (outR, 1);
            const float m = 0.5f * (outL + outR), sd = 0.5f * (outL - outR) * width;
            wetL[s] = m + sd; wetR[s] = m - sd;
        }
    }

private:
    static constexpr int N = 8;
    static float typeScale (int t) { return t == Room ? 0.55f : t == Hall ? 1.35f : 0.9f; }

    struct Allpass
    {
        std::vector<float> buf; int pos = 0;
        float process (float x, float g)
        {
            const float d = buf[(size_t) pos];
            const float v = x - g * d;
            buf[(size_t) pos] = v;
            pos = (pos + 1) % (int) buf.size();
            return d + g * v;
        }
    };

    static float readFrac (const std::vector<float>& b, int writeIdx, double delay, int size)
    {
        double rp = (double) writeIdx - delay;
        while (rp < 0.0) rp += size;
        const int i0 = (int) rp; const float fr = (float) (rp - i0);
        const int i1 = (i0 + 1) % size;
        return b[(size_t) i0] * (1.0f - fr) + b[(size_t) i1] * fr;
    }

    const double baseMs[N] = { 29.7, 37.1, 41.1, 43.7, 53.3, 61.7, 67.9, 73.1 };
    double sampleRate = 44100.0;
    std::vector<float> lines[N];
    std::vector<float> preL, preR;
    Allpass ap[4];
    float lp[N] = {};
    double lenCur[N] = {}, phase[N] = {};
    int writePos = 0, preWrite = 0;
    float scaleCur = 0.9f, scaleTarget = 0.9f, decayEff = 1.8f, dampCoef = 0.3f, apG = 0.7f, width = 1.0f, lastTone = -1.0f;
    float preTarget = 20.0f; double preCur = 0.0, modDepth = 0.0006;
    Biquad tone;
};
} // namespace sv
