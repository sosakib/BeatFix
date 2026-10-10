#include "stretch.h"
#include <algorithm>
#include <cmath>

namespace bf::dsp {

class ResampleStretcher final : public ITimeStretcher {
public:
    ResampleStretcher() = default;

    Result<void> prepare(int num_channels, double sample_rate) override {
        if (num_channels <= 0 || num_channels > 2) {
            return Result<void>::error("Unsupported channel count for ResampleStretcher: " + std::to_string(num_channels));
        }
        m_channels = num_channels;
        m_sample_rate = sample_rate;
        m_prepared = true;
        return Result<void>::ok();
    }

    void reset() override {}

    [[nodiscard]] int latency_samples() const override {
        return 0; // Pure interpolation has zero algorithmic delay
    }

    [[nodiscard]] std::string id() const override {
        return "resample";
    }

    Result<void> render_span(
        std::span<const float> source_left,
        std::span<const float> source_right,
        std::span<float> out_left,
        std::span<float> out_right,
        const WarpMap& warp_map,
        double tgt_start_sec,
        double /*tgt_end_sec*/
    ) override {
        if (!m_prepared) {
            return Result<void>::error("ResampleStretcher not prepared before render");
        }

        const size_t num_out_samples = out_left.size();
        if (num_out_samples == 0) {
            return Result<void>::ok();
        }

        const bool is_stereo = (m_channels == 2 && !source_right.empty() && !out_right.empty());
        const double fs = m_sample_rate;
        const int64_t total_src_samples = static_cast<int64_t>(source_left.size());

        // High-quality Catmull-Rom cubic interpolation helper
        auto interpolate = [](const std::span<const float>& buf, double pos, int64_t total) -> float {
            int64_t i = static_cast<int64_t>(std::floor(pos));
            double frac = pos - static_cast<double>(i);

            auto sample_at = [&](int64_t idx) -> float {
                if (idx < 0) return 0.0f;
                if (idx >= total) return 0.0f;
                return buf[static_cast<size_t>(idx)];
            };

            float y0 = sample_at(i - 1);
            float y1 = sample_at(i);
            float y2 = sample_at(i + 1);
            float y3 = sample_at(i + 2);

            // Catmull-Rom spline formula
            double a = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
            double b = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
            double c = -0.5 * y0 + 0.5 * y2;
            double d = y1;

            return static_cast<float>(((a * frac + b) * frac + c) * frac + d);
        };

        for (size_t i = 0; i < num_out_samples; ++i) {
            double tau = tgt_start_sec + static_cast<double>(i) / fs;
            double s = warp_map.map_inverse(tau);
            double src_sample_pos = s * fs;

            out_left[i] = interpolate(source_left, src_sample_pos, total_src_samples);
            if (is_stereo) {
                out_right[i] = interpolate(source_right, src_sample_pos, total_src_samples);
            }
        }

        return Result<void>::ok();
    }

private:
    int m_channels = 2;
    double m_sample_rate = 44100.0;
    bool m_prepared = false;
};

std::unique_ptr<ITimeStretcher> create_resample_stretcher() {
    return std::make_unique<ResampleStretcher>();
}

} // namespace bf::dsp
