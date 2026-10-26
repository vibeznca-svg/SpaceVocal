#pragma once
#include "Biquad.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace sv
{
// Delay estéreo com tempo suavizado (muda de BPM/tempo sem estalos), filtros no feedback,
// ping-pong opcional e limitação suave (tanh) do feedback. Produz APENAS o sinal wet.
class TempoDelay
{
public:
    static constexpr float maxFeedback = 0.92f;
    static constexpr double maxTimeSec = 2.0;

    void prepare (double sr)
    {
        sampleRate = sr;
        size = (int) (sr * (maxTimeSec + 0.1)) + 8;
        bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
        pos = 0; timeCur = -1.0;
        hp.reset(); lpf.reset();
    }
    void reset() { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); hp.reset(); lpf.reset(); }

    void setParams (float timeMs, float fb, float lowCutHz, float highCutHz, bool pingPong)
    {
        timeTarget = std::min ((double) timeMs, maxTimeSec * 1000.0) * 0.001 * sampleRate;
        if (timeCur < 0.0) timeCur = timeTarget;
        feedback = std::min (std::max (fb, 0.0f), maxFeedback);
        ping = pingPong;
        if (lowCutHz != lastLow)   { lastLow = lowCutHz;   hp.setHighpass (sampleRate, lowCutHz); }
        if (highCutHz != lastHigh) { lastHigh = highCutHz; lpf.setLowpass (sampleRate, highCutHz); }
    }

    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n)
    {
        const double slew = 1.0 - std::exp (-1.0 / (0.08 * sampleRate));
        for (int i = 0; i < n; ++i)
        {
            timeCur += (timeTarget - timeCur) * slew;
            const double d = std::max (1.0, std::min (timeCur, (double) (size - 4)));
            const float dl = read (bufL, d), dr = read (bufR, d);

            float fl = dl, fr = dr;
            fl = lpf.process (hp.process (fl, 0), 0); fr = lpf.process (hp.process (fr, 1), 1);
            // (hp/lpf partilham coeficientes; estados por canal)
            fl = std::tanh (fl) ; fr = std::tanh (fr);

            float wl, wr;
            if (ping) { const float m = 0.5f * (inL[i] + inR[i]); wl = m + fr * feedback; wr = fl * feedback; }
            else      { wl = inL[i] + fl * feedback; wr = inR[i] + fr * feedback; }
            bufL[(size_t) pos] = wl; bufR[(size_t) pos] = wr;
            pos = (pos + 1) % size;
            wetL[i] = dl; wetR[i] = dr;
        }
    }

private:
    float read (const std::vector<float>& b, double delay) const
    {
        double rp = (double) pos - delay;
        while (rp < 0.0) rp += size;
        const int i0 = (int) rp; const float fr = (float) (rp - i0);
        return b[(size_t) i0] * (1.0f - fr) + b[(size_t) ((i0 + 1) % size)] * fr;
    }

    double sampleRate = 44100.0, timeCur = -1.0, timeTarget = 0.0;
    int size = 0, pos = 0;
    float feedback = 0.3f, lastLow = -1.0f, lastHigh = -1.0f;
    bool ping = true;
    std::vector<float> bufL, bufR;
    Biquad hp, lpf;
};
} // namespace sv
