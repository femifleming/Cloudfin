#pragma once
#include "Utils.h"

namespace fp
{
//==============================================================================
// The last thing on the main output: a stereo-linked peak limiter. The gain drops the
// instant a sample would go over the ceiling (so nothing ever clips), then recovers over
// ~80 ms. No look-ahead, so no latency; below the ceiling the signal is untouched.
class PeakLimiter
{
public:
    static constexpr float ceiling = 0.891f;   // -1 dBFS: headroom for converters and inter-sample peaks

    void prepare (double sr) { release = (float) (1.0 - std::exp (-1.0 / (0.08 * sr))); gain = 1.0f; }

    void process (float* L, float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max (std::abs (L[i]), std::abs (R[i]));
            const float need = peak > ceiling ? ceiling / peak : 1.0f;
            gain = need < gain ? need : gain + release * (1.0f - gain);
            if (gain > 0.99999f) gain = 1.0f;
            L[i] = std::clamp (L[i] * gain, -ceiling, ceiling);   // (the clamp never bites; it's a promise)
            R[i] = std::clamp (R[i] * gain, -ceiling, ceiling);
        }
    }

    float getGain() const noexcept { return gain; }

private:
    float gain = 1.0f, release = 0.0005f;
};
} // namespace fp
