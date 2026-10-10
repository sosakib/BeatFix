#pragma once

#include <span>
#include <cstdint>

namespace bf::dsp {

enum class TargetBitDepth {
    Int16 = 16,
    Int24 = 24,
    Float32 = 32
};

// Generates high-quality TPDF dither with fixed seed
class TpdfDitherer {
public:
    explicit TpdfDitherer(uint64_t seed = 123456789ULL);

    // Dithers float in [-1.0, 1.0] to int16 in [-32768, 32767]
    int16_t quantize_to_int16(float sample) noexcept;

    // Dithers float in [-1.0, 1.0] to int24 in [-8388608, 8388607]
    int32_t quantize_to_int24(float sample) noexcept;

    // Batch convert float32 to int16 with TPDF
    void process_to_int16(std::span<const float> in, std::span<int16_t> out) noexcept;

    // Batch convert float32 to int24 with TPDF
    void process_to_int24(std::span<const float> in, std::span<int32_t> out) noexcept;

private:
    uint64_t m_state = 123456789ULL;

    // Linear Congruential / Xorshift uniform generator in [-0.5, 0.5]
    float uniform_half() noexcept;
};

} // namespace bf::dsp
