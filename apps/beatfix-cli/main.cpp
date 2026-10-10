#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>

#include "../../src/bf_dsp/result.h"
#include "../../src/bf_dsp/units.h"
#include "../../src/bf_io/pcm_cache.h"
#include "../../src/bf_io/decode.h"
#include "../../src/bf_io/export.h"
#include "../../src/bf_analysis/analyzer.h"
#include "../../src/bf_engine/session.h"
#include "../../src/bf_engine/project_json.h"

void print_usage() {
    std::cout << "BeatFix CLI Tool (bftool) v0.1.0\n"
              << "Usage:\n"
              << "  bftool probe <audio_file>\n"
              << "  bftool analyze <audio_file>\n"
              << "  bftool fix <input_audio> <output_audio> [options]\n"
              << "         Options:\n"
              << "           --bpm <target_bpm>       (default: auto round to nearest integer)\n"
              << "           --detent <0..5>          (0=Natural64, 1=Natural32, 2=Natural16, 3=Natural8, 4=Natural4, 5=Strict, default: 3)\n"
              << "           --mode <smooth|resample> (default: smooth)\n"
              << "           --bitdepth <16|24|32>    (default: 16)\n"
              << "  bftool project-test <audio_file> <project_json_file>\n";
}

int cmd_probe(const std::string& file_path) {
    std::cout << "=== Probing File: " << file_path << " ===\n";
    auto probe_res = bf::io::probe_file(file_path);
    if (!probe_res.is_ok()) {
        std::cerr << "Error: " << probe_res.error_message() << "\n";
        return 1;
    }

    const auto& info = probe_res.value();
    std::cout << "Format:       " << info.format << "\n"
              << "Channels:     " << info.channels << "\n"
              << "Sample Rate:  " << info.sample_rate << " Hz\n"
              << "Bit Depth:    " << info.bit_depth << "-bit\n"
              << "Total Frames: " << info.total_frames << "\n"
              << "Duration:     " << std::fixed << std::setprecision(3) << info.duration_sec << " s\n"
              << "Supported:    " << (info.is_supported ? "YES" : "NO") << "\n";

    auto hash_res = bf::io::compute_file_sha256(file_path);
    if (hash_res.is_ok()) {
        std::cout << "SHA-256:      " << hash_res.value() << "\n";
    }

    return 0;
}

int cmd_analyze(const std::string& file_path) {
    std::cout << "=== Analyzing Rhythm: " << file_path << " ===\n";
    bf::engine::Session session;
    auto open_res = session.open_audio_file(file_path);
    if (!open_res.is_ok()) {
        std::cerr << "Failed to open audio: " << open_res.error_message() << "\n";
        return 1;
    }

    auto ana_res = session.run_analysis();
    if (!ana_res.is_ok()) {
        std::cerr << "Analysis failed: " << ana_res.error_message() << "\n";
        return 1;
    }

    const auto& analysis = session.analysis();
    std::cout << "Analyzer:         " << analysis.analyzer_id << "\n"
              << "Estimated BPM:    " << std::fixed << std::setprecision(2) << analysis.bpm_global << " BPM\n"
              << "Confidence:       " << analysis.confidence_reason << "\n"
              << "Detected Beats:   " << analysis.beats.size() << "\n";

    if (!analysis.beats.empty()) {
        std::cout << "\nFirst 10 detected beats:\n";
        size_t n = std::min<size_t>(10, analysis.beats.size());
        for (size_t i = 0; i < n; ++i) {
            const auto& b = analysis.beats[i];
            std::cout << "  Beat " << std::setw(3) << i << ": "
                      << std::fixed << std::setprecision(4) << b.time_sec << " s"
                      << (b.is_downbeat ? " [DOWNBEAT]" : "")
                      << " (conf: " << std::setprecision(2) << b.confidence << ")\n";
        }
    }

    return 0;
}

int cmd_fix(int argc, char* argv[]) {
    if (argc < 4) {
        print_usage();
        return 1;
    }

    std::string in_path = argv[2];
    std::string out_path = argv[3];

    double target_bpm = -1.0;
    bf::dsp::WarpDetent detent = bf::dsp::WarpDetent::Natural8;
    bf::dsp::StretchEngineMode mode = bf::dsp::StretchEngineMode::Smooth;
    int bit_depth = 16;

    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--bpm" && i + 1 < argc) {
            target_bpm = std::stod(argv[++i]);
        } else if (arg == "--detent" && i + 1 < argc) {
            int d = std::stoi(argv[++i]);
            if (d >= 0 && d <= 5) detent = static_cast<bf::dsp::WarpDetent>(d);
        } else if (arg == "--mode" && i + 1 < argc) {
            std::string m = argv[++i];
            if (m == "resample") mode = bf::dsp::StretchEngineMode::Resample;
        } else if (arg == "--bitdepth" && i + 1 < argc) {
            bit_depth = std::stoi(argv[++i]);
        }
    }

    std::cout << "=== BeatFix Processing Pipeline ===\n"
              << "Input:  " << in_path << "\n"
              << "Output: " << out_path << "\n";

    bf::engine::Session session;
    std::cout << "[1/5] Loading source audio...\n";
    auto open_res = session.open_audio_file(in_path);
    if (!open_res.is_ok()) {
        std::cerr << "Error loading audio: " << open_res.error_message() << "\n";
        return 1;
    }

    std::cout << "[2/5] Running rhythmic analysis...\n";
    auto ana_res = session.run_analysis();
    if (!ana_res.is_ok()) {
        std::cerr << "Analysis failed: " << ana_res.error_message() << "\n";
        return 1;
    }

    double detected_bpm = session.analysis().bpm_global;
    if (target_bpm <= 0.0) {
        target_bpm = std::round(detected_bpm);
    }

    std::cout << "  Detected BPM: " << std::fixed << std::setprecision(2) << detected_bpm << "\n"
              << "  Target BPM:   " << target_bpm << "\n";

    session.set_target_bpm(target_bpm);
    session.set_detent(detent);
    session.set_mode(mode);

    const auto& warp = session.warp_map();
    const auto& diag = warp.diagnostics();
    std::cout << "[3/5] Warp map diagnostics:\n"
              << "  Min ratio:  " << std::setprecision(4) << diag.min_ratio << "\n"
              << "  Max ratio:  " << diag.max_ratio << "\n"
              << "  Warn segments: " << diag.num_warn_segments << "\n"
              << "  Extreme segments: " << diag.num_extreme_segments << "\n";

    std::cout << "[4/5] Rendering corrected audio with Signalsmith Stretch...\n";
    auto render_res = session.render_corrected([](const bf::engine::RenderProgress& prog) {
        std::cout << "\r  Progress: " << static_cast<int>(prog.progress_fraction * 100.0) << "% ("
                  << prog.completed_chunks << "/" << prog.total_chunks << " chunks)" << std::flush;
    });
    std::cout << "\n";

    if (!render_res.is_ok()) {
        std::cerr << "Render error: " << render_res.error_message() << "\n";
        return 1;
    }

    std::cout << "[5/5] Exporting and validating audio output...\n";
    bf::io::ExportSettings exp_set;
    exp_set.output_path = out_path;
    exp_set.bit_depth = bit_depth;
    exp_set.lower_gain_if_peaking = true;

    auto exp_res = session.export_corrected(exp_set);
    if (!exp_res.is_ok()) {
        std::cerr << "Export failed: " << exp_res.error_message() << "\n";
        return 1;
    }

    const auto& rep = exp_res.value();
    std::cout << "=== SUCCESS ===\n"
              << "Saved to:                 " << rep.output_path << "\n"
              << "Duration:                 " << std::fixed << std::setprecision(3) << rep.duration_sec << " s\n"
              << "Sample Peak:              " << std::setprecision(2) << rep.sample_peak_dbfs << " dBFS\n"
              << "True Peak:                " << rep.true_peak_dbtp << " dBTP\n"
              << "Gain Reduction Applied:   " << rep.applied_gain_reduction_db << " dB\n"
              << "Re-decode Validation:     " << (rep.re_decode_verified ? "PASSED" : "FAILED") << "\n";

    return 0;
}

int cmd_project_test(const std::string& audio_path, const std::string& proj_path) {
    std::cout << "=== Testing Project Serialization & Deserialization ===\n";
    bf::engine::Session session;
    auto open_res = session.open_audio_file(audio_path);
    if (!open_res.is_ok()) {
        std::cerr << "Failed open: " << open_res.error_message() << "\n";
        return 1;
    }

    session.run_analysis();
    session.set_target_bpm(124.0);
    session.set_detent(bf::dsp::WarpDetent::Detent16);

    std::cout << "Saving project to " << proj_path << "...\n";
    auto save_res = session.save_project(proj_path);
    if (!save_res.is_ok()) {
        std::cerr << "Save project failed: " << save_res.error_message() << "\n";
        return 1;
    }

    std::cout << "Reloading project from " << proj_path << "...\n";
    bf::engine::Session session2;
    auto load_res = session2.load_project(proj_path);
    if (!load_res.is_ok()) {
        std::cerr << "Load project failed: " << load_res.error_message() << "\n";
        return 1;
    }

    if (std::abs(session2.target_bpm() - 124.0) > 1e-6) {
        std::cerr << "State mismatch: target_bpm is " << session2.target_bpm() << "\n";
        return 1;
    }

    if (session2.detent() != bf::dsp::WarpDetent::Detent16) {
        std::cerr << "State mismatch: detent\n";
        return 1;
    }

    std::cout << "Project round-trip validation PASSED.\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "probe" && argc >= 3) {
        return cmd_probe(argv[2]);
    } else if (cmd == "analyze" && argc >= 3) {
        return cmd_analyze(argv[2]);
    } else if (cmd == "fix") {
        return cmd_fix(argc, argv);
    } else if (cmd == "project-test" && argc >= 4) {
        return cmd_project_test(argv[2], argv[3]);
    } else {
        print_usage();
        return 1;
    }
}
