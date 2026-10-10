# ADR-012: Testing Strategy

**Status:** Accepted, 2026-10-08.

## Context
No objective metric reliably predicts perceived time-stretch quality (best published PCC 0.865, code unlicensed). Beat metrics like F1 at ±70 ms are too loose for warp anchors (20 ms of anchor error is a 3.7 % local ratio error at 110 BPM). Most public beat datasets are non-commercial or unlicensed. Suno's ToS restricts the use of outputs.

## Alternatives
| Option | Verdict | Why |
|---|---|---|
| Rely on public datasets + F1 | Rejected | wrong granularity, licence-restricted, and the model was trained on most of them |
| Objective quality metric as release gate | Rejected | not predictive enough |
| **Layered: unit math → synthetic ground truth (exact BPM, ramps, steps, jitter, impulses, sines, stereo ITD) → MIDI-rendered references (Groove MIDI / E-GMD, CC BY 4.0) → tolerance-based golden files → corpus benchmarks with product metrics (anchor ms, spurious/missing rate, CMLt, fine BPM error) → blind listening gate** | **Chosen** | ground truth where possible, people where necessary |

## Decision
Catch2 for C++. A Python + mir_eval 0.8.2 harness (dev-only) for corpus metrics. Determinism levels D1 (bit-exact same build/thread count), D2 (cross-machine null ≤ −90 dBFS, beats ±0.5 ms), D3 (version changes: metrics + listening). Path K / Path R output checks. Listening: pairwise A/B per engine change, ABX for transparency claims, MUSHRA-style rounds at milestones with stretch-specific anchors, and non-inferiority ≤ 5 points vs Rubber Band R3. Corpus tiers S/M/C go in CI; P/U stay local only. Calibration is fitted on S/M/C only (Suno fitting is pending legal review).

## References
[QA_RESEARCH.md](../03_research/QA_RESEARCH.md), [QA_STRATEGY.md](../07_quality/QA_STRATEGY.md), [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md), [METRICS.md](../07_quality/METRICS.md), [BENCHMARK_CORPUS.md](../07_quality/BENCHMARK_CORPUS.md), [SUBJECTIVE_LISTENING_TESTS.md](../07_quality/SUBJECTIVE_LISTENING_TESTS.md).
