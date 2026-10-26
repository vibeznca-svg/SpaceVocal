#pragma once
#include <atomic>
#include <cmath>
#include <algorithm>

namespace sv
{
// Compressor feed-forward, ligado em estéreo, domínio logarítmico, joelho suave.
// Suavização da redução de ganho (dB) com attack/release independentes.
class Compressor
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { grState = 0.0; grMeter.store (0.0f); }

    void setParams (float thresholdDb, float ratio, float attackMs, float releaseMs, float kneeDb, float makeupDb)
    {
        threshold = thresholdDb;
        ratio     = std::max (1.0f, ratio);
        knee      = std::max (0.0f, kneeDb);
        makeup    = makeupDb;
        atkCoef   = std::exp (-1.0 / (std::max (0.05f, attackMs)  * 0.001 * sampleRate));
        relCoef   = std::exp (-1.0 / (std::max (1.0f,  releaseMs) * 0.001 * sampleRate));
    }

    void process (float* l, float* r, int n)
    {
        float maxGr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max (std::abs (l[i]), std::abs (r[i]));
            const float xDb  = 20.0f * std::log10 (std::max (peak, 1.0e-6f));

            // característica estática com joelho suave -> redução em dB (>= 0)
            const float over = xDb - threshold;
            float gr = 0.0f;
            if (knee > 0.0f && 2.0f * std::abs (over) <= knee)
            {
                const float t = over + knee * 0.5f;
                gr = (1.0f - 1.0f / ratio) * t * t / (2.0f * knee);
            }
            else if (over > 0.0f)
                gr = over * (1.0f - 1.0f / ratio);

            const double c = (gr > grState) ? atkCoef : relCoef;
            grState = c * grState + (1.0 - c) * gr;

            const float g = std::pow (10.0f, (-(float) grState + makeup) * 0.05f);
            l[i] *= g;
            r[i] *= g;
            maxGr = std::max (maxGr, (float) grState);
        }
        grMeter.store (maxGr);
    }

    float getGainReductionDb() const { return grMeter.load(); }

private:
    double sampleRate = 44100.0, grState = 0.0, atkCoef = 0.0, relCoef = 0.0;
    float threshold = -18.0f, ratio = 3.0f, knee = 6.0f, makeup = 0.0f;
    std::atomic<float> grMeter { 0.0f };
};
} // namespace sv
