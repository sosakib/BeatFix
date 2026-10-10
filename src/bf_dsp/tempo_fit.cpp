#include "tempo_fit.h"
#include "units.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace bf::dsp {

namespace {

double median_of_vector(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (v.size() % 2 != 0) {
        return v[mid];
    }
    double v_mid = v[mid];
    std::nth_element(v.begin(), v.begin() + mid - 1, v.end());
    return 0.5 * (v[mid - 1] + v_mid);
}

// Robust Huber linear fit: y_i = a + b * x_i
void huber_linear_fit(
    const std::vector<double>& x,
    const std::vector<double>& y,
    const std::vector<double>& initial_w,
    double& out_intercept,
    double& out_slope,
    int max_iters = 10,
    double huber_k = 1.345
) {
    const size_t n = x.size();
    std::vector<double> w = initial_w;

    // Start with OLS
    double sum_w = 0.0, sum_wx = 0.0, sum_wy = 0.0, sum_wxx = 0.0, sum_wxy = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum_w += w[i];
        sum_wx += w[i] * x[i];
        sum_wy += w[i] * y[i];
        sum_wxx += w[i] * x[i] * x[i];
        sum_wxy += w[i] * x[i] * y[i];
    }
    double denom = sum_w * sum_wxx - sum_wx * sum_wx;
    if (std::abs(denom) > 1e-12) {
        out_slope = (sum_w * sum_wxy - sum_wx * sum_wy) / denom;
        out_intercept = (sum_wy - out_slope * sum_wx) / sum_w;
    } else {
        out_slope = 0.5;
        out_intercept = 0.0;
    }

    std::vector<double> residuals(n);
    for (int iter = 0; iter < max_iters; ++iter) {
        for (size_t i = 0; i < n; ++i) {
            residuals[i] = std::abs(y[i] - (out_intercept + out_slope * x[i]));
        }

        std::vector<double> res_copy = residuals;
        double mad = median_of_vector(res_copy);
        double s = 1.4826 * mad;
        if (s < 1e-6) s = 1e-6;

        sum_w = 0.0; sum_wx = 0.0; sum_wy = 0.0; sum_wxx = 0.0; sum_wxy = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double u = residuals[i] / s;
            double huber_weight = (u <= huber_k) ? 1.0 : (huber_k / u);
            w[i] = std::max(initial_w[i] * huber_weight, 1e-4);

            sum_w += w[i];
            sum_wx += w[i] * x[i];
            sum_wy += w[i] * y[i];
            sum_wxx += w[i] * x[i] * x[i];
            sum_wxy += w[i] * x[i] * y[i];
        }

        denom = sum_w * sum_wxx - sum_wx * sum_wx;
        if (std::abs(denom) > 1e-12) {
            out_slope = (sum_w * sum_wxy - sum_wx * sum_wy) / denom;
            out_intercept = (sum_wy - out_slope * sum_wx) / sum_w;
        }
    }
}

// Compute Theil-Sen slope
double compute_theil_sen_slope(const std::vector<double>& x, const std::vector<double>& y) {
    const size_t n = x.size();
    std::vector<double> slopes;

    if (n <= 200) {
        // All pairs
        slopes.reserve(n * (n - 1) / 2);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                double dx = x[j] - x[i];
                if (dx != 0.0) {
                    slopes.push_back((y[j] - y[i]) / dx);
                }
            }
        }
    } else {
        // Sampled pairs for efficiency
        slopes.reserve(20000);
        std::mt19937_64 rng(42); // Deterministic seed
        std::uniform_int_distribution<size_t> dist(0, n - 1);
        for (int k = 0; k < 20000; ++k) {
            size_t i = dist(rng);
            size_t j = dist(rng);
            if (i != j) {
                double dx = x[j] - x[i];
                if (dx != 0.0) {
                    slopes.push_back((y[j] - y[i]) / dx);
                }
            }
        }
    }

    return median_of_vector(slopes);
}

} // namespace

std::vector<TempoCandidate> generate_octave_candidates(double base_bpm) {
    static constexpr double MULTIPLIERS[] = { 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 1.5, 2.0, 3.0 };
    std::vector<TempoCandidate> candidates;

    for (double mult : MULTIPLIERS) {
        double candidate_bpm = base_bpm * mult;
        if (candidate_bpm >= 40.0 && candidate_bpm <= 250.0) {
            TempoCandidate cand;
            cand.bpm = candidate_bpm;
            cand.multiplier = mult;
            // Preference score centered around 1.0 and 80-140 BPM sweet spot
            double dist_from_unity = std::abs(std::log2(mult));
            double sweet_spot = 1.0 - 0.2 * std::abs(candidate_bpm - 120.0) / 120.0;
            cand.score = std::max(0.1, sweet_spot / (1.0 + dist_from_unity));
            candidates.push_back(cand);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });

    return candidates;
}

Result<TempoEstimationResult> estimate_tempo_from_beats(
    std::span<const double> beat_times,
    std::span<const double> confidences
) {
    const size_t n = beat_times.size();
    if (n < 2) {
        return Result<TempoEstimationResult>::error("At least 2 beats required to estimate tempo");
    }

    std::vector<double> indices(n);
    std::vector<double> times(beat_times.begin(), beat_times.end());
    std::vector<double> w(n, 1.0);

    for (size_t i = 0; i < n; ++i) {
        indices[i] = static_cast<double>(i);
        if (i < confidences.size()) {
            w[i] = std::max(confidences[i], 1e-4);
        }
    }

    double intercept = 0.0;
    double slope = 0.0;
    huber_linear_fit(indices, times, w, intercept, slope);

    if (slope <= 0.0) {
        return Result<TempoEstimationResult>::error("Invalid non-positive slope in beat line fit");
    }

    double bpm_global = 60.0 / slope;

    // Theil-Sen cross-check
    double ts_slope = compute_theil_sen_slope(indices, times);
    double bpm_theil_sen = (ts_slope > 0.0) ? (60.0 / ts_slope) : bpm_global;

    // Calculate local BPMs for range [p5, p95]
    std::vector<double> local_bpms;
    local_bpms.reserve(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        double ioi = times[i + 1] - times[i];
        if (ioi > 0.05 && ioi < 3.0) {
            local_bpms.push_back(60.0 / ioi);
        }
    }

    double p5 = bpm_global;
    double p95 = bpm_global;
    if (!local_bpms.empty()) {
        std::sort(local_bpms.begin(), local_bpms.end());
        size_t idx5 = static_cast<size_t>(0.05 * local_bpms.size());
        size_t idx95 = static_cast<size_t>(0.95 * local_bpms.size());
        if (idx95 >= local_bpms.size()) idx95 = local_bpms.size() - 1;
        p5 = local_bpms[idx5];
        p95 = local_bpms[idx95];
    }

    // Target suggestion: round(bpm_global), or half-integer if stable and close to .5
    double rounded = std::round(bpm_global);
    double target_sugg = rounded;
    if (std::abs(bpm_global - rounded) > 0.25 && (p95 - p5 < 4.0)) {
        target_sugg = (bpm_global > rounded) ? (rounded + 0.5) : (rounded - 0.5);
    }

    TempoEstimationResult res;
    res.bpm_global = bpm_global;
    res.bpm_theil_sen = bpm_theil_sen;
    res.slope_seconds_per_beat = slope;
    res.intercept_seconds = intercept;
    res.ci_95_low = bpm_global - 0.05 * (p95 - p5);
    res.ci_95_high = bpm_global + 0.05 * (p95 - p5);
    res.local_bpm_p5 = p5;
    res.local_bpm_p95 = p95;
    res.target_suggestion = target_sugg;
    res.candidates = generate_octave_candidates(bpm_global);

    if (res.candidates.size() >= 2) {
        double margin = (res.candidates[0].score - res.candidates[1].score) / res.candidates[0].score;
        res.has_half_double_ambiguity = (margin < 0.15);
    }

    return Result<TempoEstimationResult>::ok(res);
}

} // namespace bf::dsp
