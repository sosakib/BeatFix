# DSP Test Strategy (incl. Golden Audio Tests)

Status: v1.0, 2026-10-10. Metric IDs (A*, O*) refer to `METRICS.md`. Thresholds marked PROVISIONAL are calibrated in the Phase-0 spike (§7).

## Objective
Specify the synthetic test signals, the DSP component tests built on them, the golden-file methodology and the determinism policy, so that every analysis and render property can be checked against exact ground truth.

## Why It Matters
Synthetic signals give sample-exact ground truth for beats, tempo curves, pitch, phase and inter-channel relations — something no annotated real-music dataset can. They catch time-map bugs, drift, latency-compensation errors, chunk-boundary clicks and stereo breakage before a listener ever hears them.

## Key Findings
- Every output property in §24 of the brief can be tested synthetically except perceived artifact quality.
- Two verification paths are needed (Path K known-truth through W; Path R re-detection) to attribute errors (METRICS §2).
- Bitwise golden comparison is not portable across CPU/SIMD/thread configurations; a three-level determinism policy is required.

## Technical Details

### 1. Generator principles
- All generators in `tools/gen/` (Python, numpy/scipy/soundfile; dev-only), pure functions of (parameters, seed). Output: WAV (float32) + JSON sidecar with exact event times (seconds, float64 and integer sample index), tempo curve, f0 curve, channel relations, generator version.
- Default sr 48 kHz; every generator also runs at 44.1 kHz and 96 kHz in nightly.
- Levels: events −6 dBFS peak unless stated; no clipping in source; optional TPDF noise floor −90 dBFS to avoid digital-silence special cases (and a variant without it).
- Event placement at fractional sample positions via band-limited (windowed-sinc, 64 taps) fractional delay, so tests are not biased toward integer-sample timing.

### 2. Tempo-curve maths used by generators
- Constant: t_k = φ + k·60/B.
- Linear BPM ramp B(t) = B0 + (B1−B0)·t/T: beat phase φ(t) = (B0·t + (B1−B0)·t²/(2T))/60; beat k at φ(t_k) = k ⇒ t_k = [−B0 + sqrt(B0² + 2(B1−B0)·60k/T)]·T/(B1−B0) (B1 ≠ B0).
- Step change at beat m: constant B_a for k < m, B_b afterwards, continuous position at the step.
- Jitter: t_k + ε_k, ε ~ N(0, σ²) truncated at ±3σ, seeded; jitter applied to event *positions*, ground-truth "intended grid" stored separately.
- Synthetic drift on real material (Tier M/C): apply known smooth inverse warp V (e.g. sinusoidal tempo wobble ±1.5 % period 16 bars, or ramp) with a reference-quality resampler/stretcher to grid-locked material; ground truth beats = V(grid). Ideal correction = V⁻¹.

### 3. Synthetic signal catalogue

| ID | Signal | Parameters | Primary checks |
|---|---|---|---|
| S01 | Click train (exact BPM) | B ∈ {60, 90, 110, 120, 128, 140, 174, 200}; click types: (a) 1-sample impulse, (b) 2 ms Hann-windowed 1 kHz burst, (c) synthetic kick (S08); 60 s and 240 s | A10, A11, O1–O3, O17 |
| S02 | Fractional BPM | 109.73, 120.5, 127.999, 99.95 | A11 (fine tempo), O3 drift |
| S03 | Tempo ramp | 108→113 linear over 180 s; 120→125; 90→80 (decel) | A12 drift classification; O2/O3 after correction to 110 / 120 / 85; no seams (O15) |
| S04 | Step change | 120→126 at bar 33; 120→90 at bar 17; 128→64 (half-time switch) | step localization (± 1 beat), O15 at step, UI section split |
| S05 | Jitter | σ ∈ {2, 5, 10, 20} ms on S01(c) at 110 BPM | A10 (precision degrades gracefully), confidence monotonicity (A13), smoothing vs over-correction |
| S06 | Missing beats | drop 5 / 10 / 20 % random beats; drop 4 full bars (breakdown) | A9 FN handling, interpolation through gaps, no ratio spikes |
| S07 | Ghost notes / flams | 16th-note hits at −18 dB; flams (second hit +15 ms); double-detected transients | A9 FP rate (ghosts must not become anchors) |
| S08 | Kick/snare/hat loops | kick: exp-swept sine 150→45 Hz, 300 ms decay + 2 ms click; snare: band-passed noise + 180 Hz tone, 150 ms; hat: HP noise 40 ms. Patterns: four-on-floor, rock backbeat, half-time (snare on 3), double-time hats, syncopated (kick on "and of 2"), swing 60 % | tracker metric-level (A14), O8–O10 transient integrity, O1 |
| S09 | Chirps | log sweep 20 Hz–20 kHz 10 s; linear sweep 200–2000 Hz | magnitude response flatness after stretch (±0.5 dB PROVISIONAL), aliasing/imaging in Resample mode, pitch track continuity |
| S10 | Sustained tones | sines 55, 110, 440, 1000, 3520, 10000 Hz at −12 dBFS, 30 s; band-limited sawtooth f0 220 Hz; two-tone 440+446 Hz (6 Hz beating) | O6 pitch (cents), O11a sine SINAD, O11b beating preservation (phasiness detector) |
| S11 | Vibrato / glide tone | 440 Hz ± 50 cents at 5.5 Hz; glide 220→330 Hz over 2 s | O7 contour preserved after mapping through W (vocal proxy) |
| S12 | Stereo set | (a) L≡R; (b) L = −R; (c) ITD 0.5 ms (24 samples @ 48 kHz) noise-burst train; (d) ILD 6 dB; (e) independent L/R noise (ICC≈0); (f) L sine / R Hilbert-shifted sine (90°); (g) alternating hard-L/hard-R clicks | O13 a–f; channel-swap bug detection (g) |
| S13 | Noise | white, pink, 30 s, −20 dBFS RMS | O11c spectral flatness / level preservation; no tonal "metallic" artifacts |
| S14 | Edge cases | digital silence 10 s; DC offset +0.01; −90 dBFS noise only; 0 dBFS square wave (inter-sample overs); 0.5 s file (shorter than analysis window); 1-sample file; 60-min click file; mono file; 3+ channel file (rejected or downmixed per spec) | graceful status codes, O14, O16, O17, no crash |
| S15 | Transient + reverb tail | click + exponential decay noise tail (RT60 2 s) every 2 bars | tail smoothness, O15 at tail, tail policy (O17) |
| S16 | Suno-like composite | S08 loop + sustained pad (S10 saw chord) + vibrato lead (S11) + 108→113 ramp + MP3 192 kbps encode/decode | end-to-end smoke with lossy-decoder offset (O19); represents target material |

### 4. DSP component tests (L2)

| ID | Component | Test | Pass criteria |
|---|---|---|---|
| DSP-DEC-1 | Decoder | WAV/FLAC/AIFF bit-exact vs generator buffer | exact |
| DSP-DEC-2 | Decoder | MP3/AAC/Ogg/Opus: decode encoded S01(b); event times vs truth | offset 0 ± 1 sample after gapless/priming compensation (O19); report if container lacks gapless info |
| DSP-RES-1 | Resampler (SRC + "Resample" mode) | S09 sweeps & S10 tones through 44.1↔48↔96 kHz | passband ripple ±0.1 dB to 0.9·Nyquist_min, stopband/aliasing ≤ −100 dB PROVISIONAL; in Resample mode pitch shift equals expected 1200·log2(r) within 0.5 cent |
| DSP-ANA-1 | Analyzer | S01–S08 at all B | A10 median ≤ 2 ms / P95 ≤ 5 ms on clicks & synthetic kicks after sub-frame refinement (PROVISIONAL, aligns with BEAT_TRACKING target ≤ 5 ms median on drum material); A11 ≤ 0.01 BPM on 240 s constant tempo |
| DSP-ANA-2 | Analyzer | S03/S04 | ramp detected as drift; step located within ±1 beat |
| DSP-ANA-3 | Analyzer | S05–S07 | FP ≤ 2 %, FN ≤ 2 % for σ ≤ 5 ms and ghosts at −18 dB (PROVISIONAL) |
| DSP-ANA-4 | Analyzer | S08 half-time / double-time patterns | candidate list includes correct level; ambiguity flag raised where musically ambiguous |
| DSP-MAP-1 | Warp builder | detected beats from S03 → W | monotonic; r within clamp; Δr bounded; W(s_i) = T_i |
| DSP-STR-1 | Stretch engine via W (Path K) | S01(a–c), S08 at constant ratios {0.90, 0.95, 0.98, 1.00, 1.02, 1.05, 1.10} and on S03-derived W | O1 (median ≤ 1 ms, P95 ≤ 3 ms, max ≤ 5 ms PROVISIONAL); latency bias \|mean e\| ≤ 0.5 ms |
| DSP-STR-2 | Stretch engine | S10, S11 | O6, O7, O11a/b |
| DSP-STR-3 | Stretch engine | S08 | O8–O10 vs Rubber Band R3 reference render (relative, not absolute) |
| DSP-STR-4 | Stretch engine | S12 | O13 a,b exact (engine links channels — TIME_STRETCH rec. 10); c–f within thresholds |
| DSP-STR-5 | Stretch engine | S13, S15 | O11c; O15 zero flags |
| DSP-STR-6 | Stretch engine | extreme ratios {0.5, 0.75, 1.33, 2.0} and r(t) ramps between them | no NaN/Inf, O17 length exact, no crash; quality not asserted (listening only) |
| DSP-REN-1 | Chunk-parallel renderer | render S16 with 1, 2, 4, 8 threads and with chunk boundaries forced onto transients | null vs single-pass reference render (ARCHITECTURE rec. 5): residual ≤ −90 dBFS peak PROVISIONAL; O15 zero new flags at boundaries |
| DSP-REN-2 | Renderer | cancel at random points (100 runs) | no partial file left at target path; temp cleaned; cancel latency ≤ 250 ms |
| DSP-REN-3 | Renderer "Passthrough" (r≈1) | S16 with stable regions | seams inaudible: O15 zero flags; if bypass promised bit-exact, exact inside region |
| DSP-EXP-1 | Exporter | WAV 16/24/32f, FLAC 16/24, MP3 CBR/VBR | O19 round-trip; header sr/ch/bit depth; clip report matches counter; metadata present |
| DSP-INT-1 | Source immutability | full pipeline on read-only and writable source | source SHA-256 unchanged; source never opened with write access |

### 5. Golden audio tests (L3)

**What a golden is:** for a fixed input item + fixed project state (target BPM, mode, overrides) + engine version: the stored `metrics.json` (all METRICS values), the warp map (`warp.json`), per-platform SHA-256 of the rendered float32 buffer, and, for a small subset (≤ 20 clips × ≤ 15 s), the rendered audio itself (FLAC, for listening and null tests).

**Golden set (initial):**

| ID | Input | Project state | Asserted |
|---|---|---|---|
| GOLD-01 | S01(c) 110 BPM, 120 s | target 110 | O18 identity/passthrough; minimal warp (max \|r−1\| < 1e-4) |
| GOLD-02 | S03 108→113 | target 110 | O1–O4, O15 (no seams), drift corrected |
| GOLD-03 | S04 step 120→126 | target 120 | step handled, no ratio spike beyond clamp |
| GOLD-04 | S08 rock loop + S05 σ 10 ms | target 120 | O8–O10, A9 |
| GOLD-05 | S10 sines + S11 vibrato | ratio map from S03 | O6, O7 |
| GOLD-06 | S12 stereo set | ratio 1.05 | O13 all |
| GOLD-07 | S14 edge cases | default | status codes, O16, O17 |
| GOLD-08 | S16 Suno-like MP3 | target 110 | O19 decoder offset, full chain |
| GOLD-09 | Tier M drum performance with synthetic drift | target = native tempo | O2, O12 vs native render |
| GOLD-10 | Tier C vocal-heavy CC BY track | target = rounded BPM | O7, O14, regression-only O11 |
| GOLD-11 | Tier C dense EDM CC BY track | target = rounded BPM | O2, O13, O14 |
| GOLD-12 | Any input, Resample mode | ratio map from S03 | pitch shift equals map (DSP-RES-1 logic), no TSM artifacts |

**Comparison procedure:**
1. Recompute metrics; compare each to golden with per-metric tolerance (absolute for hard invariants, relative ±5 % / ±0.5 dB / ±0.5 ms for regression metrics — PROVISIONAL).
2. Compare buffer hash under the determinism level that applies (§6). If hash differs but D2 null-test passes → pass with note. If null-test fails → metric comparison decides; any metric outside tolerance → fail.
3. Failure report: metric diff table, waveform/spectrogram diff PNG, null-residual audio for listening.

**Re-baselining:** only via an explicit `--update-goldens` run on a clean tree, committed with a changelog entry stating cause, metric deltas, and listening confirmation for changes larger than D2 tolerance.

### 6. Determinism policy
- **D1 – bit-exact (same build, same OS, same CPU feature path, same thread count, same input, same project state):** rendered buffer SHA-256 identical. Requirements: deterministic chunk partitioning (boundaries a pure function of W and audio, not of scheduling), ordered merge, no float atomics/unordered reductions, fixed FFT plans (no runtime auto-tuning that may pick different kernels), seeded dither, ORT session options fixed (thread counts set explicitly; graph optimizations fixed).
- **D2 – numeric tolerance (different thread counts, CPUs, SIMD/FMA paths, compilers):** null residual peak ≤ −90 dBFS and RMS ≤ −110 dBFS (PROVISIONAL); analysis beat times within ±0.5 ms; BPM within ±0.001; warp map anchors within ±0.5 ms. Rationale: FMA contraction and vector-width reassociation change rounding at ~1e-7 relative; sub-frame interpolation can amplify ML activation differences slightly.
- **D3 – metric tolerance (algorithm/engine/model version change):** goldens compared by metrics only; re-baseline per §5.
- Product rule derived from D3: projects store engine + model versions; re-rendering an old project with a newer engine must warn that output may differ (owned by architecture/UX docs).

### 7. Phase-0 calibration procedure (freezing PROVISIONAL thresholds)
1. Run DSP-ANA-* and DSP-STR-* with the chosen analyzer and Signalsmith (default + high-quality presets) and Rubber Band R3 (reference) at ratios {0.90 … 1.10}.
2. For each metric, take the reference-engine distribution; set threshold = max(perceptual bound, measured P95 × 1.2) and record both numbers.
3. Thresholds tighter than what the reference engine achieves are flagged as unrealistic and escalated, not silently loosened.
4. Freeze into `ACCEPTANCE_CRITERIA.md` (orchestrator).

## Alternatives Considered
- Pre-rendered synthetic audio committed to the repo: rejected (bloat; generators are the source of truth).
- Testing the engine only at constant ratios: rejected — time-varying W is the product's actual use and where latency/hop bookkeeping bugs live.
- Pure impulse click trains only: rejected — mel/onset detectors and PV engines behave differently on band-limited kicks; both included.

## Evidence / Sources
QA_RESEARCH.md F1–F30; TIME_STRETCH_RESEARCH recs 2, 3, 8–10; BEAT_TRACKING_RESEARCH recs 3, 8; ARCHITECTURE_RESEARCH rec 5.

## Recommendation
Build `tools/gen` + `tools/eval` and DSP-STR-1/DSP-REN-1 first: they validate the two highest-risk mechanisms (time-map fidelity and chunk-parallel rendering) with minimal effort.

## Risks
- Synthetic success does not imply musical quality → listening tests remain mandatory.
- Event localization on smeared output (Path K) may itself be biased; validate the localizer on unstretched input first (must give 0 ± 0.1 ms).

## Open Questions
- Tail policy (O17) and exact "Passthrough" semantics (bit-exact or not).
- Whether the ONNX Runtime CPU EP is bit-deterministic run-to-run with fixed threads on Windows (**UNVERIFIED**; test D1 on analysis early).

## Implementation Implications
- CLI must accept an externally supplied W (`--warp-json`) so Path K tests do not depend on the analyzer.
- Render API must report the exact W⁻¹ and engine latency used.
- Goldens stored under `goldens/<id>/{metrics.json, warp.json, hashes.json, [audio.flac]}`.
