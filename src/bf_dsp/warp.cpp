#include "warp.h"
#include "smooth.h"
#include <algorithm>
#include <cmath>

namespace bf::dsp {

void WarpMap::set_data(
    std::vector<WarpAnchor> anchors,
    std::vector<WarpSegment> segments,
    WarpDiagnostics diagnostics,
    double detected_bpm,
    double target_bpm,
    double pre_ratio,
    double post_ratio
) {
    m_anchors = std::move(anchors);
    m_segments = std::move(segments);
    m_diagnostics = std::move(diagnostics);
    m_detected_bpm = detected_bpm;
    m_target_bpm = target_bpm;
    m_pre_ratio = pre_ratio;
    m_post_ratio = post_ratio;
}

double WarpMap::map(double src_time_sec) const noexcept {
    if (m_anchors.empty()) {
        return src_time_sec;
    }
    if (m_anchors.size() == 1) {
        return m_anchors[0].tgt_sec + (src_time_sec - m_anchors[0].src_anchor_sec);
    }

    const double x0 = m_anchors.front().src_anchor_sec;
    const double T0 = m_anchors.front().tgt_sec;
    const double xn = m_anchors.back().src_anchor_sec;
    const double Tn = m_anchors.back().tgt_sec;

    // Pre-roll (t < x_0)
    if (src_time_sec <= x0) {
        return T0 + m_pre_ratio * (src_time_sec - x0);
    }

    // Post-roll (t >= x_n)
    if (src_time_sec >= xn) {
        return Tn + m_post_ratio * (src_time_sec - xn);
    }

    // Binary search for anchor interval [x_i, x_{i+1}]
    auto it = std::upper_bound(
        m_anchors.begin(), m_anchors.end(), src_time_sec,
        [](double val, const WarpAnchor& a) {
            return val < a.src_anchor_sec;
        }
    );

    if (it == m_anchors.end()) {
        return Tn + m_post_ratio * (src_time_sec - xn);
    }

    size_t idx = static_cast<size_t>(std::distance(m_anchors.begin(), it) - 1);
    if (idx >= m_segments.size()) {
        idx = m_segments.size() - 1;
    }

    const auto& seg = m_segments[idx];
    return seg.tgt_start_sec + seg.ratio * (src_time_sec - seg.src_start_sec);
}

double WarpMap::map_inverse(double tgt_time_sec) const noexcept {
    if (m_anchors.empty()) {
        return tgt_time_sec;
    }
    if (m_anchors.size() == 1) {
        return m_anchors[0].src_anchor_sec + (tgt_time_sec - m_anchors[0].tgt_sec);
    }

    const double x0 = m_anchors.front().src_anchor_sec;
    const double T0 = m_anchors.front().tgt_sec;
    const double xn = m_anchors.back().src_anchor_sec;
    const double Tn = m_anchors.back().tgt_sec;

    // Pre-roll (tau < T_0)
    if (tgt_time_sec <= T0) {
        return x0 + (tgt_time_sec - T0) / m_pre_ratio;
    }

    // Post-roll (tau >= T_n)
    if (tgt_time_sec >= Tn) {
        return xn + (tgt_time_sec - Tn) / m_post_ratio;
    }

    // Binary search for anchor interval [T_i, T_{i+1}]
    auto it = std::upper_bound(
        m_anchors.begin(), m_anchors.end(), tgt_time_sec,
        [](double val, const WarpAnchor& a) {
            return val < a.tgt_sec;
        }
    );

    if (it == m_anchors.end()) {
        return xn + (tgt_time_sec - Tn) / m_post_ratio;
    }

    size_t idx = static_cast<size_t>(std::distance(m_anchors.begin(), it) - 1);
    if (idx >= m_segments.size()) {
        idx = m_segments.size() - 1;
    }

    const auto& seg = m_segments[idx];
    return seg.src_start_sec + (tgt_time_sec - seg.tgt_start_sec) / seg.ratio;
}

Result<void> WarpMap::validate() const {
    if (m_anchors.empty()) {
        return Result<void>::error("WarpMap validation failed: anchors array is empty");
    }

    for (size_t i = 0; i < m_anchors.size() - 1; ++i) {
        if (m_anchors[i + 1].src_anchor_sec <= m_anchors[i].src_anchor_sec) {
            return Result<void>::error(
                "WarpMap validation failed: source anchors not strictly increasing at index " +
                std::to_string(i) + " (" + std::to_string(m_anchors[i].src_anchor_sec) + " >= " +
                std::to_string(m_anchors[i + 1].src_anchor_sec) + ")"
            );
        }
        if (m_anchors[i + 1].tgt_sec <= m_anchors[i].tgt_sec) {
            return Result<void>::error(
                "WarpMap validation failed: target grid times not strictly increasing at index " +
                std::to_string(i)
            );
        }
    }

    for (size_t i = 0; i < m_segments.size(); ++i) {
        const auto& seg = m_segments[i];
        if (seg.ratio < HARD_RATIO_MIN || seg.ratio > HARD_RATIO_MAX) {
            return Result<void>::error(
                "WarpMap validation failed: ratio " + std::to_string(seg.ratio) +
                " outside hard bounds [0.5, 2.0] at segment " + std::to_string(i)
            );
        }

        if (i > 0) {
            if (m_segments[i - 1].src_end_sec != seg.src_start_sec) {
                return Result<void>::error(
                    "WarpMap validation failed: segment discontinuity between " +
                    std::to_string(i - 1) + " and " + std::to_string(i)
                );
            }
        }
    }

    if (m_pre_ratio < HARD_RATIO_MIN || m_pre_ratio > HARD_RATIO_MAX) {
        return Result<void>::error("Pre-roll ratio outside hard bounds [0.5, 2.0]");
    }
    if (m_post_ratio < HARD_RATIO_MIN || m_post_ratio > HARD_RATIO_MAX) {
        return Result<void>::error("Post-roll ratio outside hard bounds [0.5, 2.0]");
    }

    return Result<void>::ok();
}

Result<WarpMap> build_warp_map(
    const std::vector<double>& detected_beats,
    const std::vector<double>& confidences,
    const std::vector<double>& change_point_breaks,
    const WarpBuildConfig& config
) {
    if (detected_beats.size() < 2) {
        return Result<WarpMap>::error("Cannot build warp map with fewer than 2 beat anchors");
    }

    // Determine smoothing lambda
    double lambda = (config.custom_lambda >= 0.0)
        ? config.custom_lambda
        : lambda_from_detent(config.detent);

    // Smooth source anchors
    SmoothParams smooth_params;
    smooth_params.lambda = lambda;

    auto smooth_res = whittaker_huber_smooth(
        detected_beats,
        confidences,
        change_point_breaks,
        smooth_params
    );
    if (!smooth_res.is_ok()) {
        return Result<WarpMap>::error("Anchor smoothing failed: " + smooth_res.error_message());
    }
    const std::vector<double>& smoothed_anchors = smooth_res.value();

    // Generate target grid
    GridConfig grid_cfg;
    grid_cfg.target_bpm = config.target_bpm;
    grid_cfg.beats_per_bar = config.beats_per_bar;
    grid_cfg.phase_policy = config.phase_policy;
    grid_cfg.first_downbeat_index = config.first_downbeat_index;

    auto grid_res = generate_target_grid(smoothed_anchors, grid_cfg);
    if (!grid_res.is_ok()) {
        return Result<WarpMap>::error("Target grid generation failed: " + grid_res.error_message());
    }
    const auto& grid = grid_res.value();

    const size_t n = smoothed_anchors.size();
    std::vector<WarpAnchor> anchors(n);
    for (size_t i = 0; i < n; ++i) {
        anchors[i].index = i;
        anchors[i].beat_index = static_cast<int>(i);
        anchors[i].bar_index = static_cast<int>(i / config.beats_per_bar);
        anchors[i].beat_in_bar = static_cast<int>(i % config.beats_per_bar);
        anchors[i].is_downbeat = (anchors[i].beat_in_bar == 0);
        anchors[i].src_detected_sec = detected_beats[i];
        anchors[i].src_anchor_sec = smoothed_anchors[i];
        anchors[i].tgt_sec = grid.target_times[i];
        anchors[i].confidence = (i < confidences.size()) ? confidences[i] : 1.0;
        anchors[i].inferred = (anchors[i].confidence < 0.2);
    }

    // Build segments
    std::vector<WarpSegment> segments(n - 1);
    const double global_ratio = config.detected_bpm / config.target_bpm;

    WarpDiagnostics diag;
    diag.global_ratio = global_ratio;
    diag.min_ratio = 100.0;
    diag.max_ratio = 0.0;

    for (size_t i = 0; i < n - 1; ++i) {
        auto& seg = segments[i];
        seg.index = i;
        seg.from_anchor = i;
        seg.to_anchor = i + 1;
        seg.src_start_sec = anchors[i].src_anchor_sec;
        seg.src_end_sec = anchors[i + 1].src_anchor_sec;
        seg.tgt_start_sec = anchors[i].tgt_sec;
        seg.tgt_end_sec = anchors[i + 1].tgt_sec;

        double dt_src = seg.src_end_sec - seg.src_start_sec;
        double dt_tgt = seg.tgt_end_sec - seg.tgt_start_sec;
        seg.ratio = (dt_src > 0.0) ? (dt_tgt / dt_src) : 1.0;

        diag.min_ratio = std::min(diag.min_ratio, seg.ratio);
        diag.max_ratio = std::max(diag.max_ratio, seg.ratio);

        if (seg.ratio < HARD_RATIO_MIN || seg.ratio > HARD_RATIO_MAX) {
            diag.has_hard_bound_violation = true;
        }

        // Deviation from global ratio
        double local_dev = std::abs((seg.ratio / global_ratio) - 1.0);
        if (local_dev >= 0.08) {
            seg.is_warn = true;
            diag.num_warn_segments++;
        }
        if (local_dev >= EXTREME_RATIO_THRESHOLD) {
            seg.is_extreme = true;
            diag.num_extreme_segments++;
        }

        seg.correction_class = correction_class(seg.ratio, global_ratio);
    }

    double pre_ratio = segments.front().ratio;
    double post_ratio = segments.back().ratio;

    WarpMap warp_map;
    warp_map.set_data(
        std::move(anchors),
        std::move(segments),
        diag,
        config.detected_bpm,
        config.target_bpm,
        pre_ratio,
        post_ratio
    );

    auto valid_res = warp_map.validate();
    if (!valid_res.is_ok()) {
        return Result<WarpMap>::error(valid_res.error_message());
    }

    return Result<WarpMap>::ok(std::move(warp_map));
}

} // namespace bf::dsp
