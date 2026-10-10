#include "peaks.h"
#include "warp.h"
#include <cmath>
#include <algorithm>

namespace bf::dsp {

void PeakPyramid::build_from_audio(
    std::span<const float> channel_left,
    std::span<const float> channel_right,
    double sample_rate
) {
    m_sample_rate = sample_rate;
    m_total_samples = static_cast<int64_t>(channel_left.size());
    m_levels.clear();

    if (m_total_samples == 0) return;

    static constexpr int BIN_SIZES[] = { 64, 256, 1024, 4096, 16384 };
    bool is_stereo = !channel_right.empty();

    m_levels.reserve(std::size(BIN_SIZES));

    for (int bin_size : BIN_SIZES) {
        PeakLevel level;
        level.samples_per_bin = bin_size;

        const size_t num_bins = static_cast<size_t>((m_total_samples + bin_size - 1) / bin_size);
        level.bins_left.resize(num_bins);
        if (is_stereo) {
            level.bins_right.resize(num_bins);
        }

        for (size_t b = 0; b < num_bins; ++b) {
            size_t start = b * bin_size;
            size_t end = std::min(start + static_cast<size_t>(bin_size), static_cast<size_t>(m_total_samples));

            float min_l = 0.0f, max_l = 0.0f;
            double sum_sq_l = 0.0;
            if (start < end) {
                min_l = channel_left[start];
                max_l = channel_left[start];
            }

            for (size_t s = start; s < end; ++s) {
                float v = channel_left[s];
                if (v < min_l) min_l = v;
                if (v > max_l) max_l = v;
                sum_sq_l += static_cast<double>(v) * v;
            }

            size_t count = end - start;
            level.bins_left[b] = PeakBin{
                min_l,
                max_l,
                (count > 0) ? static_cast<float>(std::sqrt(sum_sq_l / count)) : 0.0f
            };

            if (is_stereo) {
                float min_r = 0.0f, max_r = 0.0f;
                double sum_sq_r = 0.0;
                if (start < end) {
                    min_r = channel_right[start];
                    max_r = channel_right[start];
                }
                for (size_t s = start; s < end; ++s) {
                    float v = channel_right[s];
                    if (v < min_r) min_r = v;
                    if (v > max_r) max_r = v;
                    sum_sq_r += static_cast<double>(v) * v;
                }
                level.bins_right[b] = PeakBin{
                    min_r,
                    max_r,
                    (count > 0) ? static_cast<float>(std::sqrt(sum_sq_r / count)) : 0.0f
                };
            }
        }

        m_levels.push_back(std::move(level));
    }
}

PeakPyramid PeakPyramid::map_through_warp(const WarpMap& warp_map) const {
    PeakPyramid result;
    result.m_sample_rate = m_sample_rate;
    if (m_levels.empty() || m_total_samples == 0) return result;

    double total_src_sec = static_cast<double>(m_total_samples) / m_sample_rate;
    double total_tgt_sec = warp_map.map(total_src_sec);
    result.m_total_samples = static_cast<int64_t>(std::llround(total_tgt_sec * m_sample_rate));

    result.m_levels.reserve(m_levels.size());

    for (const auto& src_level : m_levels) {
        PeakLevel tgt_level;
        tgt_level.samples_per_bin = src_level.samples_per_bin;

        size_t num_tgt_bins = static_cast<size_t>((result.m_total_samples + tgt_level.samples_per_bin - 1) / tgt_level.samples_per_bin);
        tgt_level.bins_left.resize(num_tgt_bins);
        bool is_stereo = !src_level.bins_right.empty();
        if (is_stereo) {
            tgt_level.bins_right.resize(num_tgt_bins);
        }

        double bin_sec = static_cast<double>(tgt_level.samples_per_bin) / m_sample_rate;

        for (size_t b = 0; b < num_tgt_bins; ++b) {
            double tgt_t = (static_cast<double>(b) + 0.5) * bin_sec;
            double src_t = warp_map.map_inverse(tgt_t);
            int64_t src_sample = static_cast<int64_t>(std::llround(src_t * m_sample_rate));
            size_t src_bin = static_cast<size_t>(src_sample / src_level.samples_per_bin);

            if (src_bin >= src_level.bins_left.size()) {
                src_bin = src_level.bins_left.empty() ? 0 : (src_level.bins_left.size() - 1);
            }

            if (!src_level.bins_left.empty()) {
                tgt_level.bins_left[b] = src_level.bins_left[src_bin];
                if (is_stereo && !src_level.bins_right.empty()) {
                    tgt_level.bins_right[b] = src_level.bins_right[src_bin];
                }
            }
        }

        result.m_levels.push_back(std::move(tgt_level));
    }

    return result;
}

} // namespace bf::dsp
