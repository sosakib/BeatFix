# ADR-003: Beat Analysis Strategy

**Status:** Accepted, 2026-10-08. Open: legal review of the Beat This! training-data caveat; full vs small model decided by the Phase 3 performance gate.

## Context
The warp map is only as good as its anchors. BeatFix needs a dense, continuous, millisecond-accurate beat and downbeat sequence with per-beat confidence. Failure costs are asymmetric: a consistent octave or phase error is a one-click UI fix, but a **continuity error** (level or phase switching mid-song) or a missing/extra beat creates a 2× or 0.5× local stretch, which is critical. Standard MIR metrics (F1 at ±70 ms) are too lax: BeatFix targets ~5 ms anchor jitter ([BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) Why It Matters). Suno officially acknowledges tempo drift in its output (F11).

## Alternatives
| Option | Verdict | Reason |
|---|---|---|
| Pure DSP (SuperFlux + DP/HMM) | Fallback + cross-check | Periodicity mostly right (GTZAN Acc2 0.92) but octave much worse (Acc1 0.66 vs 0.83 for ML); no competitive downbeats |
| Beat This! + minimal post-processing | Agreement reference only | Frame-quantised (20 ms), no continuity model, "non-periodic beats" on hard material (authors) |
| Beat This! + madmom DBN | Rejected | madmom dependency (Python), fixed 55–215 BPM range forces octave errors on slow songs, reduces F1; unresolved low-confidence patent claim |
| **Beat This! activations + own decoder + DSP refinement** | **Chosen** | MIT model; deterministic owned post-processing addresses continuity and precision |
| madmom RNN/TCN models | Rejected | Model files CC BY-NC-SA 4.0 |
| Essentia / aubio | Rejected | AGPL-3.0 / GPL-3.0 |
| BeatNet | Rejected | Online-oriented, requires madmom + librosa |
| All-In-One (Demucs + NATTEN) | Deferred | Demixing gains ≤ 1–2 F1 points; heavy deps; weights licence unverified |
| Train our own small TCN | Later option | Needs licensed training data; only if the Beat This licence review fails |

## Decision
1. **Activations:** Beat This! (v1.1.0, `final0`) exported to ONNX by our own script, verified against PyTorch, pinned by SHA-256, run on **ONNX Runtime 1.30 CPU EP**. Telemetry disabled at startup (`DisableTelemetryEvents()`). Full model by default; switch to `small0` if the Phase 3 performance gate (analysis ≤ 0.15× RT on the reference laptop, PROVISIONAL) fails.
2. **Decoding is owned code:** deterministic DP decoder (Ellis-style, time-varying period) constrained to a ±40 % corridor around the chosen metrical level; missing/extra-beat repair; parabolic sub-frame refinement + optional transient snap with one global offset; meter/phase Viterbi for downbeats; feature-based logistic confidence.
3. **DSP tracker** (HPSS + SuperFlux → same decoder) is always computed: fallback when the model is missing/corrupt/fails (result marked "DSP mode", confidence capped at Amber), independent cross-check for confidence, and high-resolution source for refinement.
4. **Persist** activations (float16), model id + hash and preprocessing version with the analysis. Re-analysis is an explicit user action.
5. `IBeatAnalyzer` is one of the two interfaces in the codebase (Beat This ONNX, DSP tracker).

## Reasoning
- ML's measurable win is the metrical level and weak-onset robustness; DSP's is precision and independence. Using both covers the critical failures.
- Owning the decoder puts continuity, the tempo range (40–250 BPM) and determinism under our control, with no NC/GPL code and no madmom.
- ORT CPU inference is not bit-exact across machines (F10), so storing activations makes renders reproducible and reopening a project instant.
- CPU EP only: DirectML is in sustained engineering and GPU paths add driver variance for a small model.

## Tradeoffs
- ~78 MB model asset (8 MB if small) and an ORT DLL in the package.
- Our ONNX front-end (mel, chunking 1500/6 keep_first) must match the Python reference exactly; mismatch silently degrades beats → golden test vs Python.
- Inference time on mid-range x86 is **UNVERIFIED** (only datapoint: Rust port on Apple M4, ~0.017× RT).
- Training-data provenance risk remains until legal review.

## Consequences
- `bf_analysis` links ORT; `models/` pipeline with runtime SHA-256 check ([BUILD_AND_DEPENDENCIES.md](../08_engineering/BUILD_AND_DEPENDENCIES.md) §6).
- Analysis cancellation uses `Ort::RunOptions::SetTerminate()` ([ADR-011](ADR-011-threading.md)).
- Golden tests run ORT with `intra_op_num_threads = 1`; determinism levels D1/D2/D3 apply ([ADR-012](ADR-012-testing-strategy.md)).
- UI must expose ×2 / ÷2 / shift phase / set downbeat; these re-run decoding only (activations cached).
- An annotated Suno corpus (≥ 50 songs, Tier U, local only) is needed before confidence thresholds are frozen.

## References
- [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) F1–F14, §5–7, §10–13, Recommendation
- [LIBRARY_EVALUATION.md](../03_research/LIBRARY_EVALUATION.md) §C; [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) §10
- [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) F4 (ORT telemetry, SetTerminate)
- [BEAT_TRACKING.md](../04_algorithms/BEAT_TRACKING.md), [ONSET_DETECTION.md](../04_algorithms/ONSET_DETECTION.md)
- [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1, §6 (analysis determinism)
