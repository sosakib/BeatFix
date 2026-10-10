#pragma once

#include "warp.h"
#include <vector>
#include <cstdint>

namespace bf::dsp {

struct RenderChunk {
    size_t chunk_id = 0;
    size_t start_anchor_idx = 0;
    size_t end_anchor_idx = 0;
    double tgt_start_sec = 0.0;
    double tgt_end_sec = 0.0;
    int64_t out_start_sample = 0;
    int64_t out_end_sample = 0;
    double pre_roll_sec = 0.050; // 50 ms pre-roll
    double crossfade_sec = 0.020; // 20 ms crossfade
};

struct RenderPlan {
    double sample_rate = 44100.0;
    int64_t total_out_samples = 0;
    std::vector<RenderChunk> chunks;
};

// Builds deterministic chunk plan
RenderPlan build_render_plan(
    const WarpMap& warp_map,
    int64_t total_source_samples,
    double sample_rate,
    int beats_per_chunk = 16
);

} // namespace bf::dsp
