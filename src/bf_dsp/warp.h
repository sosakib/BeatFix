#pragma once

#include "result.h"
#include "units.h"
#include "grid.h"
#include <vector>
#include <string>

namespace bf::dsp {

struct WarpAnchor {
    size_t index = 0;
    int beat_index = 0;
    int bar_index = 0;
    int beat_in_bar = 0;
    bool is_downbeat = false;
    double src_detected_sec = 0.0;
    double src_anchor_sec = 0.0; // Smoothed x_i
    double tgt_sec = 0.0;        // Grid T_i
    double confidence = 1.0;
    bool inferred = false;
};

struct WarpSegment {
    size_t index = 0;
    size_t from_anchor = 0;
    size_t to_anchor = 1;
    double src_start_sec = 0.0;
    double src_end_sec = 0.0;
    double tgt_start_sec = 0.0;
    double tgt_end_sec = 0.0;
    double ratio = 1.0;          // r_i = (tgt_end - tgt_start) / (src_end - src_start)
    int correction_class = 0;    // 0..3
    bool is_warn = false;
    bool is_extreme = false;
};

struct WarpDiagnostics {
    double min_ratio = 1.0;
    double max_ratio = 1.0;
    double global_ratio = 1.0;
    size_t num_warn_segments = 0;
    size_t num_extreme_segments = 0;
    bool has_hard_bound_violation = false;
    std::string message;
};

class WarpMap {
public:
    WarpMap() = default;

    // Direct piecewise-linear mapping t (source) -> tau (target)
    [[nodiscard]] double map(double src_time_sec) const noexcept;

    // Exact closed-form inverse mapping tau (target) -> t (source)
    [[nodiscard]] double map_inverse(double tgt_time_sec) const noexcept;

    // Validates monotonicity and ratio bounds
    Result<void> validate() const;

    const std::vector<WarpAnchor>& anchors() const noexcept { return m_anchors; }
    const std::vector<WarpSegment>& segments() const noexcept { return m_segments; }
    const WarpDiagnostics& diagnostics() const noexcept { return m_diagnostics; }
    double target_bpm() const noexcept { return m_target_bpm; }
    double detected_bpm() const noexcept { return m_detected_bpm; }
    double pre_ratio() const noexcept { return m_pre_ratio; }
    double post_ratio() const noexcept { return m_post_ratio; }

    void set_data(
        std::vector<WarpAnchor> anchors,
        std::vector<WarpSegment> segments,
        WarpDiagnostics diagnostics,
        double detected_bpm,
        double target_bpm,
        double pre_ratio,
        double post_ratio
    );

private:
    std::vector<WarpAnchor> m_anchors;
    std::vector<WarpSegment> m_segments;
    WarpDiagnostics m_diagnostics;
    double m_detected_bpm = 120.0;
    double m_target_bpm = 120.0;
    double m_pre_ratio = 1.0;
    double m_post_ratio = 1.0;
};

struct WarpBuildConfig {
    double detected_bpm = 120.0;
    double target_bpm = 120.0;
    WarpDetent detent = WarpDetent::Natural8;
    double custom_lambda = -1.0; // If >= 0, overrides detent
    PhasePolicy phase_policy = PhasePolicy::PreserveStart;
    int beats_per_bar = 4;
    size_t first_downbeat_index = 0;
};

// Builds a validated WarpMap from detected anchors and configuration
Result<WarpMap> build_warp_map(
    const std::vector<double>& detected_beats,
    const std::vector<double>& confidences,
    const std::vector<double>& change_point_breaks,
    const WarpBuildConfig& config
);

} // namespace bf::dsp
