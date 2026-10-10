#pragma once

#include "pcm_cache.h"
#include "../bf_dsp/loudness.h"
#include <string>

namespace bf::io {

struct ExportSettings {
    std::string output_path;
    std::string format = "wav"; // "wav", "flac", "mp3"
    int bit_depth = 24;         // 16, 24, 32
    int mp3_bitrate_kbps = 320;
    bool apply_tpdf_dither = true;
    bool lower_gain_if_peaking = true; // Peak gate default per DECISION_SUMMARY §6
};

struct ExportReport {
    std::string output_path;
    int64_t total_frames = 0;
    double duration_sec = 0.0;
    double sample_peak_dbfs = 0.0;
    double true_peak_dbtp = 0.0;
    double applied_gain_reduction_db = 0.0;
    bool peak_gate_triggered = false;
    bool re_decode_verified = false;
};

// Safe export with peak gate, TPDF dither, atomic finalize and re-decode validation
Result<ExportReport> export_audio(
    const AudioData& audio,
    const ExportSettings& settings
);

} // namespace bf::io
