# Metrics

Status: v1.0, 2026-10-10. Thresholds marked **PROVISIONAL** must be calibrated in the Phase-0 engine spike (see `DSP_TEST_STRATEGY.md` §7) before being frozen into `ACCEPTANCE_CRITERIA.md` (owned by the orchestrator, finalized after architecture decisions).

## Objective
Define every measurable quantity BeatFix uses to judge (a) analysis quality (BPM / beats / downbeats / drift), (b) corrected-output quality (alignment, tempo, pitch, transients, artifacts, stereo, signal hygiene), and (c) performance. Each metric: definition, formula, measurement procedure, tool, known limitations, provisional threshold.

## Why It Matters
BeatFix makes two independent kinds of error: **detector errors** (wrong beats → wrong warp map) and **engine errors** (right map, badly rendered). Generic MIR metrics only cover the first; generic audio-quality metrics (PEAQ, ViSQOL) do not work on time-scaled audio without alignment and are poor TSM quality predictors (Roberts & Paliwal). A metric set that separates the two error sources, and is honest about what only humans can judge, is the basis of every test in this package.

## Key Findings
1. mir_eval 0.8.2 (MIT, 2025-02-25) implements all standard beat metrics; `evaluate()` **trims beats < 5 s by default** and its Information Gain is **normalized to [0,1]** (the original Beat Evaluation Toolbox reports bits, max log2(41)≈5.36). Reports must state which convention is used. (High)
2. Standard MIR beat tolerance (±70 ms F-measure) is far too lax for warp-anchor quality: a 20 ms anchor error over a 545 ms IOI (110 BPM) is a 3.7 % local ratio error. BeatFix needs **product-specific, millisecond-scale anchor metrics** in addition to the standard set. (High, derived)
3. Beat This! (the baseline-recommended tracker) has 50 fps frame resolution (hop 441 @ 22.05 kHz → 20 ms; uniform quantization error ±10 ms, RMS ≈ 5.8 ms) and, per its own paper, lower continuity (CMLt 79.8 vs 81.2) than prior SOTA because it "introduces non-periodic beats" on complex pieces. For a warp tool, **continuity and spurious-anchor rate matter more than F1**. (High — paper Table 2)
4. Tempo Acc1/Acc2 (±4 %) are the field standard but ±4 % at 110 BPM = ±4.4 BPM, useless for fractional-BPM claims. Add an absolute fine-tempo error. (High)
5. No validated objective TSM quality metric exists for general music. Best published: Roberts & Paliwal double-ended measure PCC 0.865 / RMSE 0.487 (MOS scale), single-ended BGRU PCC 0.794. Code (MATLAB/PyTorch, github zygurt/TSM) has **no licence file** → internal research use only after legal review; cannot be a gating metric. (High)

## Technical Details

Notation: source signal x(t), source beat times s_i (detected) or s*_i (ground truth), target grid T_k = φ + k·60/B (B = target BPM), warp map W: t_src → t_out (monotonic), corrected output y(t). sr = sample rate. "Synthetic" = generated signal with exact known event times (DSP_TEST_STRATEGY §3).

### 1. Analysis metrics (source side)

| ID | Metric | Definition / formula | Tool | Notes |
|---|---|---|---|---|
| A1 | Beat F-measure | Hits within ±70 ms (one-to-one matching), F = 2PR/(P+R) | `mir_eval.beat.f_measure(ref, est, 0.07)` | Report with **and** without 5 s trim; BeatFix warps the intro too, so the *untrimmed* number is the product-relevant one. |
| A2 | Cemgil | mean Gaussian error, σ = 40 ms | `mir_eval.beat.cemgil` | Also returns best-metric-level variant. |
| A3 | Goto | binary, ≥25 % continuous correct segment with error-statistics thresholds (0.35, μ 0.2, σ 0.2 of IOI) | `mir_eval.beat.goto` | Legacy; report only. |
| A4 | P-score | cross-correlation of impulse trains, window 0.2·median IOI | `mir_eval.beat.p_score` | |
| A5 | CMLc / CMLt / AMLc / AMLt | continuity, phase & period tolerance 17.5 %; AML accepts double, half (odd/even), off-beat variants | `mir_eval.beat.continuity` | **Primary gating metric for anchor sets** (CMLt) together with A9. AML vs CML gap quantifies octave/phase errors. |
| A6 | Information gain | 1 − H(beat-error histogram, 41 bins)/log2(41) | `mir_eval.beat.information_gain` | mir_eval value ∈[0,1]; multiply by log2(41) to compare with bit-valued literature. |
| A7 | Downbeat F | A1 applied to downbeat sequences | `mir_eval.beat.f_measure` on downbeats | Same practice as Beat This! paper. |
| A8 | Tempo Acc1 / Acc2 | Acc1: \|B̂−B\|/B ≤ 4 %. Acc2: same, also accepting B·{2, 3, 1/2, 1/3} | own 10-line implementation (mir_eval only has the MIREX two-tempo p-score, `mir_eval.tempo.detection`, tol 8 %) | Gouyon et al. 2006 definitions. Acc2−Acc1 = octave-error rate. |
| A9 | **Spurious / missing anchor rate** (product) | FP_rate = unmatched est. beats / total est.; FN_rate = unmatched ref / total ref, matching window ±35 ms (half the MIR window) | own, built on `mir_eval.util.match_events` | Each FP becomes a warp anchor that creates a local ratio spike; each FN a stretch of interpolation. |
| A10 | **Anchor precision** (product) | e_i = ŝ_i − s*_i for matched pairs; report median \|e\|, P95 \|e\|, mean e (bias) | own | Only meaningful against sample-accurate ground truth (synthetic, MIDI-rendered) or carefully corrected annotations. Detects frame quantization (Beat This 20 ms frames) and systematic latency. |
| A11 | **Fine tempo error** (product) | B̂ from robust linear regression of beat index on beat time (Theil–Sen or Huber) on stable sections; ε_B = \|B̂ − B*\| in BPM | own (scipy `theilslopes`) | Drift budget: cumulative drift D ≈ T·\|ΔB\|/B. At 110 BPM over 240 s: ΔB = 0.01 → 22 ms; 0.05 → 109 ms; 0.1 → 218 ms (computed). |
| A12 | Drift detection | Local tempo curve B(t) vs ground-truth curve: RMSE over beats, and classification (stable / drift / step / ramp) accuracy | own | Ground truth from synthetic generators or annotation regression. |
| A13 | Confidence calibration | Reliability diagram + Expected Calibration Error of per-song/per-section confidence vs actual correctness (A5 CMLt ≥ 0.8 = "correct") | own | Confidence drives UI warnings; must not be overconfident (Beat This is deliberately overconfident per its paper). |
| A14 | Octave / metric-level error class | Categorical: correct / ×2 / ×½ / ×3 / ×⅓ / off-beat / other — derived from which AML variant wins | own on mir_eval variations | Feeds half/double UI test cases. |

### 2. Corrected-output metrics (render side)

Two measurement paths, always both where possible:

- **Path K (known ground truth, engine fidelity):** source events s*_j are known (synthetic/MIDI). Expected output time is W(s*_j). Measure actual output time ô_j by sample-accurate event localization in y (cross-correlation of the source event waveform against y in a ±30 ms window around W(s*_j), or energy-centroid of the envelope for smeared events). Error e^K_j = ô_j − W(s*_j). Isolates the stretch engine and time-map implementation from the detector.
- **Path R (re-detection, end-to-end):** run beat detection on y, compare to target grid T_k. Error e^R_k = b̂_out,k − T_k. Caveat: if the same detector is used for analysis and verification, errors are correlated (a detector that is consistently late by 8 ms on a kick type is late on input and output) → use (i) a *different* detector or onset-based refinement for verification, and (ii) Path K wherever ground truth exists.

| ID | Metric | Definition | Provisional threshold | Notes |
|---|---|---|---|---|
| O1 | Engine time-map fidelity (Path K) | median \|e^K\|, P95 \|e^K\|, max \|e^K\| | **PROVISIONAL**: median ≤ 1 ms, P95 ≤ 3 ms, max ≤ 5 ms on click/kick trains at ratios 0.90–1.10 | Phase-vocoder latency compensation errors show up here as constant bias. |
| O2 | End-to-end beat alignment (Path R) | median \|e^R\|, P95 \|e^R\|, F(±70 ms) and F(±20 ms) of output beats vs T_k | **PROVISIONAL** synthetic: median ≤ 3 ms, P95 ≤ 10 ms; real music (items with source CMLt ≥ 0.9): median ≤ 10 ms, P95 ≤ 25 ms, F@70 ≥ 0.97 | Real-music limit is the detector's own precision; compare against the source-side A10 baseline, not zero. |
| O3 | Cumulative drift | e^R (or e^K) at last beat; slope of e vs k (linear fit) | **PROVISIONAL** ≤ 5 ms end-of-song on synthetic; slope ≈ 0 (\|slope\| ≤ 0.05 ms/beat) | Catches off-by-rounding grid errors and sample-rate bookkeeping bugs (e.g. 44.1 vs 48 kHz). |
| O4 | Local tempo error | Sliding window of N = 8 output beats; local BPM via regression; ε_loc = \|BPM_loc − B\| | **PROVISIONAL** synthetic P95 ≤ 0.3 BPM; real P95 ≤ 1.0 BPM | Window length trades resolution vs. jitter; with σ = 1 ms beat jitter and IOI 0.5 s, regression BPM noise over 8 beats ≈ 0.02 BPM — threshold dominated by detector noise. |
| O5 | Warp smoothness (map property, not audio) | max \|Δr_i\| between consecutive segments; count of r_i outside [r_min, r_max] | No r outside configured clamp (e.g. [0.8, 1.25]); Δr limited by smoothing policy | Pure unit-test metric on W. |
| O6 | Pitch deviation – pure tones | Long-window (≥ 186 ms, Blackman-Harris, zero-padded) FFT peak with parabolic interpolation per frame; Δc = 1200·log2(f_out/f_in) on frames ≥ 100 ms away from file edges | **PROVISIONAL** median \|Δc\| ≤ 1 cent, max ≤ 5 cents | 5 cents ≈ commonly cited pure-tone JND (≈10 cents above 1 kHz, ~3 cents best case around 2 kHz) → thresholds sit below perception. Do **not** use pYIN default `resolution=0.1` (=10-cent bins) for this. |
| O7 | Pitch deviation – complex / vocal | pYIN (`librosa.pyin`, `resolution=0.01`, fmin C2, fmax C7) on source and output; compare output f0 at t_out with source f0 at W⁻¹(t_out) on frames voiced in both | **PROVISIONAL** median \|Δc\| ≤ 5 cents, P95 ≤ 20 cents | pYIN octave errors and voicing flips contaminate P95; report voicing agreement separately. Formant shift is not measured by f0 — see listening tests. |
| O8 | Transient integrity – attack time | Envelope = RMS (1 ms frames) or \|Hilbert\| low-passed at 500 Hz; attack time = t(90 % peak) − t(10 % peak) per onset; ratio ρ_att = att_out/att_in (onsets matched via W) | **PROVISIONAL** median ρ_att ≤ 1.25, P95 ≤ 2.0 for synthetic kick/snare | PV smearing lengthens attacks. |
| O9 | Transient integrity – pre-echo | E_pre = energy in [−20 ms, −2 ms] before onset relative to [0, +20 ms]; ΔE_pre = E_pre,out − E_pre,in (dB) | **PROVISIONAL** median ΔE_pre ≤ +3 dB | Classic phase-vocoder "pre-echo" signature. Window sizes must be < half the IOI of the densest event class. |
| O10 | Transient integrity – onset sharpness | Peak height and FWHM of spectral-flux novelty (log-magnitude, 512-sample hop at 44.1/48 kHz) at each matched onset; ratio out/in | **PROVISIONAL** height ratio ≥ 0.8, FWHM ratio ≤ 1.3 | Also local crest factor (peak/RMS over ±25 ms) ratio ≥ 0.85. |
| O11 | Artifact proxies (objective, *non-gating*) | (a) Sine residual SINAD: fit A(t)·sin(ωt+φ(t)) with slowly varying A, subtract, residual power vs signal (dB). (b) Two-tone beating test (440 + 446 Hz): modulation depth preservation at 6 Hz. (c) Noise spectral flatness change (white/pink noise in → out). (d) Spectral-flux "warble" index: variance of per-bin magnitude derivative on stationary tones. (e) Roberts–Paliwal TSM measures (research-only, after legal review). | Regression-only: alert if worse than golden baseline by > 1 dB (a) / > 10 % (b–d) | None of these predict perceived quality reliably on music; they detect *regressions*. Perceived quality is decided by listening tests (SUBJECTIVE_LISTENING_TESTS.md). |
| O12 | Full-reference quality where a true reference exists | For MIDI-rendered material, render the *same MIDI at the target tempo* → a genuinely time-aligned reference; then ViSQOL v3 audio mode (Apache-2.0) or PEAQ/(open implementations) between y and the native render | Regression + engine ranking only | Only path in which double-ended metrics are valid on TSM output without the alignment hacks Roberts & Paliwal had to use. |
| O13 | Stereo integrity | (a) Mono invariance: input L≡R ⇒ max\|L_out−R_out\| = 0 (or ≤ 1e-7). (b) Polarity: L = −R ⇒ max\|L_out + R_out\| ≤ 1e-7·peak. (c) ITD: GCC-PHAT lag on stereo-delayed noise bursts; \|ΔITD\| ≤ 1 sample. (d) ICC: Pearson ρ(L,R) per 50 ms block, \|Δρ\| median ≤ 0.02. (e) M/S energy ratio change ≤ 0.5 dB. (f) Inter-channel phase difference per STFT bin, magnitude-weighted circular mean error ≤ 5°. | thresholds **PROVISIONAL** except (a),(b) which are hard requirements if the engine processes channels jointly | Independent per-channel stretching breaks (c)–(f); this is exactly the drum-multitrack phase problem from the baseline log. |
| O14 | Peak / true peak / clipping | Sample peak (dBFS), true peak (dBTP, ITU-R BS.1770-5 Annex 2, 4× oversampling at 44.1/48 kHz), count of samples \|y\| ≥ 1.0 before integer conversion, count of clipped samples after conversion | Hard: no silent clipping — any clip must be reported to user/export log; True-peak increase vs source reported (PV can raise peaks by re-phasing) | Use `pyloudnorm` (MIT) only for loudness; implement true-peak per BS.1770 Annex 2 or verified library. |
| O15 | Discontinuity / click detection | Residual r[n] of a high-order LPC predictor (order 32, 20 ms blocks) on y; flag n where \|r[n]\| > k·MAD(r) (k≈8) and the corresponding source region (W⁻¹) is not flagged | Hard on synthetic sines/noise: zero flags. Real music: flags ≤ source flags + tolerance; manual review | Block-boundary bugs in chunked/multithreaded rendering appear here first. Test at every chunk boundary explicitly. |
| O16 | Silence / NaN / Inf / denormal | Any NaN/Inf ⇒ fail. Unexpected silence: run of \|y\| < −120 dBFS longer than 50 ms where W⁻¹-mapped source RMS > −60 dBFS ⇒ fail. DC offset change > 1e-4 ⇒ warn | Hard | |
| O17 | Length correctness | N_out vs N_expected = round(W(N_in/sr)·sr) under the declared tail policy | Hard: \|N_out − N_expected\| ≤ 1 sample | Also verify sample rate, channel count, bit depth in exported header. |
| O18 | Identity / transparency | W ≡ identity (ratio 1 everywhere): output vs input null-test residual | If engine bypass at r=1: bit-exact. If processed: residual ≤ −60 dBFS RMS **PROVISIONAL**, and ABX-transparent (listening) | "Stable 110 BPM song stays effectively unchanged" acceptance test. |
| O19 | Export integrity | Re-decode exported file; WAV/FLAC: sample-exact vs internal buffer after documented conversion (dither seed fixed in tests); MP3: gapless-corrected offset 0 ± 1 sample, length within encoder padding policy | Hard | MP3 encoder/decoder delay (LAME encoder delay 576 + decoder 529 = 1105 samples, ≈25 ms @ 44.1 kHz, Med confidence) silently shifts every beat if gapless info is ignored. |

### 3. Performance metrics (see PERFORMANCE_BENCHMARKS.md)
P1 analysis RTF = wall_time / audio_duration; P2 render RTF; P3 preview latency (edit → audible); P4 peak RSS / private bytes; P5 UI main-thread max stall; P6 cancel latency; P7 decode RTF; P8 export RTF. All reported as median of ≥5 runs, warm and cold cache separately.

### 4. Aggregation and reporting rules
- Per-item metrics → per-category means **and** worst-item; the worst item is what the user hears.
- Never average across tiers (synthetic vs real) into one number.
- Bootstrap 95 % CI (1000 resamples over items) for every corpus-level mean.
- Report detector and engine metrics separately; an end-to-end failure must be attributable (Path K pass + Path R fail ⇒ detector; Path K fail ⇒ engine/time-map).

## Alternatives Considered
- **Only standard MIR metrics:** rejected — ±70 ms tolerance hides warp-relevant errors.
- **PEAQ/ViSQOL as primary quality metric:** rejected — require time-aligned reference; alignment by spectral interpolation gave only modest correlation (Roberts & Paliwal); valid only on MIDI-native references (O12).
- **Train our own TSM quality predictor:** deferred (YAGNI for MVP); possible later using the open TSM subjective dataset (IEEE DataPort, ~4.4 GB, licence not stated → legal review).
- **pYIN for all pitch checks:** rejected for pure tones (10-cent default bins, estimator noise); FFT peak interpolation is exact enough and dependency-light.

## Evidence / Sources
- mir_eval beat API & source (defaults, trimming, normalized IG): https://mir-eval.readthedocs.io/latest/api/beat.html ; https://github.com/mir-evaluation/mir_eval/blob/main/mir_eval/beat.py (lines 585–647) — High
- mir_eval tempo detection (tol 0.08, two tempi): https://mir-eval.readthedocs.io/latest/api/tempo.html — High
- mir_eval 0.8.2, MIT, 2025-02-25: https://pypi.org/project/mir_eval/ — High
- Beat This! (frame rate, GTZAN Table 2, non-periodic beats): https://arxiv.org/abs/2407.21658 — High
- Acc1/Acc2 definitions: ISMIR 2004 tempo contest https://mtg.upf.edu/ismir2004/contest/tempoContest/node7.html ; Gouyon et al. 2006 — High
- TSM objective measures: https://arxiv.org/abs/2006.06153 (PCC 0.865), https://arxiv.org/abs/2009.02940 (single-ended PCC 0.794), dataset https://arxiv.org/abs/2006.00848 — High; repo https://github.com/zygurt/TSM (no licence, GitHub API) — High
- ViSQOL Apache-2.0: https://github.com/google/visqol — High
- True peak BS.1770-5 (2023-11): https://www.itu.int/rec/R-REC-BS.1770-5-202311-I — High
- librosa.pyin parameters: Context7 `/websites/librosa_doc` — High
- Pitch JND figures: https://en.wikipedia.org/wiki/Just-noticeable_difference , HyperPhysics — Med (secondary)
- LAME delay figures: common knowledge, **UNVERIFIED** against LAME docs in this pass.

## Recommendation
Adopt the metric set above. Gate releases on: A5 (CMLt), A9, A10, A11 for analysis; O1, O2, O3, O13(a,b), O14–O19 for rendering (hard or provisional thresholds); treat O6–O12 as calibrated regression detectors; decide perceptual quality only with listening tests.

## Risks
- Provisional thresholds may be unattainable with the chosen engine at some ratios → calibrate, do not guess.
- Path R correlation with the analysis detector can mask detector bias.
- Real-music annotations are themselves noisy (Beat This! authors list GTZAN annotation errors) → real-music thresholds must be lower-bounded by annotation quality.

## Open Questions
- Which verification detector (independent of the production detector) is licence-clean for internal use? (librosa onset/beat is ISC — usable; madmom models are CC BY-NC-SA — internal-use legality needs review.)
- Does the chosen engine process stereo channels jointly (O13 a/b as hard requirements)? Rubber Band has a channels-together option; Signalsmith behaviour **UNVERIFIED**.
- Tail policy for O17 (keep reverb tail beyond last beat or cut to grid?) — architecture decision.

## Implementation Implications
- A dev-only Python evaluation package (`tools/eval/`: numpy, scipy, soundfile, mir_eval, librosa, pyloudnorm) computes all metrics from WAV + JSON sidecars; it is never shipped in the product.
- Engine must expose (or the test harness must reconstruct) the exact W used for a render, serialized with the output, so Path K can be computed.
- All metrics emitted as machine-readable JSON (`metrics.json`) per item for golden comparison and trend dashboards.
