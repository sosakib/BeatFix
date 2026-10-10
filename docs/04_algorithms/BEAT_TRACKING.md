# Beat Tracking

Status: algorithm spec, 2026-10-08. Decision: ADR-003 ([DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1). Sources: [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) F1–F10, §5, §7, §10, §12; [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) §2–4.

## Objective
Turn decoded audio into a **dense, continuous, single-metrical-level** sequence of beat anchors `S_i` (float64 s), each with a confidence `c_i ∈ [0,1]` and flags, suitable as warp anchors ([WARP_FUNCTION.md](WARP_FUNCTION.md)).

## Why It Matters
Failure costs are asymmetric (BEAT_TRACKING_RESEARCH, "Why It Matters"). A consistent octave or off-beat error is cheap (relabel). A **continuity error** (level or phase switch mid-song) or a missing/extra beat creates a local ratio of ~2× or ~0.5×, which is an audible lurch or a refused render. Per-beat jitter is copied into the output in Strict mode (WARP_FUNCTION §4.1). Standard MIR tolerance (±70 ms) is ~14 % of a beat at 120 BPM, far too lax: BeatFix targets continuity plus ≲ 5 ms anchor jitter (target, to validate).

## Architecture (decided)
```
         audio (native sr, f32)
           │
   ┌───────┴───────────────────────────┐
   ▼                                   ▼
 Beat This! front-end (§1)          DSP layer (ONSET_DETECTION.md)
 22.05 kHz mono → log-mel → ONNX     HPSS + SuperFlux + band ODFs
   │ p_beat, p_down @ 50 fps            │ obs_dsp @ 100 fps, odf_hr
   │ (persisted, §8)                    │
   ▼                                   │
 tempo candidates + level choice ◄──────┤   HALF_DOUBLE_TIME_RESOLUTION.md
   │ c1, margin                         │
   ▼                                   │
 corridor Viterbi period path τ(t) (§2) │
   ▼                                   ▼
 DP decoder on p_beat (§3)         DP decoder on obs_dsp → B_dsp (cross-check, §5)
   ▼
 repair (OUTLIER_REJECTION.md) → sub-frame refinement (§4, WARP_FUNCTION §5)
   ▼
 per-beat confidence c_i (§5) ──► DOWNBEAT_ESTIMATION, TEMPO_STABILITY_ANALYSIS, BPM_ESTIMATION
```
One ML model produces frame activations. **Everything after the activations is owned, deterministic C++** (no madmom, no DBN at runtime: dependency, fixed 55–215 BPM range and a patent claim for legal review, BEAT_TRACKING_RESEARCH §2.3).

## 1. Beat This! activation front-end
| Item | Value | Source |
|---|---|---|
| Model | Beat This! `final0` (~20 M params, ~80 MB fp32) default; `small0` (~2 M) if the Phase 3 perf gate fails | DECISION_SUMMARY §1, F2 |
| Licence | MIT code + weights. Training-data compatibility: **for legal review** | F1 |
| Runtime | ONNX Runtime 1.30 CPU EP, telemetry disabled at startup; our own ONNX export from the official checkpoint in CI, verified against PyTorch | F8 |
| Input audio | mono = (L+R)/2, resampled to 22 050 Hz with the analysis-tier resampler ([RESAMPLING.md](RESAMPLING.md)) | F2, §10 |
| Spectrogram | STFT n_fft 1024, hop 441 (**50 fps**), 128 mel bands 30 Hz–10 kHz, `ln(1 + 1000·x)` | F2 |
| Chunking | 1500-frame chunks (30 s), 6-frame border, overlap mode `keep_first` | F9 |
| Output | frame-wise beat and downbeat **logits** at 50 fps; `p = sigmoid(logit)` | F2 |

Rules:
- Preprocessing details not listed above (mel normalisation, magnitude vs power, padding, frame-centre convention) are **copied from the reference `beat_this` preprocessing and inference code**, not reinvented. The CI parity test (T-BT-1) is the guard.
- The reference implementation resamples with soxr (LGPL). We use r8brain (DECISION_SUMMARY §1). The resulting activation difference is expected to be at D2/D3 level, not bit-level. **UNVERIFIED**: measured by T-BT-1 on the parity corpus.
- Frame time convention: `t_k = k / 50` s plus a constant convention offset fixed by the click-track test (T-BT-2). Any constant offset is harmless to warping. It must be the same in analysis and display, and it is absorbed by the global offset in §4.
- The model never sees the render path. Its 22.05 kHz buffer is discarded after inference.

## 2. Local period path τ(t) (decoder prior)
The decoder needs a period prior before beats exist. Defined in [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) §4 and summarised here:
```
G      = log-BPM tempogram of p_beat (ACF × Fourier, 1 % bins, 10 s window, 0.5 s hop)
corr   = [c1 · 2^−0.4, c1 · 2^+0.4]            # ≈ −24 % / +32 % around the chosen level c1
obs    = log(ε + G / max_b G)   on corr bins only
trans  = −(Δbins / 1.5)² / 2 − 8 · [|Δbins| > 6]     # nats per 0.5 s hop
path   = viterbi(obs, trans)                    # float64; ties → lowest bin
τ(t)   = 50 · 60 / BPM(path(t)), linear interpolation to frame rate
```
- **Why a corridor of 2^±0.4 and not a literal ±40 %**: the corridor spans 0.8 octave (< 1 octave), so no two tempi inside it are in a 2:1 relation and the decoder **cannot** switch metrical level by octave. A literal 0.6c–1.4c corridor spans 1.22 octaves and would allow exactly the octave jump it is meant to prevent. A 3:2 jump (0.58 octave) is possible inside the corridor and is suppressed by the jump penalty J = 8 nats.
- Re-centring the corridor per section is allowed only for a confirmed level-switch section ([TEMPO_STABILITY_ANALYSIS.md](TEMPO_STABILITY_ANALYSIS.md) §5). MVP renders always use one level.
- Regions with no pulse give near-flat observations, so the path carries the neighbouring tempo through intros and breakdowns. This is intended.

## 3. Owned DP decoder (time-varying Ellis DP)
Objective over beat frames {b_j} (Ellis 2007, period made time-varying):
```
C = Σ_j O(b_j) + Σ_j α · F(b_j − b_{j−1}, τ(b_j)),     F(Δ, τ) = −(ln(Δ/τ))²
```
`O = p_beat / std(p_beat)` (unit-std normalisation as in Ellis), α = 100 (librosa default; placeholder, calibrated on the corpus).
```
dp_decode(O[0..N), τ[0..N), α):
    S = copy(O); back = −1
    for t in 0..N−1:
        lo = t − round(2.0·τ[t]);  hi = t − round(0.5·τ[t])
        best = −∞
        for u in max(0, lo) .. hi:                        # ascending ⇒ ties keep lowest u
            v = S[u] − α · ln((t − u)/τ[t])²
            if v > best: best = v; arg = u
        if best > 0: S[t] = O[t] + best; back[t] = arg
    end = argmax_{t ∈ last round(τ) frames, O[t] local max} S[t]   # ties → lowest t
    return backtrack(end, back)                            # frame indices, ascending
```
- Complexity ≈ N·1.5τ ≈ 12 000 × 40 ≈ 5·10⁵ operations for 4 min at 50 fps: negligible. Re-decoding after a user interpretation edit is sub-second.
- Search window [τ/2, 2τ] lets the DP bridge a missing activation (cost α·ln²2 ≈ 48 at α = 100). The repair stage then re-inserts the missing beat ([OUTLIER_REJECTION.md](OUTLIER_REJECTION.md) §2).
- All arithmetic in float64. Deterministic argmax with lowest-index tie-breaking, so ~1e-5 activation perturbations (cross-machine ORT differences) can only move a decision that was already a near-tie (BEAT_TRACKING_RESEARCH §10).
- Beat This minimal post-processing (max-pool ±3 frames, p > 0.5) is computed as `B_min` for diagnostics only. It has no continuity model (F3, F4) and never produces anchors.

## 4. Sub-frame refinement (summary)
Fully specified in [WARP_FUNCTION.md](WARP_FUNCTION.md) §5. Order and rules:
1. Parabolic interpolation on the beat **logit** around the decoded frame k: `δ = 0.5(a[k−1] − a[k+1]) / (a[k−1] − 2a[k] + a[k+1])`, |δ| ≤ 0.5, `t = (k + δ)/50`. Removes the 20 ms quantisation (uniform, σ ≈ 5.8 ms).
2. Optional transient snap within ±25 ms on `odf_hr` in the single band frozen for the song ([ONSET_DETECTION.md](ONSET_DETECTION.md) §4.4), only if the peak salience exceeds the threshold **and** no competing peak with ≥ 80 % of its salience lies within ±10 ms (flams: keep the ML time, flag `flam`, lower confidence).
3. Global offset: median(onset peak − interpolated activation peak) over confident snaps, applied as one constant shift to **all** anchors.

Deliberately not used: the research option "snap only if it reduces the residual against the smoothed curve" (§5.2). It would bias anchors toward the smooth curve, hide real drift and groove from [TEMPO_STABILITY_ANALYSIS.md](TEMPO_STABILITY_ANALYSIS.md), and duplicate what the Huber IRLS smoother already does.

## 5. Confidence
### 5.1 Per-beat confidence c_i
Features per decoded beat i (BEAT_TRACKING_RESEARCH §7):

| Feature | Definition |
|---|---|
| `a_i` | beat probability at the refined peak |
| `s_i` | salience: `a_i − median p_beat over ±½ IOI` (rejects flat-high regions) |
| `agree_i` | `exp(−d_i² / (2·(20 ms)²))`, d_i = distance to the nearest `B_dsp` beat, 0 if none within ±35 ms |
| `res_i` | `|S_i − x_i| / σ̂`, x = Whittaker/IRLS smooth at N_c = 8, σ̂ = 1.4826·MAD of residuals |
| `ioi_i` | `|ln(IOI_i / P_loc,i)|`, P_loc from OUTLIER_REJECTION §1 |
| `snap_i` | 1 if a clean percussive onset was snapped (§4.2), else 0 |

```
c_i = σ( w0 + w1·a_i + w2·s_i + w3·agree_i − w4·min(res_i, 5) − w5·ioi_i + w6·snap_i )
```
- Two-pass to avoid circularity: pass 1 computes `c_i^pre` without `res_i` and uses it as smoother weights; pass 2 computes `res_i` and the final `c_i`.
- Weights start hand-set (placeholder `w = [−2, 2, 2, 2, 0.6, 3, 0.5]`) and are then fitted by logistic regression on the annotated corpus (label: beat within ±15 ms of the annotation). This 7-parameter linear model is the only "learned" piece besides the activation model.
- Overrides: `inferred` beats (OUTLIER_REJECTION) get `c_i = min(c_i, 0.2)`; beats in damaged decode spans `min(c_i, 0.3)`.
- `c_i` is the smoother weight `w_i` in [WARP_FUNCTION.md](WARP_FUNCTION.md) §4.2 and drives the dotted-marker display (UX_BEHAVIOR.md §4.5).

### 5.2 Global beat-tracking scores (inputs to BPM confidence)
| Score | Definition |
|---|---|
| `agreement A` | F-measure between B and `B_dsp` at ±50 ms (same level). Mean mutual agreement predicts difficulty (Holzapfel et al. 2012, Med confidence) |
| `fracHigh` | fraction of beats with c_i > 0.7 |
| `longestLowRun` | longest run of beats with c_i < 0.4, in beats |
| `fracInferred` | fraction of inferred beats |
| `pathScore` | DP score per beat, and margin vs a forced decode at the competing level (HALF_DOUBLE §4) |

[BPM_ESTIMATION.md](BPM_ESTIMATION.md) §7 combines these with the octave margin and phase coherence R into **High / Medium / Low**. This doc defines **None**: no steady beat, when fewer than 16 decoded beats have c_i > 0.5, or `pulsePresence` (ONSET_DETECTION §6) is below threshold for > 70 % of the song (placeholders). With None, no warp map is built (UX_BEHAVIOR.md §4.4).

## 6. Fallback DSP path
Triggered by: model file missing or hash mismatch, ORT session or inference error, or the user forcing "DSP analysis".
```
obs  = obs_dsp (ONSET_DETECTION §4.6), 100 fps
cand = tempo candidates on obs (TEMPO research §2)
c1   = rank levels with features {sal, bb, mid, prior, ioi}  (no act, no down)
τ    = corridor path on the obs tempogram
B    = dp_decode(obs, τ, α) → repair → refine (snap only, no logit interpolation: use ODF peak interpolation)
downbeats from the backbeat pattern only (DOWNBEAT_ESTIMATION §6)
global confidence capped at Medium; analysis record marked "DSP mode"; UI says so
```
Expected weaker on octave choice (DSP Acc1 0.66 vs TCN 0.83 on GTZAN, F5) and on weak-onset material. The same decoder, repair, refinement and confidence code are used, so tests cover both paths.

## 7. Failure modes
| Failure | Symptom | Detection | Handling |
|---|---|---|---|
| Confident-but-wrong activations (expressive music, SMC-like) | regular but wrong beats | low agreement A with `B_dsp`, high res_i scatter | Low confidence, user confirms (F7) |
| Non-periodic activations in complex passages (F4) | gaps, extra peaks | DP continuity + repair | repaired/inferred flags |
| Octave choice wrong but consistent | displayed BPM ×2/÷2 | margin Amber/Red | one-click relabel (HALF_DOUBLE) |
| Mid-song level switch | IOIs halve/double for ≥ 8 beats | impossible inside the corridor; checked by OUTLIER_REJECTION §3 | re-decode, flag |
| Drumless intro / outro | flat activation | pulsePresence low | path carries tempo, beats `inferred` |
| Fade-out | weak activation | p_beat below threshold ≥ 2 bars | drop trailing anchors, edge ratio (WARP_FUNCTION §6) |
| ML timing bias per instrument | constant offset vs transient | global offset median | one constant shift, never per-beat |
| MP3 priming offset | constant offset | decoder gapless test (Phase 1 gate) | identical decode path for analysis and render |
| Model unavailable | no activations | §6 trigger | DSP fallback, capped Medium |
| Tempo outside 40–250 BPM | wrong level forced | candidate range | user types target; Post-MVP extension |

## 8. Determinism and persistence
- ORT CPU is not bit-exact across machines or thread counts (F10). So analysis is **computed once and stored**. The project persists: beat and downbeat logits (float16, 50 fps, ≈ 48 KB per 4 min), model SHA-256, ONNX export script version, preprocessing version, analysis resampler id, decoder/repair/refinement parameters (`analysisAlgoVersion`), decoded beats with c_i and flags.
- Re-analysis is an explicit user action (UX_BEHAVIOR.md §3.6). Opening a project never re-runs the model. Interpretation edits (×2/÷2, bar 1, meter) re-run §2–§5 from the persisted activations (deterministic, sub-second), never §1.
- Golden/reference runs: `intra_op_num_threads = 1`, graph optimisation level fixed. Production may use more threads because outputs are persisted.
- Determinism levels per [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) §6: D1 on the same machine, D2 (beat times ±0.5 ms) across machines.

## Alternatives Considered
| Option | Verdict | Why |
|---|---|---|
| Beat This + madmom DBN | Reject at runtime | Python dependency, fixed 55–215 BPM and 3/4 meters, lower F1, unresolved patent claim (legal review) |
| Beat This minimal post-processing as output | Reject | No continuity, frame-quantised |
| Pure DSP tracker | Fallback only | Octave and weak-onset robustness clearly worse (F5) |
| All-In-One (Demucs + NATTEN) | Post-MVP watch | Dependency weight, unverified weight licence |
| madmom RNN/TCN models | Reject | CC BY-NC-SA model files |
| Essentia | Reject | AGPL |
| Masked-diffusion / AlignBeat / BeatFM | Watch-list | No verified code or licence (F14). The activation model stays swappable behind `ActivationModel` |

## Open Questions
1. Measured Beat This localisation error (ms) on clean electronic material: decides how often the transient snap matters (research OQ4).
2. CPU time of `final0` vs `small0` on the reference laptop (Phase 0 spike, perf gate).
3. Does a 3-seed activation average (final0/1/2) improve continuity enough to justify 3× inference? Post-MVP experiment.
4. Is ORT CPU bit-deterministic run-to-run with fixed threads on Windows? (DSP_TEST_STRATEGY open question.)

## Implementation Implications
- `ActivationModel { run(mel) → (beatLogits, downLogits) }` with `BeatThisOnnx` and `DspPseudoActivation` implementations (two real implementations, so the interface is justified).
- Analysis artefact: `{versions, modelHash, activations(f16), beats[{t, conf, flags}], B_dsp summary, globalScores}`. Field names are owned by PROJECT_SESSION_MODEL.md.

## Tests
| ID | Assertion |
|---|---|
| T-BT-1 Parity | our ONNX + C++ preprocessing vs the Python reference on ≥ 10 fixed clips: max |logit diff| ≤ 1e-3 (placeholder; set after first run), decoded beat sets identical at ±1 frame |
| T-BT-2 Offset | S01(c) synthetic kick at 110/120/128 BPM: after refinement, mean error (bias) ≤ 1 ms, median |e| ≤ 2 ms (A10) |
| T-BT-3 Continuity | S03 ramps, S04 steps (except the 128→64 switch): CMLt = 1.0, no level switch |
| T-BT-4 Corridor | S01 at 120 BPM with 25 % random activation dropouts: decoded IOIs never leave [0.75, 1.33]·τ, A9 FN ≤ 1 % after repair |
| T-BT-5 Ghosts | S07 16th ghosts at −18 dB: A9 FP ≤ 2 % |
| T-BT-6 Fallback | model file removed: analysis completes in DSP mode, confidence ≤ Medium, CMLt ≥ 0.9 on S08 rock backbeat |
| T-BT-7 Determinism | same machine, threads = 1: beats bit-identical (D1); two thread counts: beats within ±0.5 ms (D2) |
| T-BT-8 Persistence | open a saved project with the model file deleted: beats load and the warp is built; no inference attempted |
| T-BT-9 None | S14 silence and S10 pad-only: global confidence None, no warp map |
| T-BT-10 Confidence calibration | corpus: ECE of c_i (A13) reported per release; c_i monotone decreasing with injected jitter σ on S05 |
