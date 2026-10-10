#pragma once

#include "result.h"
#include "units.h"
#include <vector>
#include <string>

namespace bf::dsp {

enum class PhasePolicy {
    PreserveStart, // Default: First anchor/downbeat retains source time
    DawReadyStart  // Align first downbeat to an integer bar boundary from t=0
};

struct GridConfig {
    double target_bpm = 120.0;
    int beats_per_bar = 4;
    PhasePolicy phase_policy = PhasePolicy::PreserveStart;
    size_t first_downbeat_index = 0;
};

struct TargetGrid {
    double bpm = 120.0;
    double period = 0.5;
    double phase_sec = 0.0;
    int beats_per_bar = 4;
    PhasePolicy policy = PhasePolicy::PreserveStart;
    std::vector<double> target_times; // T_k for each anchor k

    [[nodiscard]] double time_at(int k) const noexcept {
        return phase_sec + static_cast<double>(k) * period;
    }
};

// Generates target grid matching the input anchors count exactly
Result<TargetGrid> generate_target_grid(
    const std::vector<double>& source_anchors,
    const GridConfig& config
);

} // namespace bf::dsp
