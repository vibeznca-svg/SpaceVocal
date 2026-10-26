#pragma once
#include <cmath>
#include <algorithm>

namespace sv
{
// Biquad RBJ com coeficientes calculados sem alocação (seguro para a thread de áudio).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

    void reset() { for (int i = 0; i < 2; ++i) z1[i] = z2[i] = 0.0; }

    inline float process (float x, int ch = 0) noexcept
    {
        const double in = x;
        const double y = b0 * in + z1[ch];
        z1[ch] = b1 * in - a1 * y + z2[ch];
        z2[ch] = b2 * in - a2 * y;
        return (float) y;
    }

    void setLowpass (double sr, double f, double q = 0.70710678) { f = clampF (sr, f); const double w = w0 (sr, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }

    void setHighpass (double sr, double f, double q = 0.70710678) { f = clampF (sr, f); const double w = w0 (sr, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }

    void setPeak (double sr, double f, double gainDb, double q = 0.8) { f = clampF (sr, f); const double A = std::pow (10.0, gainDb / 40.0);
        const double w = w0 (sr, f), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }

    void setHighShelf (double sr, double f, double gainDb) { f = clampF (sr, f); const double A = std::pow (10.0, gainDb / 40.0);
        const double w = w0 (sr, f), c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0);
        const double beta = 2.0 * std::sqrt (A) * al;
        norm (A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - beta),
              (A + 1) - (A - 1) * c + beta, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta); }

private:
    static double clampF (double sr, double f) { return std::min (std::max (f, 10.0), sr * 0.45); }
    static double w0 (double sr, double f) { return 6.283185307179586 * f / sr; }
    void norm (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    { b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0; }
};
} // namespace sv
