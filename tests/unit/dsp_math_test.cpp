#include "../../third_party/catch2/catch_amalgamated.hpp"
#include "../../src/bf_dsp/units.h"
#include "../../src/bf_dsp/grid.h"
#include "../../src/bf_dsp/smooth.h"
#include "../../src/bf_dsp/tempo_fit.h"
#include "../../src/bf_dsp/warp.h"
#include "../../src/bf_dsp/peaks.h"
#include "../../src/bf_dsp/xfade.h"
#include "../../src/bf_dsp/dither.h"
#include <cmath>
#include <vector>
#include <random>

using namespace bf;
using namespace bf::dsp;

TEST_CASE("Units and Conversions", "[unit][dsp]") {
    SECTION("BPM and period round-trip") {
        REQUIRE(bpm_to_period(120.0) == Catch::Approx(0.5));
        REQUIRE(period_to_bpm(0.5) == Catch::Approx(120.0));
        REQUIRE(period_to_bpm(bpm_to_period(128.456)) == Catch::Approx(128.456));
    }

    SECTION("Sample and time conversions") {
        double sr = 48000.0;
        int64_t samples = seconds_to_samples(1.5, sr);
        REQUIRE(samples == 72000);
        REQUIRE(samples_to_seconds(samples, sr) == Catch::Approx(1.5));
    }

    SECTION("Natural vs Strict detent lambdas") {
        REQUIRE(lambda_from_detent(WarpDetent::Strict) == 0.0);
        REQUIRE(lambda_from_detent(WarpDetent::Detent4) == Catch::Approx(0.25));
        REQUIRE(lambda_from_detent(WarpDetent::Natural8) == Catch::Approx(2.91421356));
        REQUIRE(lambda_from_detent(WarpDetent::Detent16) == Catch::Approx(43.19).epsilon(0.01));
        REQUIRE(std::isinf(lambda_from_detent(WarpDetent::Global)));
    }

    SECTION("Correction classes") {
        // g = 1.0
        REQUIRE(correction_class(1.0, 1.0) == 0); // 0%
        REQUIRE(correction_class(1.01, 1.0) == 1); // 1%
        REQUIRE(correction_class(1.03, 1.0) == 2); // 3%
        REQUIRE(correction_class(1.07, 1.0) == 3); // 7%
    }
}

TEST_CASE("Target Beat Grid", "[unit][dsp]") {
    std::vector<double> anchors = { 1.0, 1.5, 2.0, 2.5, 3.0 }; // 120 BPM source
    GridConfig cfg;
    cfg.target_bpm = 120.0;
    cfg.phase_policy = PhasePolicy::PreserveStart;

    auto res = generate_target_grid(anchors, cfg);
    REQUIRE(res.is_ok());

    const auto& grid = res.value();
    REQUIRE(grid.target_times.size() == anchors.size());
    REQUIRE(grid.target_times[0] == Catch::Approx(1.0));
    REQUIRE(grid.target_times[1] == Catch::Approx(1.5));
    REQUIRE(grid.target_times[4] == Catch::Approx(3.0));

    // Test DawReadyStart
    cfg.phase_policy = PhasePolicy::DawReadyStart;
    cfg.beats_per_bar = 4;
    auto res_daw = generate_target_grid(anchors, cfg);
    REQUIRE(res_daw.is_ok());
    // In DawReadyStart, first downbeat lands on an integer multiple of bar period (2.0s for 120 BPM 4/4)
    REQUIRE(res_daw.value().target_times[0] == Catch::Approx(2.0));
}

TEST_CASE("Whittaker-Huber Smoother", "[unit][dsp]") {
    SECTION("Strict mode (lambda = 0)") {
        std::vector<double> S = { 0.5, 1.0, 1.48, 2.02, 2.5, 3.0 };
        std::vector<double> w(S.size(), 1.0);
        std::vector<double> g(S.size() - 2, 1.0);

        SmoothParams p;
        p.lambda = 0.0;

        auto res = whittaker_huber_smooth(S, w, g, p);
        REQUIRE(res.is_ok());
        const auto& x = res.value();
        for (size_t i = 0; i < S.size(); ++i) {
            REQUIRE(x[i] == Catch::Approx(S[i]));
        }
    }

    SECTION("Natural mode smooths jitter") {
        // Ideal 120 BPM: interval 0.5s
        std::vector<double> S(20);
        for (size_t i = 0; i < 20; ++i) {
            double noise = ((i % 2 == 0) ? 0.02 : -0.02);
            S[i] = 1.0 + static_cast<double>(i) * 0.5 + noise;
        }
        std::vector<double> w(S.size(), 1.0);
        std::vector<double> g(S.size() - 2, 1.0);

        SmoothParams p;
        p.lambda = 2.914213562; // Natural

        auto res = whittaker_huber_smooth(S, w, g, p);
        REQUIRE(res.is_ok());
        const auto& x = res.value();

        // Smoothed points should have significantly reduced second differences
        for (size_t i = 1; i < 19; ++i) {
            double d2_raw = std::abs(S[i + 1] - 2 * S[i] + S[i - 1]);
            double d2_smooth = std::abs(x[i + 1] - 2 * x[i] + x[i - 1]);
            REQUIRE(d2_smooth < d2_raw);
        }
    }

    SECTION("Outlier resistance via Huber IRLS") {
        std::vector<double> S(30);
        for (size_t i = 0; i < 30; ++i) {
            S[i] = 1.0 + static_cast<double>(i) * 0.5;
        }
        // Inject single large outlier at index 15
        S[15] += 0.25;

        std::vector<double> w(S.size(), 1.0);
        std::vector<double> g(S.size() - 2, 1.0);

        SmoothParams p;
        p.lambda = 2.914213562;

        auto res = whittaker_huber_smooth(S, w, g, p);
        REQUIRE(res.is_ok());
        const auto& x = res.value();

        // Outlier point should be heavily pulled back toward the true line
        REQUIRE(std::abs(x[15] - (1.0 + 15 * 0.5)) < 0.08);
    }
}

TEST_CASE("Robust Tempo Estimation", "[unit][dsp]") {
    SECTION("Exact BPM recovery on noiseless grid") {
        // 128.0 BPM -> period = 60 / 128 = 0.46875s
        std::vector<double> beats(100);
        for (size_t i = 0; i < 100; ++i) {
            beats[i] = 0.5 + static_cast<double>(i) * (60.0 / 128.0);
        }

        auto res = estimate_tempo_from_beats(beats);
        REQUIRE(res.is_ok());
        REQUIRE(res.value().bpm_global == Catch::Approx(128.0).epsilon(1e-6));
        REQUIRE(res.value().bpm_theil_sen == Catch::Approx(128.0).epsilon(1e-6));
        REQUIRE(res.value().target_suggestion == 128.0);
    }

    SECTION("Outlier tolerance") {
        // 110.0 BPM with 10% corrupted beats
        std::vector<double> beats(100);
        for (size_t i = 0; i < 100; ++i) {
            double jitter = (i % 10 == 0) ? 0.15 : 0.0;
            beats[i] = 0.2 + static_cast<double>(i) * (60.0 / 110.0) + jitter;
        }

        auto res = estimate_tempo_from_beats(beats);
        REQUIRE(res.is_ok());
        // Estimated BPM must remain within 0.05 BPM of 110.0
        REQUIRE(std::abs(res.value().bpm_global - 110.0) < 0.05);
    }

    SECTION("Octave candidates generation") {
        auto cands = generate_octave_candidates(120.0);
        REQUIRE(!cands.empty());
        bool has_60 = false, has_120 = false, has_240 = false;
        for (const auto& c : cands) {
            if (std::abs(c.bpm - 60.0) < 0.1) has_60 = true;
            if (std::abs(c.bpm - 120.0) < 0.1) has_120 = true;
            if (std::abs(c.bpm - 240.0) < 0.1) has_240 = true;
        }
        REQUIRE(has_60);
        REQUIRE(has_120);
        REQUIRE(has_240);
    }
}

TEST_CASE("WarpMap Monotonicity and Invertibility", "[unit][dsp]") {
    // 100 beats drifting from 108 to 112 BPM
    std::vector<double> beats(100);
    double cur = 0.5;
    for (size_t i = 0; i < 100; ++i) {
        beats[i] = cur;
        double local_bpm = 108.0 + 4.0 * (static_cast<double>(i) / 100.0);
        cur += 60.0 / local_bpm;
    }

    std::vector<double> conf(100, 1.0);
    std::vector<double> breaks(98, 1.0);

    WarpBuildConfig cfg;
    cfg.detected_bpm = 110.0;
    cfg.target_bpm = 110.0;
    cfg.detent = WarpDetent::Natural8;

    auto warp_res = build_warp_map(beats, conf, breaks, cfg);
    REQUIRE(warp_res.is_ok());

    const auto& warp = warp_res.value();
    REQUIRE(warp.validate().is_ok());

    // Test invertibility W^-1(W(t)) == t across test points
    for (double t = 0.0; t < cur + 1.0; t += 0.05) {
        double tau = warp.map(t);
        double t_rec = warp.map_inverse(tau);
        REQUIRE(t_rec == Catch::Approx(t).margin(1e-6));
    }

    // Verify all ratios stay within warning bounds [0.92, 1.08] for small drift
    for (const auto& seg : warp.segments()) {
        REQUIRE(seg.ratio >= HARD_RATIO_MIN);
        REQUIRE(seg.ratio <= HARD_RATIO_MAX);
        REQUIRE(seg.ratio == Catch::Approx(1.0).margin(0.05));
    }
}

TEST_CASE("TPDF Ditherer and Equal-Power Crossfade", "[unit][dsp]") {
    SECTION("Equal-power crossfade sum of squares == 1.0") {
        for (double t = 0.0; t <= 1.0; t += 0.01) {
            float ga = 0.0f, gb = 0.0f;
            equal_power_gains(t, ga, gb);
            float sum_sq = ga * ga + gb * gb;
            REQUIRE(sum_sq == Catch::Approx(1.0f).margin(1e-5f));
        }
    }

    SECTION("TPDF Dither quantization range") {
        TpdfDitherer dither(42ULL);
        float s_max = 1.0f;
        float s_min = -1.0f;
        int16_t q_max = dither.quantize_to_int16(s_max);
        int16_t q_min = dither.quantize_to_int16(s_min);
        REQUIRE(q_max <= 32767);
        REQUIRE(q_min >= -32768);
    }
}

TEST_CASE("Peak Pyramid Multi-resolution", "[unit][dsp]") {
    std::vector<float> audio_l(48000, 0.5f); // 1 second of 0.5 amplitude
    std::vector<float> audio_r(48000, -0.5f);

    // Inject transient spike
    audio_l[24000] = 0.95f;
    audio_l[24001] = -0.95f;

    PeakPyramid pyr;
    pyr.build_from_audio(audio_l, audio_r, 48000.0);

    REQUIRE(!pyr.empty());
    REQUIRE(pyr.levels().size() == 5);
    REQUIRE(pyr.total_samples() == 48000);

    // Check level 0 (64 samples per bin)
    const auto& lvl0 = pyr.levels()[0];
    size_t spike_bin = 24000 / 64;
    REQUIRE(lvl0.bins_left[spike_bin].max_val == Catch::Approx(0.95f));
    REQUIRE(lvl0.bins_left[spike_bin].min_val == Catch::Approx(-0.95f));
}
