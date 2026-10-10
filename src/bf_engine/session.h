#pragma once

#include "../bf_dsp/result.h"
#include "../bf_dsp/warp.h"
#include "../bf_dsp/peaks.h"
#include "../bf_io/pcm_cache.h"
#include "../bf_io/decode.h"
#include "../bf_io/export.h"
#include "../bf_analysis/analyzer.h"
#include "project_json.h"
#include "render_scheduler.h"
#include <memory>
#include <string>
#include <vector>
#include <stop_token>

namespace bf::engine {

struct EditSnapshot {
    double target_bpm = 120.0;
    dsp::WarpDetent detent = dsp::WarpDetent::Natural8;
    dsp::StretchEngineMode mode = dsp::StretchEngineMode::Smooth;
};

class Session {
public:
    Session();
    ~Session() = default;

    // Load and decode audio file
    Result<void> open_audio_file(const std::string& file_path);

    // Run beat and tempo analysis
    Result<void> run_analysis(std::stop_token stop_tok = {});

    // Set target BPM and update warp map
    Result<void> set_target_bpm(double bpm);

    // Set Natural <-> Strict detent and update warp map
    Result<void> set_detent(dsp::WarpDetent detent);

    // Set stretch mode (Smooth or Resample)
    Result<void> set_mode(dsp::StretchEngineMode mode);

    // Rebuilds warp map from current analysis and settings
    Result<void> rebuild_warp_map();

    // Renders corrected audio
    Result<void> render_corrected(ProgressCallback progress_cb = nullptr, std::stop_token stop_tok = {});

    // Exports corrected audio with peak gate and validation
    Result<io::ExportReport> export_corrected(const io::ExportSettings& settings);

    // Project persistence
    Result<void> save_project(const std::string& file_path);
    Result<void> load_project(const std::string& file_path);

    // Undo / Redo
    bool can_undo() const noexcept { return !m_undo_stack.empty(); }
    bool can_redo() const noexcept { return !m_redo_stack.empty(); }
    Result<void> undo();
    Result<void> redo();

    // Accessors
    [[nodiscard]] const io::AudioData& source_audio() const noexcept { return m_source_audio; }
    [[nodiscard]] const io::AudioData& corrected_audio() const noexcept { return m_corrected_audio; }
    [[nodiscard]] const analysis::AnalysisResult& analysis() const noexcept { return m_analysis; }
    [[nodiscard]] const dsp::WarpMap& warp_map() const noexcept { return m_warp_map; }
    [[nodiscard]] const dsp::PeakPyramid& source_pyramid() const noexcept { return m_source_pyramid; }
    [[nodiscard]] const dsp::PeakPyramid& corrected_pyramid() const noexcept { return m_corrected_pyramid; }

    [[nodiscard]] double target_bpm() const noexcept { return m_target_bpm; }
    [[nodiscard]] dsp::WarpDetent detent() const noexcept { return m_detent; }
    [[nodiscard]] dsp::StretchEngineMode mode() const noexcept { return m_mode; }
    [[nodiscard]] bool is_analyzed() const noexcept { return m_analyzed; }
    [[nodiscard]] bool is_rendered() const noexcept { return !m_corrected_audio.is_empty(); }

private:
    void push_undo();

    io::AudioData m_source_audio;
    io::AudioData m_corrected_audio;
    analysis::AnalysisResult m_analysis;
    dsp::WarpMap m_warp_map;
    dsp::PeakPyramid m_source_pyramid;
    dsp::PeakPyramid m_corrected_pyramid;

    double m_target_bpm = 120.0;
    dsp::WarpDetent m_detent = dsp::WarpDetent::Natural8;
    dsp::StretchEngineMode m_mode = dsp::StretchEngineMode::Smooth;
    bool m_analyzed = false;

    std::unique_ptr<analysis::IBeatAnalyzer> m_analyzer;
    RenderScheduler m_render_scheduler;

    std::vector<EditSnapshot> m_undo_stack;
    std::vector<EditSnapshot> m_redo_stack;
};

} // namespace bf::engine
