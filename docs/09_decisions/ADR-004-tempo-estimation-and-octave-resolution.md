# ADR-004: Tempo Estimation and Octave Resolution

**Status:** Accepted, 2026-10-08. Confidence thresholds and level-ranking weights are placeholders until calibrated on the corpus.

## Context
After beat decoding ([ADR-003](ADR-003-beat-analysis-strategy.md)) BeatFix must report a global BPM, a local tempo curve, a stability class and, above all, the right **metrical level**. Even state-of-the-art tempo estimation is only ~83–87 % octave-correct (Acc1), so roughly 1 song in 6–8 has an octave ambiguity ([TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) T1–T2). Fixed tempo floors cause systematic errors: a 55 BPM floor forces double tempo on 21 % of SMC tracks (T3). Frame-based periodicity is coarse: 4.8 BPM per lag step at 120 BPM and 50 fps (T6).

## Alternatives
| Estimator (global BPM) | Verdict | Reason |
|---|---|---|
| Mean IOI | Rejected | Telescopes to (last − first)/n; one bad endpoint shifts it |
| Median IOI | Sanity check | Snaps to the 20 ms frame lattice (111.1 / 107.1 BPM) |
| Tempogram / ACF peak | Candidates only | Coarse bins, ignores beat phase |
| OLS line fit | Rejected | Not robust to outlier beats |
| **Huber IRLS line fit on beat index** | **Chosen** | SE(BPM) ≈ 0.0009 BPM for 400 beats at σ = 10 ms |
| **Theil–Sen** | **Cross-check** | 29 % breakdown; disagreement > 0.05 BPM flagged |

Local tempo: moving median (stair-steppy), Kalman + RTS (equivalent, more parameters), LOESS (slower), **Whittaker IRLS** (chosen: same smoother as the warp, so displayed and applied tempo never diverge). Octave: fixed range + prior only (systematic bias); DBN with 55–215 BPM (rejected); **ranked candidates with a margin and a user question** (chosen).

## Decision
1. **Global BPM** = Huber IRLS fit `S_i ≈ a + b·i`, `BPM = 60/b`, with a block-bootstrap 95 % CI; Theil–Sen cross-check.
2. **Local tempo curve** from Whittaker IRLS-smoothed anchors at a fixed display cutoff N_c = 8 beats; drifting songs also report `bpmRange = [p5, p95]`.
3. **Candidates** = B̂ × {1/3, 1/2, 2/3, 1, 3/2, 2, 3} within **40–250 BPM**; salience from combined ACF × Fourier tempogram on the Beat This activation (DSP ODF in fallback).
4. **Level ranking** by features (salience, activation contrast, downbeat consistency, backbeat evidence, IOI regularity, weak log-normal prior at 120 BPM) with a **margin → Green / Amber / Red**. Amber/Red ask the user (one-click ×2 / ÷2).
5. **Local period path**: ±40 % corridor Viterbi over the tempogram keeps the level consistent.
6. **Stability classes** Stable / Drifting / Stepped / Spliced / Level switch / Unreliable. Intentional tempo changes are **flagged and the user chooses** "flatten" or "keep section unwarped (r = 1)". Single target BPM per song.
7. **Target suggestion** = `round(bpmGlobal)` ("detected 109.96 → suggested 110").

## Reasoning
- With hundreds of beats, jitter doesn't limit global BPM accuracy; drift does. A robust fit returns the best constant tempo, which a fixed grid needs.
- ACF and Fourier tempograms have opposite octave biases; combining them suppresses spurious levels (T8).
- A surfaced octave error costs one click; a silent one ruins the render.

## Tradeoffs
- More user questions on ambiguous material. Accepted.
- Backbeat heuristics fail on some genres (latin, afro, trap rolls); mitigated by the feature mix and the margin.
- The corridor can block a genuine large tempo change → per-segment re-centring when change-point and backbeat evidence agree.
- Weights risk over-fitting the annotated corpus → held-out split.

## Consequences
- Thresholds in one `params.h`, marked PROVISIONAL, logged with each analysis ([CODING_STANDARDS.md](../08_engineering/CODING_STANDARDS.md) §6).
- "High" confidence defined empirically: < 2 % octave errors and median |ΔBPM| < 0.1 on the stable subset.
- No thresholds frozen before the synthetic-drift set and ≥ 50 annotated Suno songs exist ([BENCHMARK_CORPUS.md](../07_quality/BENCHMARK_CORPUS.md)).

## References
- [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) T1–T10, Recommendation
- [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) F5, F7, F13
- [BPM_ESTIMATION.md](../04_algorithms/BPM_ESTIMATION.md), [HALF_DOUBLE_TIME_RESOLUTION.md](../04_algorithms/HALF_DOUBLE_TIME_RESOLUTION.md), [TEMPO_STABILITY_ANALYSIS.md](../04_algorithms/TEMPO_STABILITY_ANALYSIS.md)
- [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1, §3 item 6, §6
