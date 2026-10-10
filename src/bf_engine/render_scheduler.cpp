#include "render_scheduler.h"
#include "../bf_dsp/render_plan.h"
#include <algorithm>
#include <cmath>

namespace bf::engine {

Result<io::AudioData> RenderScheduler::render_full(
    const io::AudioData& source_audio,
    const dsp::WarpMap& warp_map,
    dsp::StretchEngineMode mode,
    ProgressCallback progress_cb,
    std::stop_token stop_tok
) {
    if (source_audio.is_empty()) {
        return Result<io::AudioData>::error("Source audio buffer is empty");
    }

    auto valid_res = warp_map.validate();
    if (!valid_res.is_ok()) {
        return Result<io::AudioData>::error("Invalid warp map for render: " + valid_res.error_message());
    }

    if (stop_tok.stop_requested()) {
        return Result<io::AudioData>::error("Render cancelled by user");
    }

    auto plan = dsp::build_render_plan(
        warp_map,
        source_audio.total_frames,
        source_audio.sample_rate
    );

    if (plan.total_out_samples <= 0) {
        return Result<io::AudioData>::error("Render plan produced 0 output samples");
    }

    std::unique_ptr<dsp::ITimeStretcher> stretcher;
    if (mode == dsp::StretchEngineMode::Resample) {
        stretcher = dsp::create_resample_stretcher();
    } else {
        stretcher = dsp::create_signalsmith_stretcher();
    }

    auto prep_res = stretcher->prepare(source_audio.channels, source_audio.sample_rate);
    if (!prep_res.is_ok()) {
        return Result<io::AudioData>::error("Failed to prepare stretcher: " + prep_res.error_message());
    }

    io::AudioData corrected;
    corrected.source_path = source_audio.source_path;
    corrected.channels = source_audio.channels;
    corrected.sample_rate = source_audio.sample_rate;
    corrected.total_frames = plan.total_out_samples;
    corrected.left.assign(static_cast<size_t>(plan.total_out_samples), 0.0f);
    if (corrected.channels == 2) {
        corrected.right.assign(static_cast<size_t>(plan.total_out_samples), 0.0f);
    }

    const size_t total_chunks = plan.chunks.size();

    for (size_t i = 0; i < total_chunks; ++i) {
        if (stop_tok.stop_requested()) {
            return Result<io::AudioData>::error("Render cancelled by user");
        }

        const auto& chunk = plan.chunks[i];
        int64_t start_s = std::clamp<int64_t>(chunk.out_start_sample, 0, plan.total_out_samples);
        int64_t end_s = std::clamp<int64_t>(chunk.out_end_sample, start_s, plan.total_out_samples);
        size_t chunk_len = static_cast<size_t>(end_s - start_s);

        if (chunk_len > 0) {
            std::span<float> chunk_out_l(corrected.left.data() + start_s, chunk_len);
            std::span<float> chunk_out_r;
            if (corrected.channels == 2) {
                chunk_out_r = std::span<float>(corrected.right.data() + start_s, chunk_len);
            }

            auto render_res = stretcher->render_span(
                source_audio.left,
                source_audio.right,
                chunk_out_l,
                chunk_out_r,
                warp_map,
                chunk.tgt_start_sec,
                chunk.tgt_end_sec
            );

            if (!render_res.is_ok()) {
                return Result<io::AudioData>::error("Failed rendering chunk " + std::to_string(i) + ": " + render_res.error_message());
            }
        }

        if (progress_cb) {
            RenderProgress prog;
            prog.completed_chunks = i + 1;
            prog.total_chunks = total_chunks;
            prog.progress_fraction = static_cast<double>(i + 1) / static_cast<double>(total_chunks);
            progress_cb(prog);
        }
    }

    // Validation: verify no NaN or Inf in rendered output
    for (float s : corrected.left) {
        if (std::isnan(s) || std::isinf(s)) {
            return Result<io::AudioData>::error("Render validation failed: NaN/Inf sample generated");
        }
    }

    corrected.pcm_hash = io::compute_pcm_sha256(corrected.left, corrected.right, corrected.sample_rate);
    return Result<io::AudioData>::ok(std::move(corrected));
}

} // namespace bf::engine
