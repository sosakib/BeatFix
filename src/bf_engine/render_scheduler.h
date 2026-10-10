#pragma once

#include "../bf_dsp/result.h"
#include "../bf_dsp/warp.h"
#include "../bf_dsp/stretch.h"
#include "../bf_io/pcm_cache.h"
#include <memory>
#include <stop_token>
#include <functional>

namespace bf::engine {

struct RenderProgress {
    size_t completed_chunks = 0;
    size_t total_chunks = 0;
    double progress_fraction = 0.0;
};

using ProgressCallback = std::function<void(const RenderProgress&)>;

class RenderScheduler {
public:
    RenderScheduler() = default;

    // Renders full corrected audio from source audio and warp map
    Result<io::AudioData> render_full(
        const io::AudioData& source_audio,
        const dsp::WarpMap& warp_map,
        dsp::StretchEngineMode mode = dsp::StretchEngineMode::Smooth,
        ProgressCallback progress_cb = nullptr,
        std::stop_token stop_tok = {}
    );
};

} // namespace bf::engine
