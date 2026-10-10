# ADR-002: Internal Precision and Sample Rate

**Status:** Accepted, 2026-10-08.

## Context
BeatFix moves audio in time with millisecond accuracy over songs that can last 10–60 minutes. Precision bugs here are silent: a beat drifts by 20 ms at minute 9, or an export is off by a resampler's group delay. Memory matters too: 10 min of 48 kHz stereo float32 is 230.4 MB ([PERFORMANCE_BENCHMARKS.md](../07_quality/PERFORMANCE_BENCHMARKS.md) §3). Sources are typically 44.1 or 48 kHz MP3/WAV (Suno format details **UNVERIFIED**, [AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md) F14).

## Alternatives
| Option | Verdict | Why |
|---|---|---|
| float32 buffers, float32 positions | Rejected | float32 represents integers exactly only up to 2²⁴ = 16,777,216 samples ≈ 6.3 min at 44.1 kHz; positions beyond that round to 2+ samples and drift |
| **float32 buffers, float64/int64 time and accumulators** | **Chosen** | Full precision where it matters (time, sums, filters), half the memory of f64 buffers |
| float64 buffers everywhere | Rejected | 2× memory and bandwidth (460 MB for 10 min 48 kHz stereo) with no audible benefit: float32 has ~144 dB dynamic range per sample, below any converter |
| Fixed internal rate (48 or 96 kHz) | Rejected | An extra SRC on import moves analysis grids, costs time, and adds a filter to every file; no demonstrated quality benefit |
| Oversampled time-stretch (2× rate) | Rejected | Doubles render cost; no evidence of benefit at ±10 % ratios ([RESAMPLING.md](../04_algorithms/RESAMPLING.md) Alternatives) |
| **Native source rate, one SRC at export only when needed** | **Chosen** | Zero conversions in the common case |

## Decision
1. **Sample buffers are float32.** Accumulators (filters, regressions, loudness, resampler MACs) are float64.
2. **All time/position values are float64 seconds or int64 sample frames.** Never float32, never `int`. Sample counts per block are derived from absolute positions (`round(W⁻¹(τ)·fs)`), never accumulated.
3. **Process at the native source rate.** Analysis gets its own 22.05 kHz mono copy (zero-phase, never feeds the render). Rendering never resamples.
4. **At most one SRC in the export path, only when the user asks or the codec forces it** (e.g. 96 kHz → MP3 needs 44.1/48 kHz), after render and before gain/measure/dither/encode. Equal rates → bit-exact bypass.
5. Float WAV export is never dithered; integer exports get TPDF dither ([DITHERING.md](../04_algorithms/DITHERING.md)).

## Reasoning
- The float32 integer limit is a hard arithmetic fact, not a tuning question; f64 time costs nothing measurable.
- Every SRC adds a filter and a delay that must be compensated; the safest SRC is the one not performed. The research found no quality argument for a higher internal rate.
- One rule for all modules removes a whole class of unit bugs.

## Tradeoffs
- Two numeric types to keep straight (f32 samples vs f64 time). Mitigated by unit-suffixed names and `clang-tidy` narrowing checks as errors ([CODING_STANDARDS.md](../08_engineering/CODING_STANDARDS.md) §5, §8).
- Different sources in the same session would run at different rates. Single-track MVP makes this moot.

## Consequences
- Warp map, beat times, BPM: f64 in memory and in JSON (shortest round-trip formatting).
- Renderer and resampler wrappers carry tests for exact output length and 0-sample timing offset ([RESAMPLING.md](../04_algorithms/RESAMPLING.md) R3/R4).
- **Playback when the device rate ≠ source rate** is not settled by the research: ARCHITECTURE_RESEARCH §4 proposes a background source-at-device-rate copy for preview. Interpretation adopted here: the "one SRC" rule governs data that is analysed, rendered or exported; a monitoring-only conversion for playback never feeds those paths. First choice remains opening the device at the source rate. To be confirmed in [AUDIO_PIPELINE.md](../05_architecture/AUDIO_PIPELINE.md) (flagged in hand-back).

## References
- [AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md) Recommendations 3–5
- [DSP_RESEARCH.md](../03_research/DSP_RESEARCH.md) Recommendations 3–4
- [TIME_STRETCH_RESEARCH.md](../03_research/TIME_STRETCH_RESEARCH.md) TD-4 (f64 positions)
- [RESAMPLING.md](../04_algorithms/RESAMPLING.md), [DITHERING.md](../04_algorithms/DITHERING.md), [WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md) §7
- [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1 (Internal precision, Sample rate)
