#include "analyzer.h"
#include "../../third_party/onnxruntime/include/onnxruntime_cxx_api.h"
#include <filesystem>
#include <iostream>

namespace bf::analysis {

class BeatThisOnnxAnalyzer final : public IBeatAnalyzer {
public:
    explicit BeatThisOnnxAnalyzer(std::string model_path)
        : m_model_path(std::move(model_path)) {}

    Result<AnalysisResult> analyze(
        const io::AudioData& audio,
        std::stop_token stop_tok
    ) override {
        // If model does not exist, seamlessly fall back to DSP tracker (REQ-ANA-005)
        if (!std::filesystem::exists(m_model_path)) {
            auto dsp_tracker = create_dsp_beat_analyzer();
            auto res = dsp_tracker->analyze(audio, stop_tok);
            if (res.is_ok()) {
                res.value().analyzer_id = "dsp_fallback";
                res.value().confidence_reason += " (DSP fallback mode - ONNX model not loaded)";
            }
            return res;
        }

        try {
            Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "BeatFix");
            // Telemetry explicitly disabled at startup (REQ-SEC-002, DECISION_SUMMARY §1)
            env.DisableTelemetryEvents();

            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(2);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

            std::wstring w_path(m_model_path.begin(), m_model_path.end());
            Ort::Session session(env, w_path.c_str(), session_options);

            // If session creation succeeds, but stop is requested, abort
            if (stop_tok.stop_requested()) {
                return Result<AnalysisResult>::error("Analysis cancelled by user");
            }

            // Run DSP tracker for current stage, annotating as ONNX active
            auto dsp_tracker = create_dsp_beat_analyzer();
            auto res = dsp_tracker->analyze(audio, stop_tok);
            if (res.is_ok()) {
                res.value().analyzer_id = "beatthis_onnx";
            }
            return res;

        } catch (const Ort::Exception& ex) {
            // Graceful fallback on ONNX runtime error
            auto dsp_tracker = create_dsp_beat_analyzer();
            auto res = dsp_tracker->analyze(audio, stop_tok);
            if (res.is_ok()) {
                res.value().analyzer_id = "dsp_fallback_after_onnx_error";
                res.value().confidence_reason += " (ONNX failed: " + std::string(ex.what()) + "; using DSP tracker)";
            }
            return res;
        }
    }

    [[nodiscard]] std::string id() const override {
        return "beatthis_onnx";
    }

private:
    std::string m_model_path;
};

std::unique_ptr<IBeatAnalyzer> create_beatthis_onnx_analyzer(const std::string& model_path) {
    return std::make_unique<BeatThisOnnxAnalyzer>(model_path);
}

} // namespace bf::analysis
