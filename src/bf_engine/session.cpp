#include "session.h"
#include <cmath>
#include <algorithm>

namespace bf::engine {

Session::Session() {
    m_analyzer = analysis::create_dsp_beat_analyzer();
}

Result<void> Session::open_audio_file(const std::string& file_path) {
    auto probe_res = io::probe_file(file_path);
    if (!probe_res.is_ok()) {
        return Result<void>::error("Failed to probe file: " + probe_res.error_message());
    }

    auto decode_res = io::decode_file(file_path);
    if (!decode_res.is_ok()) {
        return Result<void>::error("Failed to decode file: " + decode_res.error_message());
    }

    m_source_audio = decode_res.take_value();
    m_corrected_audio = io::AudioData{};

    std::span<const float> l_span(m_source_audio.left.data(), m_source_audio.left.size());
    std::span<const float> r_span;
    if (m_source_audio.channels == 2 && !m_source_audio.right.empty()) {
        r_span = std::span<const float>(m_source_audio.right.data(), m_source_audio.right.size());
    }

    m_source_pyramid.build_from_audio(l_span, r_span, m_source_audio.sample_rate);
    m_corrected_pyramid = dsp::PeakPyramid{};
    m_warp_map = dsp::WarpMap{};
    m_analyzed = false;
    m_undo_stack.clear();
    m_redo_stack.clear();

    return Result<void>::ok();
}

Result<void> Session::run_analysis(std::stop_token stop_tok) {
    if (m_source_audio.is_empty()) {
        return Result<void>::error("No audio file loaded for analysis");
    }

    if (!m_analyzer) {
        m_analyzer = analysis::create_dsp_beat_analyzer();
    }

    auto analysis_res = m_analyzer->analyze(m_source_audio, stop_tok);
    if (!analysis_res.is_ok()) {
        return Result<void>::error("Analysis failed: " + analysis_res.error_message());
    }

    m_analysis = analysis_res.take_value();
    m_target_bpm = std::round(m_analysis.bpm_global);
    m_analyzed = true;

    return rebuild_warp_map();
}

Result<void> Session::set_target_bpm(double bpm) {
    if (bpm < 30.0 || bpm > 350.0) {
        return Result<void>::error("Target BPM must be between 30 and 350");
    }

    push_undo();
    m_target_bpm = bpm;
    return rebuild_warp_map();
}

Result<void> Session::set_detent(dsp::WarpDetent detent) {
    push_undo();
    m_detent = detent;
    return rebuild_warp_map();
}

Result<void> Session::set_mode(dsp::StretchEngineMode mode) {
    push_undo();
    m_mode = mode;
    // Invalidate rendered corrected audio as mode changed
    m_corrected_audio = io::AudioData{};
    return Result<void>::ok();
}

Result<void> Session::rebuild_warp_map() {
    if (!m_analyzed || m_analysis.beats.empty()) {
        return Result<void>::error("Cannot build warp map: audio not analyzed or no beats detected");
    }

    std::vector<double> detected_beats;
    std::vector<double> confidences;
    detected_beats.reserve(m_analysis.beats.size());
    confidences.reserve(m_analysis.beats.size());

    size_t first_downbeat_idx = 0;
    bool found_downbeat = false;

    for (size_t i = 0; i < m_analysis.beats.size(); ++i) {
        const auto& b = m_analysis.beats[i];
        detected_beats.push_back(b.time_sec);
        confidences.push_back(b.confidence);
        if (!found_downbeat && b.is_downbeat) {
            first_downbeat_idx = i;
            found_downbeat = true;
        }
    }

    dsp::WarpBuildConfig config;
    config.detected_bpm = m_analysis.bpm_global;
    config.target_bpm = m_target_bpm;
    config.detent = m_detent;
    config.phase_policy = dsp::PhasePolicy::PreserveStart;
    config.beats_per_bar = 4;
    config.first_downbeat_index = first_downbeat_idx;

    auto warp_res = dsp::build_warp_map(
        detected_beats,
        confidences,
        m_analysis.change_point_breaks,
        config
    );

    if (!warp_res.is_ok()) {
        return Result<void>::error("Failed to build warp map: " + warp_res.error_message());
    }

    m_warp_map = warp_res.take_value();

    // Instant estimated peak pyramid mapped through warp
    if (!m_source_pyramid.empty()) {
        m_corrected_pyramid = m_source_pyramid.map_through_warp(m_warp_map);
    }

    // Invalidate previously rendered audio
    m_corrected_audio = io::AudioData{};

    return Result<void>::ok();
}

Result<void> Session::render_corrected(ProgressCallback progress_cb, std::stop_token stop_tok) {
    if (m_source_audio.is_empty()) {
        return Result<void>::error("No source audio loaded to render");
    }

    if (m_warp_map.anchors().empty()) {
        auto reb_res = rebuild_warp_map();
        if (!reb_res.is_ok()) {
            return Result<void>::error("Failed preparing warp map for render: " + reb_res.error_message());
        }
    }

    auto render_res = m_render_scheduler.render_full(
        m_source_audio,
        m_warp_map,
        m_mode,
        progress_cb,
        stop_tok
    );

    if (!render_res.is_ok()) {
        return Result<void>::error("Render failed: " + render_res.error_message());
    }

    m_corrected_audio = render_res.take_value();

    // Rebuild exact corrected peak pyramid from real rendered audio
    std::span<const float> l_span(m_corrected_audio.left.data(), m_corrected_audio.left.size());
    std::span<const float> r_span;
    if (m_corrected_audio.channels == 2 && !m_corrected_audio.right.empty()) {
        r_span = std::span<const float>(m_corrected_audio.right.data(), m_corrected_audio.right.size());
    }

    m_corrected_pyramid.build_from_audio(l_span, r_span, m_corrected_audio.sample_rate);

    return Result<void>::ok();
}

Result<io::ExportReport> Session::export_corrected(const io::ExportSettings& settings) {
    if (m_corrected_audio.is_empty()) {
        auto rend_res = render_corrected();
        if (!rend_res.is_ok()) {
            return Result<io::ExportReport>::error("Cannot export: render failed: " + rend_res.error_message());
        }
    }

    return io::export_audio(m_corrected_audio, settings);
}

Result<void> Session::save_project(const std::string& file_path) {
    if (m_source_audio.is_empty()) {
        return Result<void>::error("Cannot save project: no audio loaded");
    }

    ProjectState state;
    state.schema_version = 1;
    state.algo_version = "beatfix-1.0.0";
    state.source_path = m_source_audio.source_path;
    state.file_hash = m_source_audio.file_hash;
    state.pcm_hash = m_source_audio.pcm_hash;
    state.sample_rate = m_source_audio.sample_rate;
    state.channels = m_source_audio.channels;
    state.total_frames = m_source_audio.total_frames;

    state.analysis = m_analysis;
    state.target_bpm = m_target_bpm;
    state.detent = m_detent;
    state.stretch_mode = (m_mode == dsp::StretchEngineMode::Resample ? "resample" : "smooth");
    state.beats_per_bar = 4;

    return save_project_file(file_path, state);
}

Result<void> Session::load_project(const std::string& file_path) {
    auto load_res = load_project_file(file_path);
    if (!load_res.is_ok()) {
        return Result<void>::error("Failed to load project: " + load_res.error_message());
    }

    const auto& state = load_res.value();
    auto open_res = open_audio_file(state.source_path);
    if (!open_res.is_ok()) {
        return Result<void>::error("Failed to open source file referenced in project: " + open_res.error_message());
    }

    m_analysis = state.analysis;
    m_target_bpm = state.target_bpm;
    m_detent = state.detent;
    m_mode = (state.stretch_mode == "resample" ? dsp::StretchEngineMode::Resample : dsp::StretchEngineMode::Smooth);
    m_analyzed = true;

    return rebuild_warp_map();
}

void Session::push_undo() {
    EditSnapshot snap;
    snap.target_bpm = m_target_bpm;
    snap.detent = m_detent;
    snap.mode = m_mode;
    m_undo_stack.push_back(snap);
    m_redo_stack.clear();
}

Result<void> Session::undo() {
    if (m_undo_stack.empty()) {
        return Result<void>::error("Nothing to undo");
    }

    EditSnapshot current;
    current.target_bpm = m_target_bpm;
    current.detent = m_detent;
    current.mode = m_mode;
    m_redo_stack.push_back(current);

    auto snap = m_undo_stack.back();
    m_undo_stack.pop_back();

    m_target_bpm = snap.target_bpm;
    m_detent = snap.detent;
    m_mode = snap.mode;

    return rebuild_warp_map();
}

Result<void> Session::redo() {
    if (m_redo_stack.empty()) {
        return Result<void>::error("Nothing to redo");
    }

    EditSnapshot current;
    current.target_bpm = m_target_bpm;
    current.detent = m_detent;
    current.mode = m_mode;
    m_undo_stack.push_back(current);

    auto snap = m_redo_stack.back();
    m_redo_stack.pop_back();

    m_target_bpm = snap.target_bpm;
    m_detent = snap.detent;
    m_mode = snap.mode;

    return rebuild_warp_map();
}

} // namespace bf::engine
