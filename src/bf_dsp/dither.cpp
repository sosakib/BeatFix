#include "dither.h"
#include <algorithm>
#include <cmath>

namespace bf::dsp {

TpdfDitherer::TpdfDitherer(uint64_t seed)
    : m_state(seed == 0 ? 123456789ULL : seed) {}

float TpdfDitherer::uniform_half() noexcept {
    // 64-bit xorshift*
    m_state ^= m_state >> 12;
    m_state ^= m_state << 25;
    m_state ^= m_state >> 27;
    uint64_t val = m_state * 0x2545F4914F6CDD1DULL;
    // Normalized to [-0.5, 0.5]
    return (static_cast<float>(val & 0xFFFFFF) / 16777215.0f) - 0.5f;
}

int16_t TpdfDitherer::quantize_to_int16(float sample) noexcept {
    // TPDF = U1 - U2
    float dither = uniform_half() - uniform_half();
    // Scale to [-32768, 32767]
    float scaled = sample * 32767.0f + dither;
    long val = std::lround(scaled);
    if (val > 32767) val = 32767;
    if (val < -32768) val = -32768;
    return static_cast<int16_t>(val);
}

int32_t TpdfDitherer::quantize_to_int24(float sample) noexcept {
    float dither = uniform_half() - uniform_half();
    // Scale to [-8388608, 8388607]
    float scaled = sample * 8388607.0f + dither;
    long val = std::lround(scaled);
    if (val > 8388607) val = 8388607;
    if (val < -8388608) val = -8388608;
    return static_cast<int32_t>(val);
}

void TpdfDitherer::process_to_int16(std::span<const float> in, std::span<int16_t> out) noexcept {
    const size_t n = std::min(in.size(), out.size());
    for (size_t i = 0; i < n; ++i) {
        out[i] = quantize_to_int16(in[i]);
    }
}

void TpdfDitherer::process_to_int24(std::span<const float> in, std::span<int32_t> out) noexcept {
    const size_t n = std::min(in.size(), out.size());
    for (size_t i = 0; i < n; ++i) {
        out[i] = quantize_to_int24(in[i]);
    }
}

} // namespace bf::dsp
