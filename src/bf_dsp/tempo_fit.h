#pragma once

#include "result.h"
#include <vector>
#include <span>

namespace bf::dsp {

struct TempoCandidate {
    double bpm = 120.0;
    double multiplier = 1.0;
    double score = 1.0;
};

struct TempoEstimationResult {
    double bpm_global = 120.0;
    double bpm_theil_sen = 120.0;
    double slope_seconds_per_beat = 0.5;
    double intercept_seconds = 0.0;
    double ci_95_low = 119.5;
    double ci_95_high = 120.5;
    double local_bpm_p5 = 118.0;
    double local_bpm_p95 = 122.0;
    double target_suggestion = 120.0;
    bool has_half_double_ambiguity = false;
    std::vector<TempoCandidate> candidates;
};

// Fits robust linear model time = intercept + slope * index
// using Huber IRLS and Theil-Sen cross-check
Result<TempoEstimationResult> estimate_tempo_from_beats(
    std::span<const double> beat_times,
    std::span<const double> confidences = {}
);

// Generates and filters tempo octave candidates within [40, 250] BPM
std::vector<TempoCandidate> generate_octave_candidates(double base_bpm);

} // namespace bf::dsp
