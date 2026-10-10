# ADR-005: Warp Model and Representation

**Status:** Accepted, 2026-10-08.

## Context
The warp `W` (source time → target time) decides every audible timing result. Detected beats carry detector noise (σ ≈ 5–15 ms on full mixes). Suno drift is reported to be slow (ramps, section offsets; magnitudes **UNVERIFIED**). Producers fear flattened groove as much as drift ([COMPETITOR_RESEARCH.md](../03_research/COMPETITOR_RESEARCH.md) F7). The renderer is pull-based and needs `W⁻¹`.

## Alternatives
| Option | Verdict | Why |
|---|---|---|
| Strict map of raw detections | Rejected as default (kept as the "Strict" detent) | copies detector noise into the output (`W(s_i) = T_i − rε_i`) and destroys groove |
| Constrained QP over a spline with transient and phase penalties | Rejected | needs an unvalidated model linking W to engine artifacts. Heavy solver |
| Joint tracking + warp HMM | Rejected | couples two replaceable modules, hard to test |
| **Whittaker–Eilers 2nd-order smoother with Huber IRLS, confidence weights and change-point breaks; piecewise-linear monotone W** | **Chosen** | closed-form banded solve O(n), one interpretable knob (cutoff N_c in beats), deterministic |
| PCHIP (monotone cubic) W | Deferred | adopt only if ratio steps prove audible in Strict |
| Store the warp map in the project | Rejected | two sources of truth. The warp map is **derived** from persisted inputs |

## Decision
1. Anchors are optimised by `(W + λDᵀGD)x = WS` with Huber IRLS. λ comes from the detent N_c ∈ {∞, 32, 16, 8 (default), 4, Strict} via `λ = 1/(16 sin⁴(π/N_c))`.
2. W is piecewise linear on `(x_i, T_i)`, validated before every render (monotone, `r ∈ [0.5, 2]`, warn bands per DECISION_SUMMARY §6).
3. The warp map is a derived JSON artifact with `schemaVersion`, `algoVersion` and `inputsHash`. The project stores inputs only.

## Reasoning
The smoother is a low-pass filter on beat positions: deviations slower than N_c beats (drift) are corrected, faster ones (groove, detector noise) are kept. Weighting makes drumless sections follow the smooth tempo automatically. Breaks keep intentional steps sharp. See [WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md) §4.

## Tradeoffs
- Natural mode leaves fast deviations in, including genuinely wrong ones. Strict is one click away, and the residual is shown ("grid err ≤ X ms").
- The default N_c = 8 is an assumption until listening test S-3.

## Consequences
One shared smoother for display and warp (REQ-WARP-004). Unit tests: λ = 0 → identity, λ → ∞ → linear fit, frequency response within ±10 %.

## References
[WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md), [WARP_MAP.md](../04_algorithms/WARP_MAP.md), [TARGET_BEAT_GRID.md](../04_algorithms/TARGET_BEAT_GRID.md), [TEMPO_STABILITY_ANALYSIS.md](../04_algorithms/TEMPO_STABILITY_ANALYSIS.md), [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md).
