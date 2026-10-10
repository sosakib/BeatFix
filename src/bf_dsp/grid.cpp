#include "grid.h"
#include <cmath>

namespace bf::dsp {

Result<TargetGrid> generate_target_grid(
    const std::vector<double>& source_anchors,
    const GridConfig& config
) {
    if (source_anchors.empty()) {
        return Result<TargetGrid>::error("Cannot generate target grid for empty anchors");
    }

    if (config.target_bpm < 20.0 || config.target_bpm > 400.0) {
        return Result<TargetGrid>::error("Target BPM " + std::to_string(config.target_bpm) + " out of valid range [20, 400]");
    }

    const double period = bpm_to_period(config.target_bpm);
    const size_t n = source_anchors.size();

    // Determine reference anchor for phase alignment
    size_t ref_idx = (config.first_downbeat_index < n) ? config.first_downbeat_index : 0;
    double ref_src_time = source_anchors[ref_idx];

    double phase_sec = 0.0;
    if (config.phase_policy == PhasePolicy::PreserveStart) {
        // T_{ref_idx} = phi + ref_idx * period = ref_src_time
        phase_sec = ref_src_time - static_cast<double>(ref_idx) * period;
    } else { // DawReadyStart
        // Align downbeat to nearest bar boundary
        double bar_period = period * static_cast<double>(config.beats_per_bar);
        int bar_num = static_cast<int>(std::ceil(ref_src_time / bar_period));
        if (bar_num < 1) bar_num = 1;
        double target_downbeat_time = static_cast<double>(bar_num) * bar_period;
        phase_sec = target_downbeat_time - static_cast<double>(ref_idx) * period;
    }

    TargetGrid grid;
    grid.bpm = config.target_bpm;
    grid.period = period;
    grid.phase_sec = phase_sec;
    grid.beats_per_bar = config.beats_per_bar;
    grid.policy = config.phase_policy;
    grid.target_times.resize(n);

    for (size_t i = 0; i < n; ++i) {
        grid.target_times[i] = phase_sec + static_cast<double>(i) * period;
    }

    return Result<TargetGrid>::ok(grid);
}

} // namespace bf::dsp
