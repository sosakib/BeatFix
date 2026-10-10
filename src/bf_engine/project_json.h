#pragma once

#include "../bf_dsp/result.h"
#include "../bf_dsp/units.h"
#include "../bf_analysis/analyzer.h"
#include <string>

namespace bf::engine {

struct ProjectState {
    int schema_version = 1;
    std::string algo_version = "beatfix-1.0.0";
    std::string source_path;
    std::string file_hash;
    std::string pcm_hash;
    double sample_rate = 44100.0;
    int channels = 2;
    int64_t total_frames = 0;

    // Analysis
    analysis::AnalysisResult analysis;

    // User settings
    double target_bpm = 120.0;
    dsp::WarpDetent detent = dsp::WarpDetent::Natural8;
    std::string stretch_mode = "smooth"; // "smooth" or "resample"
    int beats_per_bar = 4;
};

// Serializes ProjectState to JSON string
std::string serialize_project_json(const ProjectState& state);

// Deserializes ProjectState from JSON string
Result<ProjectState> deserialize_project_json(const std::string& json_str);

// Atomic project save (temp file + ReplaceFileW)
Result<void> save_project_file(const std::string& file_path, const ProjectState& state);

// Load project from file
Result<ProjectState> load_project_file(const std::string& file_path);

} // namespace bf::engine
