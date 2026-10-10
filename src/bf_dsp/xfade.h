#pragma once

#include <cmath>
#include <numbers>
#include <span>

namespace bf::dsp {

// 20 ms equal-power crossfade constants and helpers
inline constexpr double DEFAULT_XFADE_DURATION_SEC = 0.020; // 20 ms per DECISION_SUMMARY §6

// Equal-power crossfade gain: g_a^2 + g_b^2 == 1.0
// t in [0.0, 1.0]
inline void equal_power_gains(double t, float& out_gain_a, float& out_gain_b) noexcept {
    if (t <= 0.0) {
        out_gain_a = 1.0f;
        out_gain_b = 0.0f;
        return;
    }
    if (t >= 1.0) {
        out_gain_a = 0.0f;
        out_gain_b = 1.0f;
        return;
    }
    double angle = t * (std::numbers::pi / 2.0);
    out_gain_a = static_cast<float>(std::cos(angle));
    out_gain_b = static_cast<float>(std::sin(angle));
}

// In-place crossfade between buffer A and buffer B into out_dst
inline void crossfade_buffers(
    std::span<const float> buf_a,
    std::span<const float> buf_b,
    std::span<float> out_dst
) noexcept {
    const size_t n = std::min({ buf_a.size(), buf_b.size(), out_dst.size() });
    if (n == 0) return;

    for (size_t i = 0; i < n; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(n);
        float ga = 0.0f, gb = 0.0f;
        equal_power_gains(t, ga, gb);
        out_dst[i] = buf_a[i] * ga + buf_b[i] * gb;
    }
}

} // namespace bf::dsp
