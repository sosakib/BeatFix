#define DR_WAV_IMPLEMENTATION
#include "../../third_party/dr_libs/dr_wav.h"

#define DR_FLAC_IMPLEMENTATION
#include "../../third_party/dr_libs/dr_flac.h"

#define MINIMP3_IMPLEMENTATION
#include "../../third_party/minimp3/minimp3_ex.h"

#include "decode.h"
#include <algorithm>
#include <filesystem>
#include <cmath>

namespace bf::io {

namespace {

std::string get_extension_lower(const std::string& path) {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

} // namespace

Result<AudioProbeInfo> probe_file(const std::string& file_path) {
    if (!std::filesystem::exists(file_path)) {
        return Result<AudioProbeInfo>::error("File does not exist: " + file_path);
    }

    std::string ext = get_extension_lower(file_path);

    // Reject unsupported formats explicitly per REQ-IMP-003
    if (ext == ".m4a" || ext == ".aac" || ext == ".mp4" || ext == ".opus") {
        AudioProbeInfo info;
        info.format = ext;
        info.is_supported = false;
        info.error_message = "Unsupported format: " + ext + ". Please convert losslessly to WAV or FLAC.";
        return Result<AudioProbeInfo>::error(info.error_message);
    }

    AudioProbeInfo info;
    info.format = ext;

    if (ext == ".wav" || ext == ".wave" || ext == ".aif" || ext == ".aiff") {
        drwav wav;
        if (!drwav_init_file(&wav, file_path.c_str(), nullptr)) {
            return Result<AudioProbeInfo>::error("Failed to parse WAV/AIFF header: " + file_path);
        }
        info.channels = static_cast<int>(wav.channels);
        info.sample_rate = static_cast<double>(wav.sampleRate);
        info.bit_depth = static_cast<int>(wav.bitsPerSample);
        info.total_frames = static_cast<int64_t>(wav.totalPCMFrameCount);
        info.duration_sec = (info.sample_rate > 0) ? (static_cast<double>(info.total_frames) / info.sample_rate) : 0.0;
        drwav_uninit(&wav);
    } else if (ext == ".flac") {
        drflac* flac = drflac_open_file(file_path.c_str(), nullptr);
        if (!flac) {
            return Result<AudioProbeInfo>::error("Failed to parse FLAC header: " + file_path);
        }
        info.channels = static_cast<int>(flac->channels);
        info.sample_rate = static_cast<double>(flac->sampleRate);
        info.bit_depth = static_cast<int>(flac->bitsPerSample);
        info.total_frames = static_cast<int64_t>(flac->totalPCMFrameCount);
        info.duration_sec = (info.sample_rate > 0) ? (static_cast<double>(info.total_frames) / info.sample_rate) : 0.0;
        drflac_close(flac);
    } else if (ext == ".mp3") {
        mp3dec_ex_t dec;
        if (mp3dec_ex_open(&dec, file_path.c_str(), MP3D_SEEK_TO_SAMPLE)) {
            return Result<AudioProbeInfo>::error("Failed to parse MP3 file: " + file_path);
        }
        info.channels = dec.info.channels;
        info.sample_rate = static_cast<double>(dec.info.hz);
        info.bit_depth = 16;
        info.total_frames = static_cast<int64_t>(dec.samples / (dec.info.channels ? dec.info.channels : 1));
        info.duration_sec = (info.sample_rate > 0) ? (static_cast<double>(info.total_frames) / info.sample_rate) : 0.0;
        mp3dec_ex_close(&dec);
    } else {
        return Result<AudioProbeInfo>::error("Unsupported file extension: " + ext);
    }

    // Validate channel count per REQ-IMP-003 and REQ-IMP-004
    if (info.channels <= 0 || info.channels > 2) {
        info.is_supported = false;
        info.error_message = "Audio file has " + std::to_string(info.channels) + " channels. Only mono and stereo (1 or 2 channels) are supported.";
        return Result<AudioProbeInfo>::error(info.error_message);
    }

    info.is_supported = true;
    return Result<AudioProbeInfo>::ok(info);
}

Result<AudioData> decode_file(const std::string& file_path) {
    auto probe_res = probe_file(file_path);
    if (!probe_res.is_ok()) {
        return Result<AudioData>::error(probe_res.error_message());
    }

    const auto& probe = probe_res.value();
    std::string ext = get_extension_lower(file_path);

    AudioData audio;
    audio.source_path = file_path;
    audio.channels = probe.channels;
    audio.sample_rate = probe.sample_rate;
    audio.total_frames = probe.total_frames;

    if (ext == ".wav" || ext == ".wave" || ext == ".aif" || ext == ".aiff") {
        drwav wav;
        if (!drwav_init_file(&wav, file_path.c_str(), nullptr)) {
            return Result<AudioData>::error("Failed to open WAV file: " + file_path);
        }

        std::vector<float> interleaved(static_cast<size_t>(wav.totalPCMFrameCount * wav.channels));
        drwav_uint64 frames_read = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, interleaved.data());
        drwav_uninit(&wav);

        audio.total_frames = static_cast<int64_t>(frames_read);
        audio.left.resize(static_cast<size_t>(audio.total_frames));
        if (audio.channels == 2) {
            audio.right.resize(static_cast<size_t>(audio.total_frames));
            for (size_t i = 0; i < static_cast<size_t>(audio.total_frames); ++i) {
                audio.left[i] = interleaved[i * 2];
                audio.right[i] = interleaved[i * 2 + 1];
            }
        } else {
            for (size_t i = 0; i < static_cast<size_t>(audio.total_frames); ++i) {
                audio.left[i] = interleaved[i];
            }
        }
    } else if (ext == ".flac") {
        drflac* flac = drflac_open_file(file_path.c_str(), nullptr);
        if (!flac) {
            return Result<AudioData>::error("Failed to open FLAC file: " + file_path);
        }

        std::vector<float> interleaved(static_cast<size_t>(flac->totalPCMFrameCount * flac->channels));
        drflac_uint64 frames_read = drflac_read_pcm_frames_f32(flac, flac->totalPCMFrameCount, interleaved.data());
        drflac_close(flac);

        audio.total_frames = static_cast<int64_t>(frames_read);
        audio.left.resize(static_cast<size_t>(audio.total_frames));
        if (audio.channels == 2) {
            audio.right.resize(static_cast<size_t>(audio.total_frames));
            for (size_t i = 0; i < static_cast<size_t>(audio.total_frames); ++i) {
                audio.left[i] = interleaved[i * 2];
                audio.right[i] = interleaved[i * 2 + 1];
            }
        } else {
            for (size_t i = 0; i < static_cast<size_t>(audio.total_frames); ++i) {
                audio.left[i] = interleaved[i];
            }
        }
    } else if (ext == ".mp3") {
        mp3dec_ex_t dec;
        if (mp3dec_ex_open(&dec, file_path.c_str(), MP3D_SEEK_TO_SAMPLE)) {
            return Result<AudioData>::error("Failed to open MP3 file: " + file_path);
        }

        size_t total_samples = static_cast<size_t>(dec.samples);
        std::vector<mp3d_sample_t> sample_buf(total_samples);
        size_t samples_read = mp3dec_ex_read(&dec, sample_buf.data(), total_samples);
        mp3dec_ex_close(&dec);

        size_t frames = samples_read / static_cast<size_t>(audio.channels);
        audio.total_frames = static_cast<int64_t>(frames);
        audio.left.resize(frames);

        constexpr float inv_scale = 1.0f / 32768.0f;
        if (audio.channels == 2) {
            audio.right.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                audio.left[i] = static_cast<float>(sample_buf[i * 2]) * inv_scale;
                audio.right[i] = static_cast<float>(sample_buf[i * 2 + 1]) * inv_scale;
            }
        } else {
            for (size_t i = 0; i < frames; ++i) {
                audio.left[i] = static_cast<float>(sample_buf[i]) * inv_scale;
            }
        }
    }

    // Compute hashes for integrity and caching
    auto file_hash_res = compute_file_sha256(file_path);
    audio.file_hash = file_hash_res.is_ok() ? file_hash_res.value() : "";
    audio.pcm_hash = compute_pcm_sha256(audio.left, audio.right, audio.sample_rate);

    return Result<AudioData>::ok(std::move(audio));
}

} // namespace bf::io
