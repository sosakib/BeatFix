#pragma once

#include "result.h"
#include "warp.h"
#include <memory>
#include <span>
#include <string>

namespace bf::dsp {

enum class StretchEngineMode {
    Smooth,   // Signalsmith Stretch (pitch-preserving, default)
    Resample  // Varispeed constant-ratio resampling
};

class ITimeStretcher {
public:
    virtual ~ITimeStretcher() = default;

    virtual Result<void> prepare(int num_channels, double sample_rate) = 0;
    virtual void reset() = 0;
    [[nodiscard]] virtual int latency_samples() const = 0;
    [[nodiscard]] virtual std::string id() const = 0;

    // Renders output samples for target time span [tgt_start_sec, tgt_end_sec]
    // Samples are read from source buffers via absolute W^{-1} coordinate mapping
    virtual Result<void> render_span(
        std::span<const float> source_left,
        std::span<const float> source_right,
        std::span<float> out_left,
        std::span<float> out_right,
        const WarpMap& warp_map,
        double tgt_start_sec,
        double tgt_end_sec
    ) = 0;
};

// Factory functions
std::unique_ptr<ITimeStretcher> create_signalsmith_stretcher();
std::unique_ptr<ITimeStretcher> create_resample_stretcher();

} // namespace bf::dsp
