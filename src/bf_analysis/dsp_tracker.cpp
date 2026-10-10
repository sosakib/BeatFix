#include "analyzer.h"
#include "../bf_dsp/tempo_fit.h"
#include "../bf_dsp/units.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace bf::analysis {

namespace {

// Computes Spectral Flux / Energy Novelty curve
std::vector<double> compute_novelty_curve(
    const std::vector<float>& mono,
    double sample_rate,
    int hop_size = 512,
    int win_size = 1024
) {
    const size_t total_samples = mono.size();
    if (total_samples < static_cast<size_t>(win_size)) {
        return {};
    }

    const size_t num_frames = (total_samples - win_size) / hop_size;
    std::vector<double> novelty(num_frames, 0.0);

    // Simple robust Hann window energy flux & band difference
    std::vector<double> prev_band_energies(8, 0.0);

    for (size_t f = 0; f < num_frames; ++f) {
        size_t start = f * hop_size;
        double frame_energy = 0.0;
        double hfc = 0.0; // High-frequency content proxy

        // Divide into 8 rough sub-bands in time domain via finite differences
        double band_energies[8] = { 0.0 };

        for (int i = 0; i < win_size; ++i) {
            double s = mono[start + i];
            double w = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * i / (win_size - 1)));
            double val = s * w;
            frame_energy += val * val;

            if (i > 0) {
                double diff = std::abs(val - mono[start + i - 1] * w);
                hfc += diff * diff;
            }
        }

        // Half-wave rectified flux
        double flux = std::max(0.0, frame_energy - prev_band_energies[0]) + 0.5 * std::max(0.0, hfc - prev_band_energies[1]);
        prev_band_energies[0] = frame_energy;
        prev_band_energies[1] = hfc;

        novelty[f] = flux;
    }

    // Normalize novelty curve
    double max_val = 0.0;
    for (double v : novelty) {
        if (v > max_val) max_val = v;
    }
    if (max_val > 1e-9) {
        for (double& v : novelty) v /= max_val;
    }

    // Local moving-average baseline subtraction
    const int smooth_win = 15;
    std::vector<double> smoothed(num_frames, 0.0);
    for (size_t f = 0; f < num_frames; ++f) {
        size_t w_start = (f >= smooth_win) ? (f - smooth_win) : 0;
        size_t w_end = std::min(f + smooth_win + 1, num_frames);
        double sum = 0.0;
        for (size_t k = w_start; k < w_end; ++k) sum += novelty[k];
        smoothed[f] = sum / (w_end - w_start);
    }

    for (size_t f = 0; f < num_frames; ++f) {
        novelty[f] = std::max(0.0, novelty[f] - smoothed[f]);
    }

    return novelty;
}

// Estimates tempo period in frames via autocorrelation
int estimate_period_frames(
    const std::vector<double>& novelty,
    double frame_rate,
    int min_bpm = 50,
    int max_bpm = 220
) {
    if (novelty.size() < 100) return 50;

    int min_lag = static_cast<int>(frame_rate * 60.0 / max_bpm);
    int max_lag = static_cast<int>(frame_rate * 60.0 / min_bpm);

    if (max_lag >= static_cast<int>(novelty.size() / 2)) {
        max_lag = static_cast<int>(novelty.size() / 2) - 1;
    }
    if (min_lag >= max_lag) return 50;

    int best_lag = min_lag;
    double max_corr = -1e9;

    for (int lag = min_lag; lag <= max_lag; ++lag) {
        double corr = 0.0;
        size_t count = novelty.size() - lag;
        for (size_t i = 0; i < count; ++i) {
            corr += novelty[i] * novelty[i + lag];
        }
        corr /= static_cast<double>(count);

        // Preference weighting for human musical range ~120 BPM
        double bpm = (frame_rate * 60.0) / lag;
        double weight = 1.0 - 0.2 * std::abs(bpm - 120.0) / 120.0;
        double weighted_corr = corr * weight;

        if (weighted_corr > max_corr) {
            max_corr = weighted_corr;
            best_lag = lag;
        }
    }

    return best_lag;
}

} // namespace

class DspBeatAnalyzer final : public IBeatAnalyzer {
public:
    DspBeatAnalyzer() = default;

    Result<AnalysisResult> analyze(
        const io::AudioData& audio,
        std::stop_token stop_tok
    ) override {
        if (audio.is_empty()) {
            return Result<AnalysisResult>::error("Cannot analyze empty audio buffer");
        }

        if (stop_tok.stop_requested()) {
            return Result<AnalysisResult>::error("Analysis cancelled by user");
        }

        const size_t total_samples = static_cast<size_t>(audio.total_frames);
        std::vector<float> mono(total_samples);
        if (audio.channels == 2 && !audio.right.empty()) {
            for (size_t i = 0; i < total_samples; ++i) {
                mono[i] = 0.5f * (audio.left[i] + audio.right[i]);
            }
        } else {
            mono = audio.left;
        }

        const int hop_size = 512;
        const double frame_rate = audio.sample_rate / static_cast<double>(hop_size);

        auto novelty = compute_novelty_curve(mono, audio.sample_rate, hop_size);
        if (novelty.empty()) {
            return Result<AnalysisResult>::error("Failed to compute onset novelty curve");
        }

        if (stop_tok.stop_requested()) {
            return Result<AnalysisResult>::error("Analysis cancelled by user");
        }

        int target_period = estimate_period_frames(novelty, frame_rate);
        double est_bpm = (frame_rate * 60.0) / static_cast<double>(target_period);

        // Dynamic programming beat tracker (Ellis DP)
        const size_t num_frames = novelty.size();
        std::vector<double> cum_score(num_frames, 0.0);
        std::vector<int> prev_beat(num_frames, -1);

        const double alpha = 8.0; // Transition penalty weight
        const int min_delta = std::max(1, static_cast<int>(target_period * 0.6));
        const int max_delta = static_cast<int>(target_period * 1.4);

        for (size_t t = 0; t < num_frames; ++t) {
            double best_prev_score = -1e9;
            int best_prev_idx = -1;

            int search_start = std::max(0, static_cast<int>(t) - max_delta);
            int search_end = std::max(0, static_cast<int>(t) - min_delta);

            for (int p = search_start; p <= search_end; ++p) {
                double delta = static_cast<double>(t - p);
                double ratio = delta / static_cast<double>(target_period);
                double transition_cost = -alpha * (std::log(ratio) * std::log(ratio));
                double score = cum_score[p] + transition_cost;

                if (score > best_prev_score) {
                    best_prev_score = score;
                    best_prev_idx = p;
                }
            }

            if (best_prev_idx >= 0) {
                cum_score[t] = novelty[t] + best_prev_score;
                prev_beat[t] = best_prev_idx;
            } else {
                cum_score[t] = novelty[t];
                prev_beat[t] = -1;
            }
        }

        if (stop_tok.stop_requested()) {
            return Result<AnalysisResult>::error("Analysis cancelled by user");
        }

        // Find best end frame
        size_t best_end = 0;
        double max_final_score = -1e9;
        size_t scan_start = (num_frames > static_cast<size_t>(target_period)) ? (num_frames - target_period) : 0;
        for (size_t t = scan_start; t < num_frames; ++t) {
            if (cum_score[t] > max_final_score) {
                max_final_score = cum_score[t];
                best_end = t;
            }
        }

        // Backtrack
        std::vector<int> beat_frames;
        int curr = static_cast<int>(best_end);
        while (curr >= 0) {
            beat_frames.push_back(curr);
            curr = prev_beat[curr];
        }
        std::reverse(beat_frames.begin(), beat_frames.end());

        if (beat_frames.size() < 2) {
            return Result<AnalysisResult>::error("No steady beat sequence found");
        }

        // Convert frames to seconds
        std::vector<double> beat_times(beat_frames.size());
        for (size_t i = 0; i < beat_frames.size(); ++i) {
            beat_times[i] = static_cast<double>(beat_frames[i]) / frame_rate;
        }

        // Estimate global tempo using robust Huber IRLS
        auto tempo_res = bf::dsp::estimate_tempo_from_beats(beat_times);
        double global_bpm = tempo_res.is_ok() ? tempo_res.value().bpm_global : est_bpm;

        // Downbeat estimation: test 4 possible bar phases
        double best_phase_energy = -1.0;
        int best_bar_phase = 0;
        for (int phase = 0; phase < 4; ++phase) {
            double energy = 0.0;
            size_t count = 0;
            for (size_t i = phase; i < beat_frames.size(); i += 4) {
                energy += novelty[beat_frames[i]];
                count++;
            }
            if (count > 0) energy /= static_cast<double>(count);
            if (energy > best_phase_energy) {
                best_phase_energy = energy;
                best_bar_phase = phase;
            }
        }

        AnalysisResult result;
        result.bpm_global = global_bpm;
        result.bpm_range_low = tempo_res.is_ok() ? tempo_res.value().local_bpm_p5 : (global_bpm - 2.0);
        result.bpm_range_high = tempo_res.is_ok() ? tempo_res.value().local_bpm_p95 : (global_bpm + 2.0);
        result.analyzer_id = "dsp_tracker";

        const size_t num_beats = beat_times.size();
        result.beats.resize(num_beats);

        for (size_t i = 0; i < num_beats; ++i) {
            Beat& b = result.beats[i];
            b.time_sec = beat_times[i];
            b.confidence = std::clamp(novelty[beat_frames[i]] * 1.5, 0.3, 1.0);
            b.beat_in_bar = (static_cast<int>(i) - best_bar_phase + 4000) % 4;
            b.is_downbeat = (b.beat_in_bar == 0);
            b.bar_index = (static_cast<int>(i) - best_bar_phase) / 4;
            b.inferred = false;
        }

        // Initialize continuity breaks array (size num_beats - 2)
        if (num_beats > 2) {
            result.change_point_breaks.assign(num_beats - 2, 1.0);
        }

        // Stability classification
        double bpm_spread = result.bpm_range_high - result.bpm_range_low;
        if (bpm_spread < 2.5) {
            result.stability_class = StabilityClass::Stable;
            result.global_confidence = GlobalConfidence::High;
            result.confidence_reason = "Stable tempo detected (" + std::to_string(static_cast<int>(std::round(global_bpm))) + " BPM)";
        } else if (bpm_spread < 8.0) {
            result.stability_class = StabilityClass::Drifting;
            result.global_confidence = GlobalConfidence::Medium;
            result.confidence_reason = "Tempo drift detected across sections (" + std::to_string(static_cast<int>(result.bpm_range_low)) + " to " + std::to_string(static_cast<int>(result.bpm_range_high)) + " BPM)";
        } else {
            result.stability_class = StabilityClass::Unreliable;
            result.global_confidence = GlobalConfidence::Low;
            result.confidence_reason = "High tempo variability or irregular rhythmic structure";
        }

        return Result<AnalysisResult>::ok(std::move(result));
    }

    [[nodiscard]] std::string id() const override {
        return "dsp_tracker";
    }
};

std::unique_ptr<IBeatAnalyzer> create_dsp_beat_analyzer() {
    return std::make_unique<DspBeatAnalyzer>();
}

} // namespace bf::analysis
