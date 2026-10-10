#include "project_json.h"
#include "../../third_party/nlohmann/json.hpp"
#include <windows.h>
#include <fstream>
#include <sstream>

namespace bf::engine {

using json = nlohmann::json;

std::string serialize_project_json(const ProjectState& state) {
    json j;
    j["format"] = "beatfix.project";
    j["schemaVersion"] = state.schema_version;
    j["algoVersion"] = state.algo_version;

    j["source"] = {
        { "path", state.source_path },
        { "fileHash", state.file_hash },
        { "pcmHash", state.pcm_hash },
        { "sampleRate", state.sample_rate },
        { "channels", state.channels },
        { "totalFrames", state.total_frames }
    };

    j["target"] = {
        { "bpm", state.target_bpm },
        { "beatsPerBar", state.beats_per_bar },
        { "detent", static_cast<int>(state.detent) },
        { "stretchMode", state.stretch_mode }
    };

    json j_beats = json::array();
    for (const auto& b : state.analysis.beats) {
        j_beats.push_back({
            { "timeSec", b.time_sec },
            { "confidence", b.confidence },
            { "isDownbeat", b.is_downbeat },
            { "inferred", b.inferred },
            { "barIndex", b.bar_index },
            { "beatInBar", b.beat_in_bar }
        });
    }

    std::string conf_str = "High";
    if (state.analysis.global_confidence == analysis::GlobalConfidence::Medium) conf_str = "Medium";
    if (state.analysis.global_confidence == analysis::GlobalConfidence::Low) conf_str = "Low";
    if (state.analysis.global_confidence == analysis::GlobalConfidence::None) conf_str = "None";

    std::string stab_str = "Stable";
    if (state.analysis.stability_class == analysis::StabilityClass::Drifting) stab_str = "Drifting";
    if (state.analysis.stability_class == analysis::StabilityClass::Stepped) stab_str = "Stepped";
    if (state.analysis.stability_class == analysis::StabilityClass::Spliced) stab_str = "Spliced";
    if (state.analysis.stability_class == analysis::StabilityClass::Unreliable) stab_str = "Unreliable";

    j["analysis"] = {
        { "bpmGlobal", state.analysis.bpm_global },
        { "bpmRangeLow", state.analysis.bpm_range_low },
        { "bpmRangeHigh", state.analysis.bpm_range_high },
        { "globalConfidence", conf_str },
        { "confidenceReason", state.analysis.confidence_reason },
        { "stabilityClass", stab_str },
        { "analyzerId", state.analysis.analyzer_id },
        { "beats", j_beats }
    };

    return j.dump(2);
}

Result<ProjectState> deserialize_project_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);

        ProjectState state;
        state.schema_version = j.value("schemaVersion", 1);
        state.algo_version = j.value("algoVersion", "beatfix-1.0.0");

        if (j.contains("source")) {
            const auto& js = j["source"];
            state.source_path = js.value("path", "");
            state.file_hash = js.value("fileHash", "");
            state.pcm_hash = js.value("pcmHash", "");
            state.sample_rate = js.value("sampleRate", 44100.0);
            state.channels = js.value("channels", 2);
            state.total_frames = js.value("totalFrames", 0LL);
        }

        if (j.contains("target")) {
            const auto& jt = j["target"];
            state.target_bpm = jt.value("bpm", 120.0);
            state.beats_per_bar = jt.value("beatsPerBar", 4);
            int det_int = jt.value("detent", 8);
            state.detent = static_cast<dsp::WarpDetent>(det_int);
            state.stretch_mode = jt.value("stretchMode", "smooth");
        }

        if (j.contains("analysis")) {
            const auto& ja = j["analysis"];
            state.analysis.bpm_global = ja.value("bpmGlobal", 120.0);
            state.analysis.bpm_range_low = ja.value("bpmRangeLow", 118.0);
            state.analysis.bpm_range_high = ja.value("bpmRangeHigh", 122.0);
            state.analysis.confidence_reason = ja.value("confidenceReason", "");
            state.analysis.analyzer_id = ja.value("analyzerId", "dsp_tracker");

            std::string c_str = ja.value("globalConfidence", "High");
            if (c_str == "High") state.analysis.global_confidence = analysis::GlobalConfidence::High;
            else if (c_str == "Medium") state.analysis.global_confidence = analysis::GlobalConfidence::Medium;
            else if (c_str == "Low") state.analysis.global_confidence = analysis::GlobalConfidence::Low;
            else state.analysis.global_confidence = analysis::GlobalConfidence::None;

            std::string s_str = ja.value("stabilityClass", "Stable");
            if (s_str == "Stable") state.analysis.stability_class = analysis::StabilityClass::Stable;
            else if (s_str == "Drifting") state.analysis.stability_class = analysis::StabilityClass::Drifting;
            else if (s_str == "Stepped") state.analysis.stability_class = analysis::StabilityClass::Stepped;
            else if (s_str == "Spliced") state.analysis.stability_class = analysis::StabilityClass::Spliced;
            else state.analysis.stability_class = analysis::StabilityClass::Unreliable;

            if (ja.contains("beats") && ja["beats"].is_array()) {
                for (const auto& jb : ja["beats"]) {
                    analysis::Beat b;
                    b.time_sec = jb.value("timeSec", 0.0);
                    b.confidence = jb.value("confidence", 1.0);
                    b.is_downbeat = jb.value("isDownbeat", false);
                    b.inferred = jb.value("inferred", false);
                    b.bar_index = jb.value("barIndex", 0);
                    b.beat_in_bar = jb.value("beatInBar", 0);
                    state.analysis.beats.push_back(b);
                }
            }
        }

        return Result<ProjectState>::ok(std::move(state));
    } catch (const std::exception& ex) {
        return Result<ProjectState>::error("JSON parse error: " + std::string(ex.what()));
    }
}

Result<void> save_project_file(const std::string& file_path, const ProjectState& state) {
    std::string temp_path = file_path + ".tmp." + std::to_string(GetCurrentProcessId());

    std::string json_data = serialize_project_json(state);
    {
        std::ofstream out(temp_path, std::ios::trunc);
        if (!out) {
            return Result<void>::error("Failed to write temporary project file: " + temp_path);
        }
        out << json_data;
    }

    std::wstring w_temp(temp_path.begin(), temp_path.end());
    std::wstring w_target(file_path.begin(), file_path.end());

    BOOL ok = MoveFileExW(w_temp.c_str(), w_target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        DeleteFileW(w_temp.c_str());
        return Result<void>::error("Atomic project save failed: Windows error " + std::to_string(GetLastError()));
    }

    return Result<void>::ok();
}

Result<ProjectState> load_project_file(const std::string& file_path) {
    std::ifstream in(file_path);
    if (!in) {
        return Result<ProjectState>::error("Failed to open project file: " + file_path);
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return deserialize_project_json(buffer.str());
}

} // namespace bf::engine
