#pragma once

#include "../bf_dsp/result.h"
#include <vector>
#include <string>
#include <cstdint>
#include <span>

namespace bf::io {

struct AudioData {
    int channels = 2;
    double sample_rate = 44100.0;
    int64_t total_frames = 0;
    std::vector<float> left;
    std::vector<float> right;
    std::string file_hash;
    std::string pcm_hash;
    std::string source_path;

    [[nodiscard]] double duration_sec() const noexcept {
        return (sample_rate > 0.0) ? (static_cast<double>(total_frames) / sample_rate) : 0.0;
    }

    [[nodiscard]] bool is_empty() const noexcept {
        return total_frames == 0 || left.empty();
    }
};

// Computes SHA-256 hash of a file on disk
Result<std::string> compute_file_sha256(const std::string& file_path);

// Computes SHA-256 hash over PCM samples
std::string compute_pcm_sha256(
    std::span<const float> left,
    std::span<const float> right,
    double sample_rate
);

} // namespace bf::io
