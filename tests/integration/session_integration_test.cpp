#include "../../third_party/catch2/catch_amalgamated.hpp"
#include "../../src/bf_dsp/result.h"
#include "../../src/bf_dsp/units.h"
#include "../../src/bf_io/pcm_cache.h"
#include "../../src/bf_io/decode.h"
#include "../../src/bf_io/export.h"
#include "../../src/bf_analysis/analyzer.h"
#include "../../src/bf_engine/session.h"
#include "../../src/bf_engine/project_json.h"
#include "../../third_party/dr_libs/dr_wav.h"

#include <vector>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Generates a synthetic click train at exact bpm
void generate_test_click_track(
    const std::string& out_path,
    double bpm,
    double duration_sec,
    double sample_rate = 44100.0
) {
    size_t total_samples = static_cast<size_t>(duration_sec * sample_rate);
    std::vector<float> samples(total_samples, 0.0f);

    double beat_interval_sec = 60.0 / bpm;
    size_t beat_interval_samples = static_cast<size_t>(beat_interval_sec * sample_rate);

    // Write a short click (burst of 1 kHz sine decaying over 10 ms) at each beat
    size_t click_len = static_cast<size_t>(0.010 * sample_rate);
    for (size_t b_idx = 0; b_idx * beat_interval_samples < total_samples; ++b_idx) {
        size_t start = b_idx * beat_interval_samples;
        for (size_t i = 0; i < click_len && (start + i) < total_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            double env = std::exp(-t * 200.0);
            samples[start + i] += static_cast<float>(0.8 * std::sin(2.0 * kPi * 1000.0 * t) * env);
        }
    }

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = 1;
    format.sampleRate = static_cast<drwav_uint32>(sample_rate);
    format.bitsPerSample = 32;

    drwav wav;
    drwav_init_file_write(&wav, out_path.c_str(), &format, nullptr);
    drwav_write_pcm_frames(&wav, total_samples, samples.data());
    drwav_uninit(&wav);
}

void safe_remove(const std::string& path) {
    for (int i = 0; i < 10; ++i) {
        std::error_code ec;
        if (std::filesystem::remove(path, ec) || !std::filesystem::exists(path, ec)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace

TEST_CASE("Session Audio Import and Peak Pyramid", "[integration]") {
    std::string test_wav = "test_synth_120bpm.wav";
    generate_test_click_track(test_wav, 120.0, 5.0, 44100.0);

    bf::engine::Session session;
    auto open_res = session.open_audio_file(test_wav);
    REQUIRE(open_res.is_ok());

    CHECK(session.source_audio().sample_rate == 44100.0);
    CHECK(session.source_audio().channels == 1);
    CHECK(session.source_audio().total_frames == 44100 * 5);
    CHECK_FALSE(session.source_pyramid().empty());
    CHECK(session.source_pyramid().total_samples() == 44100 * 5);

    safe_remove(test_wav);
}

TEST_CASE("Session Rhythmic Analysis and Warp Map", "[integration]") {
    std::string test_wav = "test_synth_110bpm.wav";
    generate_test_click_track(test_wav, 110.0, 6.0, 44100.0);

    bf::engine::Session session;
    REQUIRE(session.open_audio_file(test_wav).is_ok());

    auto ana_res = session.run_analysis();
    REQUIRE(ana_res.is_ok());
    CHECK(session.is_analyzed());

    // Estimated BPM should be close to 110 BPM (within 1%)
    CHECK(session.analysis().bpm_global == Catch::Approx(110.0).epsilon(0.02));
    CHECK_FALSE(session.analysis().beats.empty());

    // Warp map should be built
    const auto& warp = session.warp_map();
    CHECK_FALSE(warp.anchors().empty());
    CHECK(warp.validate().is_ok());

    // Setting target BPM to 120 should update warp map
    REQUIRE(session.set_target_bpm(120.0).is_ok());
    CHECK(session.target_bpm() == 120.0);
    CHECK(session.warp_map().target_bpm() == 120.0);

    // Estimated corrected pyramid should be populated
    CHECK_FALSE(session.corrected_pyramid().empty());

    safe_remove(test_wav);
}

TEST_CASE("Session Render, Peak Gate, and Export Round-trip", "[integration]") {
    std::string test_wav = "test_synth_render.wav";
    std::string export_wav = "test_synth_exported.wav";
    generate_test_click_track(test_wav, 120.0, 4.0, 44100.0);

    bf::engine::Session session;
    REQUIRE(session.open_audio_file(test_wav).is_ok());
    REQUIRE(session.run_analysis().is_ok());
    REQUIRE(session.set_target_bpm(126.0).is_ok());

    // Render corrected audio
    auto render_res = session.render_corrected();
    REQUIRE(render_res.is_ok());
    CHECK(session.is_rendered());
    CHECK_FALSE(session.corrected_audio().is_empty());
    CHECK(session.corrected_audio().pcm_hash.length() == 64);

    // Export corrected audio
    bf::io::ExportSettings exp_set;
    exp_set.output_path = export_wav;
    exp_set.bit_depth = 16;
    exp_set.lower_gain_if_peaking = true;

    auto exp_res = session.export_corrected(exp_set);
    REQUIRE(exp_res.is_ok());

    const auto& rep = exp_res.value();
    CHECK(rep.re_decode_verified == true);
    CHECK(rep.output_path == export_wav);
    CHECK(std::filesystem::exists(export_wav));

    // Re-verify decoding the exported file independently
    auto re_dec = bf::io::decode_file(export_wav);
    REQUIRE(re_dec.is_ok());
    CHECK(re_dec.value().sample_rate == 44100.0);
    CHECK(re_dec.value().total_frames > 0);

    safe_remove(test_wav);
    safe_remove(export_wav);
}

TEST_CASE("Session Project State Persistence Round-trip", "[integration]") {
    std::string test_wav = "test_synth_proj.wav";
    std::string proj_file = "test_project.beatfix";
    generate_test_click_track(test_wav, 120.0, 3.0, 44100.0);

    bf::engine::Session session;
    REQUIRE(session.open_audio_file(test_wav).is_ok());
    REQUIRE(session.run_analysis().is_ok());
    REQUIRE(session.set_target_bpm(130.0).is_ok());
    REQUIRE(session.set_detent(bf::dsp::WarpDetent::Detent16).is_ok());

    // Save project
    REQUIRE(session.save_project(proj_file).is_ok());
    CHECK(std::filesystem::exists(proj_file));

    // Load project into a fresh session
    bf::engine::Session session2;
    REQUIRE(session2.load_project(proj_file).is_ok());
    CHECK(session2.target_bpm() == 130.0);
    CHECK(session2.detent() == bf::dsp::WarpDetent::Detent16);
    CHECK(session2.is_analyzed());
    CHECK_FALSE(session2.warp_map().anchors().empty());

    safe_remove(test_wav);
    safe_remove(proj_file);
}

TEST_CASE("Session Undo / Redo Mechanism", "[integration]") {
    std::string test_wav = "test_synth_undo.wav";
    generate_test_click_track(test_wav, 120.0, 3.0, 44100.0);

    bf::engine::Session session;
    REQUIRE(session.open_audio_file(test_wav).is_ok());
    REQUIRE(session.run_analysis().is_ok());

    double orig_bpm = session.target_bpm();
    REQUIRE(session.set_target_bpm(140.0).is_ok());
    CHECK(session.target_bpm() == 140.0);
    CHECK(session.can_undo());

    REQUIRE(session.undo().is_ok());
    CHECK(session.target_bpm() == orig_bpm);
    CHECK(session.can_redo());

    REQUIRE(session.redo().is_ok());
    CHECK(session.target_bpm() == 140.0);

    safe_remove(test_wav);
}
