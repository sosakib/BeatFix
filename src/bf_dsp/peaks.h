#pragma once

#include <vector>
#include <span>
#include <cstdint>

namespace bf::dsp {

class WarpMap;

struct PeakBin {
    float min_val = 0.0f;
    float max_val = 0.0f;
    float rms_val = 0.0f;
};

struct PeakLevel {
    int samples_per_bin = 256;
    std::vector<PeakBin> bins_left;
    std::vector<PeakBin> bins_right;
};

class PeakPyramid {
public:
    PeakPyramid() = default;

    // Builds multi-resolution peak pyramid from audio buffers
    // rates: 64, 256, 1024, 4096, 16384
    void build_from_audio(
        std::span<const float> channel_left,
        std::span<const float> channel_right,
        double sample_rate
    );

    // Creates an instant estimated corrected pyramid by mapping time through W
    [[nodiscard]] PeakPyramid map_through_warp(const WarpMap& warp_map) const;

    [[nodiscard]] const std::vector<PeakLevel>& levels() const noexcept { return m_levels; }
    [[nodiscard]] double sample_rate() const noexcept { return m_sample_rate; }
    [[nodiscard]] int64_t total_samples() const noexcept { return m_total_samples; }
    [[nodiscard]] bool empty() const noexcept { return m_levels.empty(); }

private:
    std::vector<PeakLevel> m_levels;
    double m_sample_rate = 44100.0;
    int64_t m_total_samples = 0;
};

} // namespace bf::dsp
