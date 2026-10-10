#pragma once

#include "result.h"
#include <span>
#include <vector>

namespace bf::dsp {

struct LoudnessMetrics {
    double integrated_lufs = -70.0;
    double sample_peak_dbfs = -100.0;
    double true_peak_dbtp = -100.0;
    double sample_peak_linear = 0.0;
    double true_peak_linear = 0.0;
};

// Measures integrated LUFS and true peak according to ITU-R BS.1770 / EBU R128
Result<LoudnessMetrics> measure_loudness(
    std::span<const float> channel_left,
    std::span<const float> channel_right,
    double sample_rate
);

} // namespace bf::dsp
