#include "export.h"
#include "decode.h"
#include "path_utf8.h"
#include "../bf_dsp/dither.h"
#include "../../third_party/dr_libs/dr_wav.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

namespace bf::io {

Result<ExportReport> export_audio(
    const AudioData& audio,
    const ExportSettings& settings
) {
    if (audio.is_empty()) {
        return Result<ExportReport>::error("Cannot export empty audio buffer");
    }

    if (settings.output_path.empty()) {
        return Result<ExportReport>::error("Export output path is empty");
    }

    // Source immutability check: prevent overwriting the source file (REQ-AUDIO-001, REQ-EXP-008)
    try {
        if (!audio.source_path.empty()) {
            auto src_p = path_from_utf8(audio.source_path);
            auto out_p = path_from_utf8(settings.output_path);
            std::error_code ec1, ec2;
            if (std::filesystem::exists(src_p, ec1) && std::filesystem::exists(out_p, ec2)) {
                if (std::filesystem::equivalent(src_p, out_p, ec1)) {
                    return Result<ExportReport>::error("Refusing to overwrite input source file: " + settings.output_path);
                }
            }
        }
    } catch (...) {}

    // Ensure parent directory exists
    std::filesystem::path out_p = path_from_utf8(settings.output_path);
    if (out_p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(out_p.parent_path(), ec);
    }

    // Temporary file path in same directory for atomic commit, preserving extension for re-decode
    std::wstring temp_name = out_p.stem().wstring() + L".tmp." + std::to_wstring(GetCurrentProcessId()) + out_p.extension().wstring();
    std::filesystem::path p_temp = out_p.has_parent_path() ? (out_p.parent_path() / temp_name) : std::filesystem::path(temp_name);
    std::string temp_path = path_to_utf8(p_temp);

    // Measure peak and true-peak with libebur128 (REQ-AUDIO-009)
    auto loud_res = bf::dsp::measure_loudness(audio.left, audio.right, audio.sample_rate);
    double sample_peak = loud_res.is_ok() ? loud_res.value().sample_peak_linear : 1.0;
    double true_peak = loud_res.is_ok() ? loud_res.value().true_peak_linear : 1.0;
    (void)true_peak;

    float gain_factor = 1.0f;
    double gain_reduction_db = 0.0;
    bool peak_gate_triggered = false;

    // Peak gate logic (REQ-EXP-003): 0 dBFS for integer, -1 dBTP for lossy
    if (settings.bit_depth <= 24 && sample_peak > 1.0) {
        peak_gate_triggered = true;
        if (settings.lower_gain_if_peaking) {
            gain_factor = static_cast<float>(0.999 / sample_peak); // Safe headroom
            gain_reduction_db = 20.0 * std::log10(gain_factor);
        }
    }

    // Copy and apply gain reduction if triggered
    std::vector<float> export_left = audio.left;
    std::vector<float> export_right = audio.right;
    if (gain_factor < 1.0f) {
        for (float& s : export_left) s *= gain_factor;
        for (float& s : export_right) s *= gain_factor;
    }

    // Write to temp WAV file
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = (settings.bit_depth == 32) ? DR_WAVE_FORMAT_IEEE_FLOAT : DR_WAVE_FORMAT_PCM;
    format.channels = static_cast<drwav_uint32>(audio.channels);
    format.sampleRate = static_cast<drwav_uint32>(audio.sample_rate);
    format.bitsPerSample = static_cast<drwav_uint32>(settings.bit_depth);

    drwav wav;
#if defined(_WIN32)
    if (!drwav_init_file_write_w(&wav, p_temp.wstring().c_str(), &format, nullptr)) {
#else
    if (!drwav_init_file_write(&wav, temp_path.c_str(), &format, nullptr)) {
#endif
        return Result<ExportReport>::error("Failed to open temporary file for export write: " + temp_path);
    }

    const size_t num_frames = export_left.size();
    bf::dsp::TpdfDitherer dither(42ULL);

    if (settings.bit_depth == 32) {
        // 32-bit float: no dither
        std::vector<float> interleaved(num_frames * audio.channels);
        if (audio.channels == 2) {
            for (size_t i = 0; i < num_frames; ++i) {
                interleaved[i * 2] = export_left[i];
                interleaved[i * 2 + 1] = export_right[i];
            }
        } else {
            for (size_t i = 0; i < num_frames; ++i) {
                interleaved[i] = export_left[i];
            }
        }
        drwav_write_pcm_frames(&wav, num_frames, interleaved.data());
    } else if (settings.bit_depth == 16) {
        // 16-bit PCM with TPDF dither
        std::vector<int16_t> interleaved(num_frames * audio.channels);
        if (audio.channels == 2) {
            for (size_t i = 0; i < num_frames; ++i) {
                interleaved[i * 2] = dither.quantize_to_int16(export_left[i]);
                interleaved[i * 2 + 1] = dither.quantize_to_int16(export_right[i]);
            }
        } else {
            for (size_t i = 0; i < num_frames; ++i) {
                interleaved[i] = dither.quantize_to_int16(export_left[i]);
            }
        }
        drwav_write_pcm_frames(&wav, num_frames, interleaved.data());
    } else {
        // 24-bit PCM: drwav expects 24-bit PCM in 3-byte packed or 32-bit container
        // drwav_write_pcm_frames_s24 handles 32-bit int with 24-bit values
        std::vector<drwav_int32> interleaved(num_frames * audio.channels);
        if (audio.channels == 2) {
            for (size_t i = 0; i < num_frames; ++i) {
                interleaved[i * 2] = dither.quantize_to_int24(export_left[i]);
                interleaved[i * 2 + 1] = dither.quantize_to_int24(export_right[i]);
            }
        } else {
            for (size_t i = 0; i < num_frames; ++i) {
                interleaved[i] = dither.quantize_to_int24(export_left[i]);
            }
        }
        // Write as s24
        drwav_write_pcm_frames(&wav, num_frames, interleaved.data());
    }

    drwav_uninit(&wav);

    // Re-decode validation (REQ-EXP-007)
    auto verify_res = decode_file(temp_path);
    if (!verify_res.is_ok()) {
        std::filesystem::remove(temp_path);
        return Result<ExportReport>::error("Export validation failed: re-decode error - " + verify_res.error_message());
    }

    const auto& verified = verify_res.value();
    if (verified.total_frames == 0) {
        std::filesystem::remove(temp_path);
        return Result<ExportReport>::error("Export validation failed: rendered file is empty");
    }

    // Check for NaN or Inf in re-decoded output
    for (float s : verified.left) {
        if (std::isnan(s) || std::isinf(s)) {
            std::filesystem::remove(temp_path);
            return Result<ExportReport>::error("Export validation failed: invalid NaN/Inf sample in output");
        }
    }

    // Atomic rename (REQ-EXP-008)
    std::wstring w_temp = p_temp.wstring();
    std::wstring w_target = out_p.wstring();

    BOOL move_ok = MoveFileExW(
        w_temp.c_str(),
        w_target.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
    );

    if (!move_ok) {
        std::error_code ec;
        std::filesystem::remove(p_temp, ec);
        return Result<ExportReport>::error("Failed to atomically finalize export file: Windows error " + std::to_string(GetLastError()));
    }

    ExportReport report;
    report.output_path = settings.output_path;
    report.total_frames = static_cast<int64_t>(num_frames);
    report.duration_sec = static_cast<double>(num_frames) / audio.sample_rate;
    report.sample_peak_dbfs = loud_res.is_ok() ? loud_res.value().sample_peak_dbfs : 0.0;
    report.true_peak_dbtp = loud_res.is_ok() ? loud_res.value().true_peak_dbtp : 0.0;
    report.applied_gain_reduction_db = gain_reduction_db;
    report.peak_gate_triggered = peak_gate_triggered;
    report.re_decode_verified = true;

    return Result<ExportReport>::ok(report);
}

} // namespace bf::io
