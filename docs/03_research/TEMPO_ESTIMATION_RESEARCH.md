# Tempo Estimation, Octave Resolution & Drift Analysis — Research (Agent B)

Date: 2026-10-08 · Author: Research Agent B · Status: research input
Companion: `BEAT_TRACKING_RESEARCH.md` (activations, decoding, refinement, confidence).
**Does not repeat** math already specified by the orchestrator in:
- `04_algorithms/BPM_ESTIMATION.md`: global BPM via Huber-IRLS line fit and Theil–Sen, SE formula, block-bootstrap CI, integer/fractional policy, local BPM from the Whittaker-smoothed anchors, lag-1 IOI autocorrelation signature.
- `04_algorithms/WARP_FUNCTION.md` §4–5: Whittaker/IRLS smoother, λ ↔ cutoff N_c (Natural/Strict), sub-frame refinement.
- `04_algorithms/TARGET_BEAT_GRID.md`: grid equation, phase policies, circular-statistics phase and R.

This document adds: periodicity front-ends, tempo **candidate generation and metrical-level ranking** (half/double/third), the **time-domain local period path** that drives beat decoding, and the **statistical separation of drift, steps, splices and detector noise**.

## Objective

1. Generate tempo hypotheses robustly (multi-hypothesis, 40–250 BPM).
2. Choose the metrical level (55 vs 110, 70 vs 140, 110 vs 220, 3:2 cases) with an explicit margin and ask the user when it is ambiguous.
3. Supply a smooth, octave-stable local period τ(t) for the beat decoder.
4. Classify each song or region as Stable, Drifting, Stepped (tempo change), Spliced (phase jump), or Unreliable, without confusing detector noise with drift.

## Why It Matters

- Octave choice sets the displayed BPM and the default target. The guard in `TARGET_BEAT_GRID.md` §2 (|log2(B/B̂)| > 0.6 ⇒ re-interpret level) relies on it. A *consistent* octave error is cheap to fix in the UI. An *inconsistent* one (a level switch) is catastrophic (see the beat doc, "Why It Matters").
- The local period path is the prior that prevents the decoder from following fills or ghost notes.
- Drift classification decides what the warp should do. Smooth drift gets the Whittaker correction. A tempo step needs a segment boundary in the smoother. A splice or phase jump can't be fixed by any smooth warp and must be surfaced to the user.

## Key Findings

| # | Claim | Evidence / source | Conf. | Implication | Recommendation |
|---|---|---|---|---|---|
| T1 | ML multi-task tempo clearly beats DSP on octave-correct accuracy. Measured Acc1/Acc2 (±4 %): Böck & Davies 2020 TCN scores GTZAN .830/.950, ACM Mirum .841/.990, GiantSteps .870/.965. Percival & Tzanetakis 2014 (DSP) scores GTZAN .658/.924, Mirum .733/.972, GiantSteps .506/.956. Schreiber & Müller 2018 CNN scores GTZAN .769/.926. | Böck & Davies ISMIR 2020 Table 1 (http://archives.ismir.net/ismir2020/paper/000223.pdf) | High | DSP finds the period *up to octave* nearly as well (Acc2 0.92–0.97). The gap is the octave (Acc1 −0.17 on GTZAN). | Periodicity from DSP + ML activation. Octave choice from ML-informed features plus musical evidence. |
| T2 | Even SOTA is ~83–87 % Acc1, so octave errors remain in roughly 1 song in 6–8 on generic data. | same | High | The UI must make x2 / ÷2 one click. Confidence margin must be exposed. | Multi-hypothesis output with margin. Amber/Red ⇒ ask the user. |
| T3 | Fixed tempo floors cause octave errors: the madmom DBN floor of 55 BPM forces double tempo on 21 % of SMC tracks. The authors recommend multi-hypothesis tempo. | arXiv 2605.12287 | Med-High | A narrow range is a systematic bias, not a safety net. | Internal candidate range 40–250 BPM. **Note: conflicts with `BPM_ESTIMATION.md` §6 range 50–220**, so I propose widening it (see Recommendation 2). |
| T4 | Pulse salience peaks near ~2 Hz (≈120 BPM). The resonance model of van Noorden & Moelants (1999) uses a damped oscillator at 2 Hz. In tapping studies, McKinney & Moelants (2006) found most excerpts elicited ≥ 2 perceived metrical levels, with accent distribution driving "extreme" choices. | Search summaries of van Noorden & Moelants 1999 and McKinney & Moelants 2006 (RPPW abstract https://cspeech.ucd.ie/rppw/rppw10/mckinney/abstract.html) | Med | A perceptual prior is legitimate but weak. Real songs are often ambiguous for humans too. | Use a log-normal prior centred on 120 BPM (σ ≈ 1 octave) as a tie-breaker only. Musical evidence (backbeat, downbeats) dominates. |
| T5 | librosa's tempo and PLP functions accept a prior distribution. The documentation demonstrates `lognorm(loc=log(120), scale=120, s=1)` and uniform priors, and the uniform prior changes the result (143.6 → 161.5 BPM in the docs example). | Context7 `/websites/librosa_doc` (feature.tempo, beat.plp) | High | Demonstrates the prior's leverage on ambiguous material. | Prior weight is a calibrated parameter, logged with each analysis. |
| T6 | Lag resolution of frame-based periodicity is coarse: ΔBPM per lag step = BPM²/(60·fps). At 120 BPM this is 2.4 BPM at 100 fps and 4.8 BPM at 50 fps. | Derived | High | Tempograms are only good for candidates and paths, not final BPM. | Final BPM comes from beat regression (`BPM_ESTIMATION.md` §3). Tempograms use parabolic lag interpolation and log-spaced bins. |
| T7 | Comb-filter resonators (`y(t) = x(t) + α·y(t−τ)`) on a beat activation give robust tempo histograms. madmom provides `CombFilterbankProcessor` and `TempoEstimationProcessor` (methods acf/comb/dbn; activation smoothing, histogram smoothing, interpolate option). | Context7 `/cpjku/madmom`. Böck, Krebs, Widmer ISMIR 2015 "Accurate tempo estimation based on RNNs and resonating comb filters" (α ≈ 0.79, not re-fetched) | Med-High | A simple, deterministic, reimplementable algorithm (no licence issue for our own code). | Use comb or ACF on the **Beat This activation** for candidates. |
| T8 | Autocorrelation and Fourier tempograms have opposite octave biases. ACF reinforces sub-harmonics of the period (2τ, 3τ ⇒ T/2, T/3). The Fourier tempogram reinforces harmonics (2T, 3T, the tatum). Combining them suppresses spurious levels. | Grosche & Müller, IEEE TASLP 2011 (PLP); Peeters 2007 (EURASIP JASP) template/combination approach. Not re-fetched. | Med | Cheap octave-error reduction. | Candidate salience = geometric mean of normalised ACF and Fourier tempogram values. |
| T9 | AI-music drift is anecdotally slow (ramps of a few %, section offsets), with possible steps and phase jumps at continuation seams. Suno officially acknowledges drift. | Suno help 8363457. Anecdotes and vendor example 206→214 BPM over 3 min (aislice). See the beat doc F11–F12. | Low (magnitudes) / High (existence) | Must support ±5–10 % slow drift, steps and splices. Thresholds must come from our corpus. | Stability classifier with four non-stable classes. Annotated Suno corpus is a prerequisite for threshold freezing. |
| T10 | Over a whole song, drift is far easier to detect than per-beat noise suggests. A linear tempo ramp of relative size ε over N beats (period p0) leaves a residual of amplitude ≈ ε·p0·N/12 after the best constant-tempo fit. With N = 400 and p0 = 0.5 s, ε = 0.1 % gives 17 ms and ε = 0.5 % gives 83 ms, versus detector σ ≈ 5–10 ms. | Derived (L2 fit of a quadratic on [0, N]: max residual = a·N²/6 with a = ε·p0/(2N)) | High | "Is there drift?" is easy. "Where, and how fast?" is the hard, windowed question. | Use global tests for presence, windowed tests for localisation (§5). |

## Technical Details

### 1. Periodicity front-ends (time domain)

Input novelty signal: preferably the **Beat This beat probability** p_beat(t) (50 fps). Fallback: HPSS-percussive SuperFlux ODF (100 fps), mean-removed and half-wave rectified.

| Method | Definition (window w, lag ℓ, tempo f in Hz) | Bias / property | Cost |
|---|---|---|---|
| Global ACF | `A(ℓ) = Σ_t x(t)·x(t+ℓ)` (biased, normalised) | Peaks at ℓ = kτ ⇒ favours slower levels | O(N log N) via FFT |
| ACF tempogram | `A(t,ℓ) = Σ_n x(n)w(n−t)·x(n+ℓ)w(n+ℓ−t)`, window 8–12 s | Local version of the above. librosa default `win_length=384` frames | small |
| Fourier tempogram | `F(t,f) = |Σ_n x(n)·w(n−t)·e^{−2πi f n/fps}|`, f on a BPM/60 grid | Peaks at f, 2f, 3f ⇒ favours faster levels | small |
| Comb resonators | `y_τ(t) = x(t) + α·y_τ(t−τ)`, α ≈ 0.79. Histogram H(τ) = Σ_t [τ = argmax_τ' y_τ'(t)]·max | Robust to missing onsets. Mild sub-harmonic bias | O(N·#τ) |
| PLP (Grosche & Müller 2011) | Keep the max-magnitude Fourier tempogram component per frame, overlap-add the windowed sinusoids, half-wave rectify | Gives a *local pulse curve* that follows tempo changes. librosa `beat.plp` supports a prior | small |
| CNN/TCN tempo head | Classifier over 0–300 BPM (Böck 2020; Schreiber & Müller 2018) | Best Acc1. Needs a licensed model | n/a (no MIT model identified) |

Tempo axis: log-spaced bins, 1 % spacing from 40 to 250 BPM (≈ 184 bins). Each bin's value comes from interpolating the lag-domain function. This avoids T6's coarse high-tempo resolution and makes octave relations exact offsets (+69.7 bins per octave at 1 %).

### 2. Candidate generation (multi-hypothesis)

```
tempo_candidates(p_beat, odf):
  x   = p_beat if model_ok else odf
  G_a = acf_tempogram(x, win=10 s, hop=0.5 s)      -> resampled to log-BPM bins
  G_f = fourier_tempogram(x, win=10 s, hop=0.5 s)  -> same bins
  G   = sqrt(norm(G_a) * norm(G_f))                # T8: octave-bias cancellation
  g   = median_over_time(G)                        # robust global salience (median ignores breakdowns)
  peaks = top_k_local_maxima(g, k=5, min_sep=4 %)
  # close the set under metrical relations so the true level is always present
  C = { c·m : c in peaks, m in {1/3, 1/2, 2/3, 1, 3/2, 2, 3} } ∩ [40, 250]
  merge candidates within 2 %
  return C with salience g(c) (interpolated)
```
The ×3 and ÷3 relations cover 6/8 and 12/8 and triplet-feel confusions. `BPM_ESTIMATION.md` §6 currently lists {B̂, B̂/2, 2B̂, 2B̂/3, 3B̂/2}. **I recommend adding 3B̂ and B̂/3**, gated by the range.

### 3. Metrical-level ranking (half / double / third resolution)

For each candidate c, decode a provisional beat sequence at that level (cheap DP from the beat doc §12, with a constant τ = fps·60/c) and compute:

| Feature | Definition | Why it discriminates |
|---|---|---|
| `sal(c)` | Combined tempogram salience at c (§2) | Periodic energy at this level |
| `act(c)` | Mean Beat This activation at the decoded beats, minus the mean at mid-beat positions | ML's own metrical preference. A double-time grid puts half its beats on low-activation frames |
| `down(c)` | Fraction of Beat This downbeat peaks (prob > 0.5) that coincide (±35 ms) with every M-th decoded beat for the best phase, M ∈ {3, 4} | Bars must contain an integer number of beats at the true level |
| `bb(c)` | **Backbeat score**: with decoded beats b_j, bar phase from `down`, low-band ODF L and mid/snare-band ODF S: `bb = mean_bars[ S(b_2)+S(b_4) − S(b_1)−S(b_3) ] / (mean S + ε)` plus a kick term `mean[L(b_1)+L(b_3) − L(b_2)−L(b_4)]` | Rock/pop/hip-hop convention: kick on 1 and 3, snare on 2 and 4. At 2× level the snare lands on every 4th beat (bb ≈ 0). At ½ level the snare falls *between* beats: high `S(midpoints)` ⇒ evidence for 2c |
| `mid(c)` | `mean S(b_j + IBI/2) / mean S(b_j)` | > 1 strongly suggests the true level is 2c (off-beat snare) |
| `prior(c)` | `−(log2(c/120))² / (2·1²)` | Perceptual tie-breaker (T4, T5) |
| `ioi(c)` | Robust CV of the decoded IOIs | A wrong level often decodes irregularly (fills hit some beats) |

Score: `S(c) = w·[z(sal), z(act), z(down), z(bb), −z(mid−1)₊, prior, −z(ioi)]`, with weights hand-set initially and then fitted by multinomial logistic regression on the annotated corpus (label = annotated level). Margin: `m = S(c₁) − S(c₂)` for the best two candidates **that are metrically related** (ratio 2, 3 or 3/2).
- m ≥ m_hi ⇒ Green, auto-accept.
- m_lo ≤ m < m_hi ⇒ Amber: accept c₁, show c₂ as a one-click alternative ("Detected 87.0 — or 174.0?").
- m < m_lo ⇒ Red: ask before analysis completes (or default to the candidate nearer 120 BPM and highlight it).

Genre caveats (Med confidence, common knowledge): drum'n'bass (~170–175 vs 85–87), trap and hip-hop halftime (140 vs 70), and dubstep (140 halftime) are inherently ambiguous. The backbeat convention picks the "snare on 2 and 4" level, which for halftime genres yields the slower number while producers often label the faster one. Since warping is octave-invariant, the **only** consequence is labelling, and the user can set it once. Remember the choice per project, not globally.

**Octave choice does not change the render** if the target grid uses the same level. Changing the target from B to 2B with the same beats is a relabel (`TARGET_BEAT_GRID.md` §2). This lowers the stakes of T2 for BeatFix specifically. **Level consistency across the song** is what matters, so after choosing c₁ the decoder runs with τ(t) constrained to that level (§4).

### 4. Local period path τ(t) for decoding (time domain)

Beat-domain smoothing (Whittaker on anchors) is already specified (`WARP_FUNCTION.md` §4). It needs beats first. The decoder needs a period prior *before* beats exist, so this is a time-domain path:

```
local_period_path(G, c1):                 # G: log-BPM tempogram (bins x frames), c1: chosen level
  restrict bins to [c1·2^-0.4, c1·2^0.4]  # ±40 % corridor around the chosen level: no octave jumps possible
  obs[f][b]  = log(eps + G[b][f] / max_b G[b][f])
  trans(b→b') = −(Δbins / s)²/2 ,  s = 1.5 bins (≈1.5 %) per 0.5 s hop      # allows ~6 %/min drift easily
              − J·[|Δbins| > 6]                                            # steps allowed but penalised (J ≈ 8 nats)
  path = viterbi(obs, trans)              # float64, ties → lowest bin index (deterministic)
  tau(t) = fps · 60 / BPM(path(t)), linearly interpolated to frame rate
  return tau
```
- The ±40 % corridor guarantees level consistency while allowing large drift and steps up to ~±35 %. For bigger intentional changes (e.g., a halftime bridge), the corridor is re-centred per segment only when §5 detects a sustained level change *and* the backbeat score supports it.
- Frames where max G is low (intro, breakdown) contribute nearly flat observations, so the path carries the neighbouring tempo through them. That is the desired "inferred" behaviour.

**Beat domain vs time domain (brief §7.6).** The time domain (tempogram, seconds) is right for priors, display before beats exist, and detecting regions with no pulse. The beat domain (IOIs indexed by beat number) is right for warping and drift statistics, because anchors are beats and smoothing in beat units makes λ musically meaningful (N_c beats). BeatFix uses both: time domain upstream of decoding, beat domain downstream. The UI's tempo curve is the beat-domain curve mapped to time (`BPM_ESTIMATION.md` §4).

### 5. Drift vs detector noise vs groove: a decomposition and tests

Model for refined anchors (index i, seconds):
```
S_i = a + b·i            (constant tempo)
    + d_i                (drift: slow, correlated over ≫ N_c beats)
    + g_i                (groove/micro-timing: periodic with bar, e.g. lag 2/4 patterns; intentional)
    + ε_i                (detector noise: ~i.i.d., σ_d ≈ 3–10 ms after refinement; plus rare outliers)
```
Computed pieces (no new smoothers; reuse the shared Whittaker implementation):
- `e_i = S_i − (â + b̂·i)` from the robust line fit (`BPM_ESTIMATION.md` §3).
- `x_i` = Whittaker smooth at N_c = 8 (the display smooth). `D_i = x_i − (â + b̂·i)` ≈ drift. `r_i = S_i − x_i` ≈ groove + noise.
- Noise scale `σ̂ = 1.4826·MAD(r_i)`. With pure noise the IOI lag-1 autocorrelation ≈ −0.5 (`BPM_ESTIMATION.md` §2).

Windowed tempo precision (regression over W beats, σ = 10 ms, 120 BPM):

| W (beats) | SE(period) | SE(BPM) | Smallest significant difference between adjacent windows (≈3·√2·SE) |
|---|---|---|---|
| 8 | 1.54 ms | 0.37 | 1.6 BPM (1.3 %) |
| 16 | 0.54 ms | 0.13 | 0.55 BPM (0.46 %) |
| 32 | 0.19 ms | 0.046 | 0.2 BPM (0.16 %) |

Formula: `SE(period) = σ·sqrt(12 / (W(W²−1)))`, `SE(BPM) = 60·SE(period)/p²`. The per-beat BPM noise for comparison is ≈ 3.4 BPM at σ = 10 ms (`BPM_ESTIMATION.md` §2).
⇒ Local tempo statements finer than ~0.5 % need ≥ 16-beat windows. This matches the N_c = 8–16 "Natural" defaults.

**Classification per song (and per region between change points):**

| Class | Test (initial thresholds = placeholders, calibrate on the corpus) | Warp consequence |
|---|---|---|
| **Stable** | `max|D_i| < max(3σ̂, 10 ms)` **and** BIC prefers constant over the quadratic/spline model (ΔBIC < 10) | Constant ratio. A strict grid is safe |
| **Drifting** | Not stable. No change points. Smooth D | Whittaker correction at the user's N_c |
| **Stepped** (tempo change) | Change point in log-IOI series (PELT or binary segmentation, penalty β = 2·σ_logIOI²·ln n), step > 1.5 %, both sides ≥ 8 beats | Insert a smoother break (`g_i = 0` at the boundary in `WARP_FUNCTION.md` §4.2) so the step is not smeared into a multi-bar ramp. Flag "tempo change at bar k: keep (sections) or flatten?" |
| **Spliced** (phase jump) | Step in `e_i` (or r_i) of size `|Δ| > max(4σ̂, 30 ms)`, sustained ≥ 8 beats on both sides, while windowed tempo on both sides differs < 0.5 % | No smooth warp can hide it. Options: absorb in one IOI if the ratio is within the engine's bound (`TARGET_BEAT_GRID.md` §2 warn band), else flag to the user ("possible splice at 1:42; +/− x ms"). Never spread it over N_c beats (that creates an audible surge) |
| **Level switch** | Sustained IOI ratio ≈ 2 or 0.5 vs the neighbours (beat doc §6) **with** backbeat support at the new level | Treat as a labelled section (halftime). Same grid level in the render unless the user asks |
| **Unreliable / rubato** | σ̂ > 25 ms, or tracker agreement < 0.6, or > 30 % inferred beats | Red. Suggest Natural with large N_c or manual anchors. Do not auto-strict |

Ordering: detect change points and splices **first** on robust residuals, then fit the smoother per segment. Otherwise the smoother partially absorbs steps and hides them.

**Discriminating drift from detector error (brief Q3), concisely:**
1. Detector errors are white (IOI lag-1 ≈ −0.5) and bounded (|r| ≲ 3σ̂). Drift is low-frequency (positive lag-1 IOI correlation, large |D|).
2. Detector errors are local and confidence-correlated (low c_i, disagreement with the DSP tracker). Drift appears in high-confidence beats and in both trackers.
3. Tracker-independent check: the DSP tracker's anchors give the same D_i (within 2σ̂) if the drift is real. Drift seen by only one tracker ⇒ suspect.
4. Groove is periodic with the bar (residual autocorrelation peaks at lag 2 or 4 beats) and is preserved because it lies above the N_c cutoff in frequency.

### 6. Intros, outros, breakdowns, abrupt changes

- No-pulse regions (low tempogram salience, low activation) take τ(t) carried by the Viterbi path. Beats there are `inferred` with low weight, so the warp follows the neighbouring tempo (`WARP_FUNCTION.md` §4.2 handles low-weight anchors). The intro's first downbeat comes from extrapolation and requires confirmation if the downbeat confidence is low.
- Fade-out: drop anchors after the activation falls below threshold for ≥ 2 bars, and extend with the edge ratio (`WARP_FUNCTION.md` §6).
- Abrupt intentional changes (bridge at a new tempo, ritardando ending): the Stepped class and a monotone ramp at the end (ritardando = drift localised in the last bars). The product decision "keep vs flatten" belongs to the UX layer. The default is to flatten drift but **ask** for steps > 5 % (placeholder).

### 7. Suno/AI-specific tempo considerations

- Existence of drift: verified (Suno help). Magnitude and shape: unverified. Suspected forms are slow ramps, section offsets, and seam steps or phase jumps (beat doc §9 hypotheses H1–H5).
- Integer-BPM conditioning of generators is *believed* (`BPM_ESTIMATION.md` §5, UNVERIFIED). A useful corpus statistic: the distribution of `bpmGlobal − round(bpmGlobal)` on stable Suno tracks. If it clusters at 0, the "suggest round(B̂)" default is justified. The one reported "107.553 BPM" non-integer anecdote (genxnotes, page inaccessible: 403) cuts the other way.
- Very long renders (> 6 min, multiple Extends) are where seams are expected. Run the splice and step detectors with extra sensitivity near suspected seam times if Suno metadata exposes them (UNVERIFIED whether exports carry this).

### 8. Pseudocode: tempo module (glue)

```
analyze_tempo(p_beat, p_down, odf, odf_low, odf_mid, fps):
  C       = tempo_candidates(p_beat, odf)                                  # §2
  ranked  = rank_levels(C, p_beat, p_down, odf_low, odf_mid, prior_120)    # §3
  c1, margin = ranked[0], ranked.margin
  tau     = local_period_path(tempogram(p_beat), c1)                       # §4
  # beats are decoded with tau (BEAT_TRACKING_RESEARCH §12), refined, repaired → S, conf
  return {candidates: ranked, level: c1, margin, tau}

classify_stability(S, conf):
  (a, b), e   = huber_linefit(S, conf)                         # BPM_ESTIMATION §3
  cps         = changepoints(log(diff(S)), conf)                # tempo steps
  jumps       = phase_jumps(e, cps, min_len=8, thr=max(4σ̂, 30ms))
  segments    = split(S, cps ∪ jumps)
  for seg in segments:
      x  = whittaker_irls(seg.S, seg.conf, N_c=8)                # WARP_FUNCTION §4 (shared code)
      D  = x − linefit(seg.S);  r = seg.S − x;  σ̂ = 1.4826·MAD(r)
      seg.class = Stable if max|D| < max(3σ̂, 10ms) and ΔBIC(const vs quad) < 10
                  else Unreliable if σ̂ > 25ms or seg.agree < .6
                  else Drifting
  return {segments, cps, jumps, flags}
```
All thresholds live in one versioned config, logged per analysis (consistent with `BPM_ESTIMATION.md` §7).

### 9. Evaluation for this module

- Standard: Acc1/Acc2 (±4 %) on GTZAN/Ballroom/GiantSteps for regression tracking only. These are not product targets.
- BeatFix targets (to validate; placeholders):
  - Octave-correct rate ≥ 95 % on the annotated Suno corpus, with every miss surfaced as Amber/Red.
  - |bpmGlobal error| < 0.05 BPM on stable tracks.
  - Synthetic drift injection: take grid-locked electronic tracks, apply known smooth warps (ramps 0.5–5 %, sinusoidal ±2 % with 8–64-beat periods, steps 2–10 %, phase jumps 20–200 ms) with a high-quality stretcher. Measure classification accuracy, change-point localisation (± 1 beat), and recovered-tempo RMSE.
  - Noise robustness: add σ = 0–20 ms synthetic anchor jitter and confirm that no Stable track is classified Drifting at σ ≤ 10 ms.

## Alternatives Considered

| Option | Verdict | Reason |
|---|---|---|
| Single global ACF peak (classic) | Reject | Octave errors (T1). Coarse resolution (T6). No drift |
| CNN tempo classifier (Schreiber & Müller; madmom TCN) | Not available | Best Acc1, but no MIT-licensed model identified (madmom models CC BY-NC-SA; Essentia TempoCNN AGPL per baseline log). Revisit if we train our own |
| Bar-pointer HMM joint tempo+phase decoding | Defer | Strong continuity, but heavy state space, fixed meters and an unresolved patent claim (beat doc §2.3). The corridor-constrained Viterbi path + DP achieves the needed level consistency more simply |
| Kalman/RTS for local tempo | Equivalent alternative | Already assessed in `BPM_ESTIMATION.md` §4 and `WARP_FUNCTION.md` §4.5. The Whittaker smoother was chosen for shared code. Kalman stays an option for adaptive per-segment noise |
| Moving median of BPM_i | Display sanity only | Stair-steps and frame-lattice bias |
| Interacting-multiple-model filter for steps | Reject for MVP | Change point first, then a per-segment smoother is simpler and testable |
| Perceptual prior only for octave | Reject | T4: humans disagree. Genre conventions (halftime) defeat a 120 BPM prior |

## Evidence / Sources

- Böck & Davies, ISMIR 2020, Table 1 (tempo Acc1/Acc2 for TCN, Schreiber & Müller 2018, Percival & Tzanetakis 2014, Gkiokas, Foroughmand & Peeters, Böck 2015): http://archives.ismir.net/ismir2020/paper/000223.pdf
- Ahn, Hwang, Jung, "The SMC Blind Spot", arXiv 2605.12287 (2026): https://arxiv.org/abs/2605.12287
- McKinney & Moelants, "Ambiguity in tempo perception" (RPPW / Music Perception 2006): https://cspeech.ucd.ie/rppw/rppw10/mckinney/abstract.html. Van Noorden & Moelants 1999 (JNMR) resonance model, via search summary.
- librosa API (tempo, tempogram, fourier_tempogram, plp, priors): Context7 `/websites/librosa_doc`.
- madmom API (TempoEstimationProcessor, CombFilterbankProcessor, DBN defaults): Context7 `/cpjku/madmom`.
- Suno help, "Fixing Tempo Drift": https://help.suno.com/en/articles/8363457
- `aislice` (vendor example of drift, method description): https://pypi.org/project/aislice/
- Established literature, cited without re-fetching (Med confidence on exact parameters): Grosche & Müller 2011 (IEEE TASLP, PLP / Fourier tempogram); Peeters 2007 (EURASIP JASP); Böck, Krebs, Widmer ISMIR 2015 (resonating comb filters); Percival & Tzanetakis 2014 (IEEE/ACM TASLP); Schreiber & Müller ISMIR 2018; Scheirer 1998 (JASA); Moelants 2002 (ICMPC, preferred tempo); Killick et al. 2012 (PELT, JASA).
- Internal: `04_algorithms/BPM_ESTIMATION.md`, `WARP_FUNCTION.md`, `TARGET_BEAT_GRID.md`.

## Recommendation

1. **Candidates:** combined ACF×Fourier tempogram on the Beat This activation (DSP ODF fallback), log-BPM bins at 1 %, closed under {1/3, 1/2, 2/3, 1, 3/2, 2, 3}. (High)
2. **Range:** internal 40–250 BPM. **Proposed edit to `BPM_ESTIMATION.md` §6** (currently 50–220) for consistency with T3. Display normalisation stays a UI choice. (Med-High)
3. **Level ranking:** feature score (salience, activation contrast, downbeat consistency, backbeat and mid-beat snare, IOI regularity, weak 120 BPM log-normal prior) with a margin → Green/Amber/Red. Weights fitted on the annotated corpus. (Med)
4. **Local period path:** corridor-constrained (±40 %) Viterbi over the tempogram to drive the decoder. This guarantees octave consistency. (High)
5. **Stability classification:** change points and phase jumps first, then per-segment Whittaker (shared code) → Stable / Drifting / Stepped / Spliced / Level switch / Unreliable. Splices are never smoothed. (Med-High)
6. **Corpus first:** freeze no thresholds until the synthetic-drift set and ≥ 50 annotated Suno songs exist. (High)

## Risks

- Backbeat heuristics fail on non-backbeat genres (four-on-the-floor house with claps on 2/4 still works; latin, afro and trap rolls may not). This is mitigated by the feature mix and the margin-based asking.
- Corridor constraint can block a genuine large tempo change. Mitigated by per-segment re-centring when the change-point and backbeat evidence agree.
- The splice detector may flag intentional pushes or pulls (live-feel generations). The output is a flag, not an automatic action.
- Prior and weights over-fit to the annotated corpus. Mitigate with held-out evaluation and logged parameters.
- Unknown Suno drift statistics may make "Natural N_c = 8" a poor default (also flagged in `WARP_FUNCTION.md` §4.3).

## Open Questions

1. Suno drift statistics: ramp magnitudes, step frequency, phase-jump frequency, dependence on Suno version, Extend usage and genre.
2. Do Suno stable tracks sit on integer BPM (supports the `round()` default)?
3. Do Suno exports or metadata expose prompt BPM or Extend seam times usable as priors? (UNVERIFIED)
4. Halftime-genre labelling: should the default label follow the backbeat convention (slower) or producer convention (faster, e.g. trap at 140)? This is a UX decision.
5. Should Stepped regions default to "keep sections" or "flatten"? This needs subjective testing.

## Implementation Implications

- One module `tempo/` with pure functions: `tempogram_logbpm`, `candidates`, `rank_levels`, `period_path`, `changepoints`, `phase_jumps`, `classify`. All are float64 and deterministic, with ties broken by lowest index and no RNG (the bootstrap in `BPM_ESTIMATION.md` already fixes its seed).
- Shared dependency on the Whittaker/IRLS implementation from the warp module (single source of truth, as `BPM_ESTIMATION.md` §4 requires).
- Analysis artefact additions: `tempo.candidates[{bpm, score}]`, `tempo.level`, `tempo.margin`, `tempo.period_path(f32 @ 2 Hz)`, `stability.segments[{start_beat, end_beat, class, bpm_range}]`, `stability.events[{beat, type: step|splice|level_switch, size}]`.
- UI hooks: candidate chips (x2 / ÷2 / ×3/2), segment classes on the tempo lane, and event markers with keep/flatten actions. Editing level or phase re-runs decoding from cached activations (sub-second).
- Unit tests: candidate closure contains the true level for synthetic clicks at 60/90/120/174 BPM. Backbeat score sign is correct on a synthetic kick/snare pattern at c and 2c. The corridor path never jumps an octave on a click track with 25 % dropouts. Step and splice detectors meet the localisation and false-positive targets in §9.
