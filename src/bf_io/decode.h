#pragma once

#include "pcm_cache.h"
#include <string>

namespace bf::io {

struct AudioProbeInfo {
    std::string format;
    int channels = 0;
    double sample_rate = 0.0;
    int bit_depth = 0;
    double duration_sec = 0.0;
    int64_t total_frames = 0;
    bool is_supported = false;
    std::string error_message;
};

// Fast probe without full decode (REQ-IMP-002)
Result<AudioProbeInfo> probe_file(const std::string& file_path);

// Safe gapless decode into immutable float32 PCM (REQ-IMP-001, REQ-IMP-005)
Result<AudioData> decode_file(const std::string& file_path);

} // namespace bf::io
