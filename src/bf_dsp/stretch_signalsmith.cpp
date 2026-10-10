#include "stretch.h"
#include "../../third_party/signalsmith-stretch/signalsmith-stretch.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace bf::dsp {

class SignalsmithStretcher final : public ITimeStretcher {
public:
    SignalsmithStretcher() = default;

    Result<void> prepare(int num_channels, double sample_rate) override {
        if (num_channels <= 0 || num_channels > 2) {
            return Result<void>::error("Unsupported channel count for SignalsmithStretcher: " + std::to_string(num_channels));
        }
        if (sample_rate <= 8000.0 || sample_rate > 384000.0) {
            return Result<void>::error("Invalid sample rate for SignalsmithStretcher: " + std::to_string(sample_rate));
        }

        m_channels = num_channels;
        m_sample_rate = sample_rate;
        m_stretch.presetDefault(m_channels, static_cast<float>(m_sample_rate));
        m_prepared = true;
        return Result<void>::ok();
    }

    void reset() override {
        if (m_prepared) {
            m_stretch.reset();
        }
    }

    [[nodiscard]] int latency_samples() const override {
        return m_prepared ? m_stretch.inputLatency() : 0;
    }

    [[nodiscard]] std::string id() const override {
        return "signalsmith";
    }

    Result<void> render_span(
        std::span<const float> source_left,
        std::span<const float> source_right,
        std::span<float> out_left,
        std::span<float> out_right,
        const WarpMap& warp_map,
        double tgt_start_sec,
        double tgt_end_sec
    ) override {
        if (!m_prepared) {
            return Result<void>::error("SignalsmithStretcher not prepared before render");
        }

        const size_t num_out_samples = out_left.size();
        if (num_out_samples == 0) {
            return Result<void>::ok();
        }

        reset();

        const bool is_stereo = (m_channels == 2 && !source_right.empty() && !out_right.empty());
        const size_t block_size = 512;
        const double fs = m_sample_rate;
        const int64_t total_src_samples = static_cast<int64_t>(source_left.size());

        std::vector<float> in_block_l;
        std::vector<float> in_block_r;
        std::vector<float> out_block_l(block_size);
        std::vector<float> out_block_r(block_size);

        size_t out_pos = 0;
        while (out_pos < num_out_samples) {
            size_t current_block_size = std::min(block_size, num_out_samples - out_pos);

            double tau0 = tgt_start_sec + static_cast<double>(out_pos) / fs;
            double tau1 = tgt_start_sec + static_cast<double>(out_pos + current_block_size) / fs;

            double s0 = warp_map.map_inverse(tau0);
            double s1 = warp_map.map_inverse(tau1);

            int64_t src_start = static_cast<int64_t>(std::llround(s0 * fs));
            int64_t src_end = static_cast<int64_t>(std::llround(s1 * fs));
            int64_t n_in = std::max<int64_t>(0, src_end - src_start);

            in_block_l.assign(static_cast<size_t>(n_in), 0.0f);
            if (is_stereo) {
                in_block_r.assign(static_cast<size_t>(n_in), 0.0f);
            }

            for (int64_t i = 0; i < n_in; ++i) {
                int64_t src_idx = src_start + i;
                if (src_idx >= 0 && src_idx < total_src_samples) {
                    in_block_l[static_cast<size_t>(i)] = source_left[static_cast<size_t>(src_idx)];
                    if (is_stereo) {
                        in_block_r[static_cast<size_t>(i)] = source_right[static_cast<size_t>(src_idx)];
                    }
                }
            }

            if (is_stereo) {
                const float* inputs[2] = { in_block_l.data(), in_block_r.data() };
                float* outputs[2] = { out_block_l.data(), out_block_r.data() };
                m_stretch.process(inputs, static_cast<int>(n_in), outputs, static_cast<int>(current_block_size));

                for (size_t i = 0; i < current_block_size; ++i) {
                    out_left[out_pos + i] = out_block_l[i];
                    out_right[out_pos + i] = out_block_r[i];
                }
            } else {
                const float* inputs[1] = { in_block_l.data() };
                float* outputs[1] = { out_block_l.data() };
                m_stretch.process(inputs, static_cast<int>(n_in), outputs, static_cast<int>(current_block_size));

                for (size_t i = 0; i < current_block_size; ++i) {
                    out_left[out_pos + i] = out_block_l[i];
                }
            }

            out_pos += current_block_size;
        }

        return Result<void>::ok();
    }

private:
    signalsmith::stretch::SignalsmithStretch<float> m_stretch;
    int m_channels = 2;
    double m_sample_rate = 44100.0;
    bool m_prepared = false;
};

std::unique_ptr<ITimeStretcher> create_signalsmith_stretcher() {
    return std::make_unique<SignalsmithStretcher>();
}

} // namespace bf::dsp
