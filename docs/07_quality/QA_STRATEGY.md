# QA Strategy (incl. Unit Test Strategy)

Status: v1.0, 2026-10-10. Consistent with ARCHITECTURE_RESEARCH proposal (C++20 + JUCE core, Catch2, CMake, GitHub Actions `windows-latest`), TIME_STRETCH_RESEARCH (Signalsmith "Smooth" default, "Resample" mode, Rubber Band R3 as reference), BEAT_TRACKING_RESEARCH (Beat This ONNX + own DP decoder + sub-frame refinement). If the stack decision changes, only tool names change; the strategy stands. Acceptance criteria are finalized by the orchestrator in `ACCEPTANCE_CRITERIA.md`.

## Objective
One coherent plan for verifying BeatFix: what is tested at which level, with which data, in which CI tier, and what blocks a release.

## Why It Matters
Audio bugs are often silent (a 25 ms MP3 offset, a click at a chunk boundary, a 0.05 BPM rounding drift). Users notice them only after export. The plan front-loads cheap deterministic tests (math, synthetic signals) and reserves expensive tests (corpus, listening) for decisions that need them.

## Key Findings
- Most correctness properties of BeatFix are mathematically checkable without real music: grid generation, warp monotonicity, ratio bounds, length, drift, stereo invariants, export integrity. → large unit/property + synthetic-DSP layer.
- Perceived quality is not objectively measurable for TSM (QA_RESEARCH F20–F23) → listening tests are a release gate for engine changes, not an afterthought.
- Test data licensing forces a tiered corpus; CI may only use synthetic, MIDI-rendered (CC BY 4.0) and CC BY/CC0 material.

## Technical Details

### 1. Test pyramid

| Level | Scope | Tooling | Data | Runs |
|---|---|---|---|---|
| L1 Unit + property | pure functions in `bf_dsp`/`bf_engine`: tempo math, grid, warp map, outlier rejection, candidates, confidence, metadata, project model, I/O conversion | Catch2 (+ RapidCheck or hand-rolled seeded generators for properties) | in-code vectors | every commit (< 2 min) |
| L2 Component DSP | decoder, resampler, stretch engine wrapper, analyzer pipeline, renderer, exporter on synthetic signals | Catch2 test executables + `tools/eval` Python metrics via CLI target | Tier S generated on the fly (seeded) | every PR (< 10 min, short signals) / nightly full |
| L3 Golden audio | end-to-end CLI: import → analyze → map → render → export; compare metrics + hashes vs stored goldens | BeatFix CLI (JUCE console target) + `tools/eval` | Tier S + M + C | nightly; mandatory before merge to `main` for DSP-touching changes |
| L4 Corpus benchmark | analysis and render metrics across corpus categories; trend vs previous release | `tools/eval/bench.py` | S, M, C in CI; P, U locally | nightly (S/M/C), release (all, on dev machine) |
| L5 Performance | RTF, memory, UI stall, cancel latency | CLI timing + ETW/`GetProcessMemoryInfo`; UI probe | fixed perf set | nightly smoke on CI (trend only), release on reference laptop |
| L6 Robustness / fuzz | malformed files, extreme params, long files, cancellation storms | libFuzzer/ASan job on decoders & project parser; scripted CLI | generated | nightly (time-boxed) |
| L7 Subjective listening | engine/mode decisions, release sign-off | webMUSHRA / ABX scripts | listening set (SUBJECTIVE_LISTENING_TESTS.md) | per engine change + milestone |
| L8 Acceptance / UX | user-visible scenarios (§44 matrix) | manual script + CLI-assisted checks | acceptance set | release candidate |

### 2. Unit test strategy (L1)

Principles: deterministic, no files, no threads unless the unit is a threading primitive, double precision for time math, every bug fix adds a regression test. Property tests use a fixed seed in CI and a random seed (logged) nightly.

| Area | Example tests | Properties / oracles |
|---|---|---|
| BPM calculation | 60/Δt; robust BPM from intervals (median, Theil–Sen/Huber regression); fractional BPM (109.73); ≥ 0.001 BPM resolution | Scale invariance: scaling all beat times by c scales BPM by 1/c. Exact recovery on noiseless grids (\|err\| < 1e-9 BPM). Breakdown: ≤ 20 % random outliers leave estimate within 0.05 BPM. |
| Interval calculation | Δt_i = t_i − t_{i−1}; local BPM_i | Empty/1-beat inputs return explicit "insufficient" status, never NaN. Unsorted input rejected. |
| Beat-grid generation | T_k = φ + k·60/B over [0, duration]; φ inference from detected beats (circular mean of phase residuals) | Known φ recovered within 0.1 ms on synthetic jittered beats (σ 5 ms, n ≥ 64). Grid count = floor((dur−φ)·B/60)+1. No cumulative float drift: T_k computed as φ + k·P (not repeated addition); \|T_10000 − exact\| < 1 µs. |
| Warp ratio | r_i = (T_{i+1}−T_i)/(S_{i+1}−S_i) | r = 1 for identical anchors; clamp to [r_min, r_max] with flag; never division by zero (merged/duplicate anchors rejected earlier). |
| Monotonic warp mapping | W and W⁻¹ (piecewise linear and monotone spline, per ALGORITHM docs) | Property: for random sorted anchor sets, W strictly increasing; W⁻¹(W(t)) = t within 1 µs; W(anchor_i) = target_i exactly; derivative bounded by clamp; boundary policy (before first / after last anchor) matches spec; monotone spline never overshoots (Fritsch–Carlson condition). |
| Outlier rejection | doubled transients, missing kicks, ghost notes injected into beat lists | Removing a known spurious beat restores CMLt; never removes > X % of beats on clean grids (no false repairs). |
| Half/double candidates | beat sets at 55/110/220, 70/140 | Candidate list contains all metric levels with scores; ranking stable under ±2 % jitter; tie → explicit ambiguity flag, not silent pick. |
| Confidence scoring | jitter, dropout, conflicting DSP vs ML | Monotonic: more jitter ⇒ lower or equal confidence. Range [0,1]. Calibration checked at L4 (ECE). |
| Metadata logic | sample rate/channel layout propagation, tags passthrough, artwork, filename sanitization, source hash | Round-trip equality; reserved Windows names/long paths handled; source file never opened for write (assert via read-only handle). |
| Project/session model | JSON (de)serialization, schemaVersion migration chain, relocation | Round-trip identity; every past schema fixture migrates; corrupted file → typed error, no crash. |
| Sample/time conversion | seconds ↔ samples at 44.1/48/96 kHz; resample ratio bookkeeping | Integer-sample rounding policy single-sourced; 10-min file: last-sample time error < 0.5 sample. |
| PCM conversion | float → int16/24 with TPDF dither (seeded in tests), clipping counter | Bit-exact against reference vectors; clip count correct; dither off ⇒ deterministic rounding. |

### 3. Component and golden tests (L2–L3)
Defined in `DSP_TEST_STRATEGY.md`: synthetic generators, Path K/Path R verification, golden-file method and determinism policy.

### 4. Test data management
| Tier | Content | Where stored | CI? | Licence rule |
|---|---|---|---|---|
| S | generated synthetic signals | generator code in repo; audio regenerated, never committed | yes | own |
| M | renders of Groove MIDI (CC BY 4.0) / own MIDI with own or CC0 synth samples | MIDI + render script in repo; audio cached as CI artifact | yes | attribution file |
| C | CC BY / CC0 real recordings (FMA, ccMixter, Freesound one-shots) + own annotations | Git LFS or release-asset bucket, manifest with per-file licence + SHA-256 | yes | only BY/CC0; NC/ND excluded |
| P | Ballroom, Hainsworth, GTZAN, SMC, Harmonix(annotations), RWC, GiantSteps, Beatles | developer machine only, path via env var | no | legal review pending; never committed |
| U | user-supplied Suno tracks (paid tier, written permission) | developer machine only, encrypted folder | no | evaluation only; no ML training (Suno ToS item 10); legal review pending |

Manifest schema: see `BENCHMARK_CORPUS.md` §4.

### 5. CI tiers
- **Per commit / PR (≤ 10 min):** build (MSVC, Release + RelWithDebInfo), L1 all, L2 short subset (signals ≤ 30 s), determinism D1 smoke (render twice, hash equal), clang-tidy/MSVC /W4 warnings-as-errors for core.
- **Nightly (≤ 2 h):** L2 full, L3 goldens, L4 on S/M/C, L5 smoke (trend), L6 fuzz 30 min with ASan, determinism D2 (thread counts 1/4/8; AVX2 vs SSE2 path if dispatch exists).
- **Release candidate:** everything + L4 on P/U (local), L5 on reference laptop, L7 listening sign-off for any engine/mode change since last release, L8 acceptance matrix.

### 6. Regression and sign-off rules
- Any change touching `bf_dsp`, stretch wrapper, decoder, exporter or analysis must pass L3 goldens; golden re-baselining requires: metric diff report, explanation in changelog, and (if render output changed beyond D2 tolerance) a listening check of the affected golden clips.
- Analysis metric regression: corpus-level CMLt or anchor precision worse by more than the bootstrap CI ⇒ block.
- Performance regression > 15 % RTF or > 10 % peak RSS vs last release on the reference laptop ⇒ block unless justified.
- Hard invariants (NaN, length, mono/polarity invariance, export round-trip, source immutability) are never re-baselined; they are fixed.

### 7. Traceability
Each requirement in `06_requirements` (SRS) gets a test ID (UT-*, DSP-*, GOLD-*, BENCH-*, PERF-*, LIS-*, ACC-*). The orchestrator's `ACCEPTANCE_CRITERIA.md` references METRICS IDs (A*, O*, P*) and thresholds once frozen.

## Alternatives Considered
- GoogleTest instead of Catch2: equivalent; follow architecture decision (Catch2 proposed).
- Golden tests by bitwise hash only: rejected — breaks across CPUs/SIMD/thread counts (DSP_TEST_STRATEGY §5).
- Running all tests on real music: rejected — licensing, cost, unknown ground truth.

## Evidence / Sources
See `03_research/QA_RESEARCH.md` (F1–F30) and sibling research documents named above.

## Recommendation
Implement L1/L2 synthetic infrastructure (generators + `tools/eval`) in Phase 1 alongside the core math, before UI work; goldens from the first end-to-end render; corpus Tier C/U annotation work in parallel with engine development so thresholds can be frozen before beta.

## Risks
- Python eval harness drift vs C++ metrics → keep metrics only in Python (single source), C++ tests call the CLI and assert on JSON.
- Flaky perf tests on shared CI runners → CI perf is trend-only, never gating.
- Solo-developer bandwidth for annotation and listening → minimum viable sizes defined in BENCHMARK_CORPUS / SUBJECTIVE_LISTENING_TESTS.

## Open Questions
- Final stack (affects tool names only).
- Whether CPU-dispatch SIMD paths exist (affects D2 matrix).
- Storage for Tier C audio (LFS quota vs release assets).

## Implementation Implications
- Repository layout gains `tests/` (Catch2), `tools/gen/`, `tools/eval/`, `corpus/manifest/`, `goldens/`.
- CLI target must expose: `analyze`, `render --warp-json`, `export`, `dump-warp`, deterministic flags (`--threads N`, `--seed`).
