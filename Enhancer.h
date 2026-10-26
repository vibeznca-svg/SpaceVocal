#pragma once
#include "Biquad.h"
#include <cmath>

namespace sv
{
// MAGIC: realçador vocal em DSP convencional (NÃO é IA).
//  1) presença: peak EQ ~3.5 kHz
//  2) tom: shelf de agudos ~6 kHz (bipolar)
//  3) saturação suave tanh com viés (harmónicos pares+ímpares), ganho unitário em sinais pequenos
//  4) excitador: harmónicos gerados a partir da banda > 2.5 kHz, misturados em paralelo
//  Saída = lerp(original, processado, amount * mix). amount = 0 => passagem exacta.
class Enhancer
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        hp.reset(); presence.reset(); tone.reset();
        hp.setHighpass (sr, 2500.0);
        lastKey = -1.0f;
    }
    void reset() { hp.reset(); presence.reset(); tone.reset(); }

    void setParams (float amountIn, float drive, float pres, float toneV, float harm, float mixIn)
    {
        amount = amountIn; mix = mixIn; harmonics = harm;
        k = 1.0f + drive * 3.0f;
        bias = 0.12f * harm;
        const float key = pres * 7.0f + toneV * 13.0f;
        if (key != lastKey)
        {
            lastKey = key;
            presence.setPeak (sampleRate, 3500.0, pres * 6.0, 0.8);
            tone.setHighShelf (sampleRate, 6000.0, (toneV - 0.5) * 2.0 * 5.0);
        }
    }

    void process (float* l, float* r, int n)
    {
        const float blend = amount * mix;
        if (blend <= 0.0f) return;
        const float norm = 1.0f / (k * (1.0f - std::tanh (bias) * std::tanh (bias)));
        const float dc = std::tanh (bias);
        for (int i = 0; i < n; ++i)
        {
            float* ch[2] = { &l[i], &r[i] };
            for (int c = 0; c < 2; ++c)
            {
                const float x = *ch[c];
                float y = presence.process (x, c);
                y = tone.process (y, c);
                const float sat = (std::tanh (k * y + bias) - dc) * norm;
                const float hf  = hp.process (y, c);
                const float ex  = std::tanh (3.0f * hf) * (1.0f / 3.0f);
                const float proc = sat + ex * harmonics * 0.6f;
                *ch[c] = x + (proc - x) * blend;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    float amount = 0, mix = 1, harmonics = 0.5f, k = 1.0f, bias = 0.0f, lastKey = -1.0f;
    Biquad hp, presence, tone;
};
} // namespace sv
