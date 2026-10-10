#include "render_plan.h"
#include <algorithm>
#include <cmath>

namespace bf::dsp {

RenderPlan build_render_plan(
    const WarpMap& warp_map,
    int64_t total_source_samples,
    double sample_rate,
    int beats_per_chunk
) {
    RenderPlan plan;
    plan.sample_rate = sample_rate;

    double total_src_sec = static_cast<double>(total_source_samples) / sample_rate;
    double total_tgt_sec = warp_map.map(total_src_sec);
    plan.total_out_samples = static_cast<int64_t>(std::llround(total_tgt_sec * sample_rate));

    const auto& anchors = warp_map.anchors();
    if (anchors.size() < 2) {
        // Single chunk fallback
        RenderChunk chunk;
        chunk.chunk_id = 0;
        chunk.tgt_start_sec = 0.0;
        chunk.tgt_end_sec = total_tgt_sec;
        chunk.out_start_sample = 0;
        chunk.out_end_sample = plan.total_out_samples;
        plan.chunks.push_back(chunk);
        return plan;
    }

    const size_t num_anchors = anchors.size();
    size_t chunk_step = static_cast<size_t>(std::max(4, beats_per_chunk));

    size_t cur_anchor = 0;
    size_t chunk_id = 0;

    while (cur_anchor < num_anchors - 1) {
        size_t next_anchor = std::min(cur_anchor + chunk_step, num_anchors - 1);

        RenderChunk chunk;
        chunk.chunk_id = chunk_id++;
        chunk.start_anchor_idx = cur_anchor;
        chunk.end_anchor_idx = next_anchor;

        if (cur_anchor == 0) {
            chunk.tgt_start_sec = 0.0;
            chunk.out_start_sample = 0;
        } else {
            chunk.tgt_start_sec = anchors[cur_anchor].tgt_sec;
            chunk.out_start_sample = static_cast<int64_t>(std::llround(chunk.tgt_start_sec * sample_rate));
        }

        if (next_anchor == num_anchors - 1) {
            chunk.tgt_end_sec = total_tgt_sec;
            chunk.out_end_sample = plan.total_out_samples;
        } else {
            chunk.tgt_end_sec = anchors[next_anchor].tgt_sec;
            chunk.out_end_sample = static_cast<int64_t>(std::llround(chunk.tgt_end_sec * sample_rate));
        }

        plan.chunks.push_back(chunk);
        cur_anchor = next_anchor;
    }

    return plan;
}

} // namespace bf::dsp
