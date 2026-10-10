#pragma once

#include "../bf_dsp/result.h"
#include "../bf_io/pcm_cache.h"
#include <vector>
#include <string>
#include <memory>
#include <stop_token>

namespace bf::analysis {

struct Beat {
    double time_sec = 0.0;
    double confidence = 1.0;
    bool is_downbeat = false;
    bool inferred = false;
    int bar_index = 0;
    int beat_in_bar = 0;
};

enum class StabilityClass {
    Stable,
    Drifting,
    Stepped,
    Spliced,
    Unreliable
};

enum class GlobalConfidence {
    High,
    Medium,
    Low,
    None
};

struct AnalysisResult {
    std::vector<Beat> beats;
    double bpm_global = 120.0;
    double bpm_range_low = 118.0;
    double bpm_range_high = 122.0;
    GlobalConfidence global_confidence = GlobalConfidence::High;
    std::string confidence_reason = "Consistent periodic beat evidence";
    StabilityClass stability_class = StabilityClass::Stable;
    std::vector<size_t> change_points; // Beat indices of detected change points
    std::vector<double> change_point_breaks; // g_i (0 at change points, 1 elsewhere, size = beats.size() - 2)
    std::string analyzer_id = "dsp_tracker";
    std::string model_hash;
};

class IBeatAnalyzer {
public:
    virtual ~IBeatAnalyzer() = default;

    virtual Result<AnalysisResult> analyze(
        const io::AudioData& audio,
        std::stop_token stop_tok = {}
    ) = 0;

    [[nodiscard]] virtual std::string id() const = 0;
};

// Factory functions
std::unique_ptr<IBeatAnalyzer> create_dsp_beat_analyzer();
std::unique_ptr<IBeatAnalyzer> create_beatthis_onnx_analyzer(const std::string& model_path);

} // namespace bf::analysis
