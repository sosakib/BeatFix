#include "loudness.h"
#include "../../third_party/libebur128/ebur128.h"
#include <algorithm>
#include <cmath>

namespace bf::dsp {

Result<LoudnessMetrics> measure_loudness(
    std::span<const float> channel_left,
    std::span<const float> channel_right,
    double sample_rate
) {
    const size_t num_frames = channel_left.size();
    if (num_frames == 0) {
        return Result<LoudnessMetrics>::error("Cannot measure loudness of empty buffer");
    }

    const unsigned int channels = channel_right.empty() ? 1u : 2u;
    ebur128_state* st = ebur128_init(
        channels,
        static_cast<size_t>(sample_rate),
        EBUR128_MODE_I | EBUR128_MODE_TRUE_PEAK
    );

    if (!st) {
        return Result<LoudnessMetrics>::error("Failed to initialize ebur128 state");
    }

    // Interleave into temporary buffer for ebur128
    const size_t chunk_frames = 4096;
    std::vector<float> interleaved(chunk_frames * channels);

    float max_sample_linear = 0.0f;

    size_t processed = 0;
    while (processed < num_frames) {
        size_t current = std::min(chunk_frames, num_frames - processed);

        if (channels == 2) {
            for (size_t i = 0; i < current; ++i) {
                float l = channel_left[processed + i];
                float r = channel_right[processed + i];
                interleaved[i * 2] = l;
                interleaved[i * 2 + 1] = r;
                max_sample_linear = std::max({ max_sample_linear, std::abs(l), std::abs(r) });
            }
        } else {
            for (size_t i = 0; i < current; ++i) {
                float l = channel_left[processed + i];
                interleaved[i] = l;
                max_sample_linear = std::max(max_sample_linear, std::abs(l));
            }
        }

        ebur128_add_frames_float(st, interleaved.data(), current);
        processed += current;
    }

    double global_lufs = -70.0;
    ebur128_loudness_global(st, &global_lufs);

    double max_true_peak = 0.0;
    for (unsigned int c = 0; c < channels; ++c) {
        double tp = 0.0;
        ebur128_true_peak(st, c, &tp);
        if (tp > max_true_peak) {
            max_true_peak = tp;
        }
    }

    ebur128_destroy(&st);

    LoudnessMetrics metrics;
    metrics.integrated_lufs = global_lufs;
    metrics.sample_peak_linear = max_sample_linear;
    metrics.true_peak_linear = max_true_peak;
    metrics.sample_peak_dbfs = (max_sample_linear > 1e-9f) ? (20.0 * std::log10(max_sample_linear)) : -120.0;
    metrics.true_peak_dbtp = (max_true_peak > 1e-9) ? (20.0 * std::log10(max_true_peak)) : -120.0;

    return Result<LoudnessMetrics>::ok(metrics);
}

} // namespace bf::dsp
