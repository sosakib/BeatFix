# Architecture Decision Records

Each ADR records context, alternatives, decision, reasoning, tradeoffs, consequences and status. [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) is the one-page digest. If an ADR and the summary disagree, the summary wins and the ADR gets updated.

| ADR | Decision | Status |
|---|---|---|
| [ADR-001](ADR-001-desktop-framework-and-language.md) | C++20 + JUCE 9 (commercial tier) shell; framework-free engine core | Accepted |
| [ADR-002](ADR-002-internal-precision-and-sample-rate.md) | float32 buffers, float64/int64 time; native sample rate; ≤ 1 SRC at export | Accepted |
| [ADR-003](ADR-003-beat-analysis-strategy.md) | Beat This! ONNX activations + owned DP decoder; DSP fallback | Accepted |
| [ADR-004](ADR-004-tempo-estimation-and-octave-resolution.md) | Robust line fit + Whittaker curve; ranked octave candidates 40–250 BPM | Accepted |
| [ADR-005](ADR-005-warp-model-and-representation.md) | Whittaker/IRLS smoothed anchors, Natural↔Strict detents, derived JSON warp map | Accepted |
| [ADR-006](ADR-006-time-stretch-architecture.md) | Signalsmith "Smooth" + "Resample"; R3 dev-only reference; Transient in 1.5 | Accepted |
| [ADR-007](ADR-007-rendering-pipeline.md) | Deterministic chunk-parallel render, content-addressed cache | Accepted |
| [ADR-008](ADR-008-waveform-rendering.md) | Peak pyramid + instant warped estimate | Accepted |
| [ADR-009](ADR-009-codecs-resampling-export.md) | JUCE + minimp3 decode, LAME DLL, r8brain, libebur128; peak gate; validate | Accepted |
| [ADR-010](ADR-010-project-storage.md) | JSON project of inputs; atomic save; autosave; relink | Accepted |
| [ADR-011](ADR-011-threading.md) | Role threads + render pool; lock-free RT boundary | Accepted |
| [ADR-012](ADR-012-testing-strategy.md) | Synthetic-first layered testing; listening gate | Accepted |
| [ADR-013](ADR-013-build-packaging-distribution.md) | CMake FetchContent; GH Actions; Inno Setup; no updater | Accepted |

New ADRs use the next number and this format. Superseded ADRs stay, with `Status: Superseded by ADR-0xx`.
