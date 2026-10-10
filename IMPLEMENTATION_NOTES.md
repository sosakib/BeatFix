# Implementation Notes & Decision Log

## Architecture & Hierarchy of Authority
1. **DECISION_SUMMARY.md** is the supreme authority in case of cross-document discrepancies.
2. **MVP_DEFINITION.md** and **SOFTWARE_SYSTEM_REQUIREMENTS.md** define scope boundaries.
3. Pure C++20 engine core (`bf_dsp`, `bf_analysis`, `bf_io`, `bf_engine`) has **zero JUCE dependencies**. JUCE 9 is strictly confined to `apps/beatfix/` (GUI shell and WASAPI audio playback).

## Key Implementation Decisions & Technical Solutions

1. **Error Handling**: Per DECISION_SUMMARY §1, `std::expected` is replaced by a strongly typed `Result<T>` struct (`value()`, `take_value()`, `error_message()`, `is_ok()`) across all pure core libraries.
2. **Numerical Precision & Zero Drift**:
   - Audio sample buffers: `float32`.
   - Accumulators, time coordinates, anchor positions, durations, sample rates, BPM: strictly `float64` (`double`) or `int64_t`.
   - Sample accounting in time warping: Always computed from absolute $W^{-1}(\tau)$ positions ($s = \text{round}(W^{-1}(\tau) \cdot f_s)$), never accumulated incrementally, mathematically ensuring zero cumulative drift.
3. **Stretch & Resample Modes**:
   - Primary: Signalsmith Stretch 1.4.0 ("Smooth" pitch-preserving mode), driven by inverse warp map $W^{-1}$ with linked-stereo processing.
   - Secondary: "Resample" mode (Catmull-Rom cubic varispeed interpolation).
4. **Natural vs Strict Warp Detents**:
   - 6 detents: Global ($\infty$), 32 bars, 16 bars, **8 bars (Natural, default)**, 4 bars, Strict (0).
   - Cutoff parameter $\lambda = \frac{1}{16 \sin^4(\pi / N_c)}$ for $N_c \in \{4, 8, 16, 32\}$; $\lambda = 0$ for Strict.
   - Correction heatmap: Intensity calculated using $|\log_2(r/g)|$ where $g = B_{\text{detected}} / B_{\text{target}}$, preventing global tempo change from coloring the entire song.
5. **Decoders & File Integrity**:
   - Source is treated as strictly immutable (read-only handle).
   - MP3 gapless decode trims LAME delay and padding using `minimp3_ex`.
   - Decoded PCM cached once in float32 native sample rate with SHA-256 fingerprint computed via Windows BCrypt CNG API (`BCryptOpenAlgorithmProvider`, `BCryptCreateHash`, `BCryptHashData`, `BCryptFinishHash`).
6. **Peak Gate & Export Pipeline**:
   - Peak gate: 0 dBFS integer / -1 dBTP true peak measured using `libebur128`.
   - Safe headroom reduction: applies safe gain reduction if signal peaks above gate.
   - High-precision TPDF dither applied on 16/24-bit PCM exports.
   - Atomic temporary file commit via Windows `MoveFileExW` (`MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`).
   - Temporary export file preserves extension (e.g. `.tmp.<pid>.wav`) so re-decode validation can verify file format before commit.
7. **Beat Analysis & Privacy**:
   - Owned DSP beat tracker using spectral novelty flux, Dan Ellis dynamic programming decoder, downbeat estimation, and stability classification.
   - Prebuilt ONNX Runtime 1.20.1 integration: telemetry explicitly disabled via `env.DisableTelemetryEvents()`, with automatic fallback to pure DSP tracker if model is unavailable.
8. **JUCE 9 GUI Integration**:
   - Dual-lane waveform view with neutral original waveform, class-tinted corrected waveform, and slanting gutter connectors.
   - Real-time 20 ms equal-power crossfader between Source A and Source B.
   - Metronome click generator synchronized to beat markers.
   - `JUCE_MODAL_LOOPS_PERMITTED=1` defined for native dialogs.
