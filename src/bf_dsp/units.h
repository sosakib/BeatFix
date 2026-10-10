#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>
#include <limits>

namespace bf::dsp {

// Hard and warning bounds for stretch ratio r (output duration / input duration)
inline constexpr double HARD_RATIO_MIN = 0.5;
inline constexpr double HARD_RATIO_MAX = 2.0;

inline constexpr double WARN_RATIO_MIN = 0.92; // 8% local deviation
inline constexpr double WARN_RATIO_MAX = 1.08;

inline constexpr double EXTREME_RATIO_THRESHOLD = 0.12; // 12% likely missed/extra beat
inline constexpr double GLOBAL_RATIO_WARN_THRESHOLD = 0.15; // 15% overall change warning

// Minimum anchor spacing relative to local period to prevent collapsed anchors
inline constexpr double MIN_ANCHOR_SPACING_FACTOR = 0.25;

// Unit conversions
[[nodiscard]] inline constexpr double bpm_to_period(double bpm) noexcept {
    return (bpm > 0.0) ? (60.0 / bpm) : 0.0;
}

[[nodiscard]] inline constexpr double period_to_bpm(double period) noexcept {
    return (period > 0.0) ? (60.0 / period) : 0.0;
}

[[nodiscard]] inline int64_t seconds_to_samples(double seconds, double sample_rate) noexcept {
    return static_cast<int64_t>(std::llround(seconds * sample_rate));
}

[[nodiscard]] inline double samples_to_seconds(int64_t samples, double sample_rate) noexcept {
    return (sample_rate > 0.0) ? (static_cast<double>(samples) / sample_rate) : 0.0;
}

// Natural <-> Strict detents
// Detents: Global (inf), 32, 16, 8 (default), 4, Strict (0)
enum class WarpDetent {
    Strict = 0,
    Detent4 = 4,
    Natural8 = 8,  // Default
    Detent16 = 16,
    Detent32 = 32,
    Global = 999999
};

[[nodiscard]] inline double lambda_from_detent(WarpDetent detent) noexcept {
    switch (detent) {
        case WarpDetent::Strict:
            return 0.0;
        case WarpDetent::Detent4:
            // 1 / (16 * sin^4(pi / 4)) = 1 / (16 * (1/sqrt(2))^4) = 1 / (16 * 0.25) = 1/4 = 0.25
            return 0.25;
        case WarpDetent::Natural8: {
            // 1 / (16 * sin^4(pi / 8)) ≈ 2.914213562
            double s = std::sin(std::numbers::pi / 8.0);
            return 1.0 / (16.0 * s * s * s * s);
        }
        case WarpDetent::Detent16: {
            double s = std::sin(std::numbers::pi / 16.0);
            return 1.0 / (16.0 * s * s * s * s); // ≈ 43.19
        }
        case WarpDetent::Detent32: {
            double s = std::sin(std::numbers::pi / 32.0);
            return 1.0 / (16.0 * s * s * s * s); // ≈ 677.2
        }
        case WarpDetent::Global:
            return std::numeric_limits<double>::infinity();
        default:
            return 2.914213562;
    }
}

[[nodiscard]] inline double lambda_from_cutoff_beats(double nc) noexcept {
    if (nc <= 0.0) return 0.0; // Strict
    if (std::isinf(nc) || nc > 10000.0) return std::numeric_limits<double>::infinity();
    double s = std::sin(std::numbers::pi / nc);
    return 1.0 / (16.0 * s * s * s * s);
}

// Correction class (0: <0.5%, 1: 0.5-2%, 2: 2-5%, 3: >=5%)
// Calculated from intensity p = 100 * |log2(r / g)| where g = B_detected / B_target
[[nodiscard]] inline int correction_class(double ratio, double global_ratio) noexcept {
    if (ratio <= 0.0 || global_ratio <= 0.0) return 0;
    double pct = 100.0 * (std::exp(std::abs(std::log(ratio / global_ratio))) - 1.0);
    if (pct < 0.5) return 0;
    if (pct < 2.0) return 1;
    if (pct < 5.0) return 2;
    return 3;
}

} // namespace bf::dsp
