# Warp Function: Mapping Source Beats to the Target Grid

## Objective
Define the time-warp `τ = W(t)` from source time `t` to output time `τ`. Cover the stretch ratios, the interpolation between anchors, why the map must be monotonic, and the optimisation that decides how strictly detected beats are pulled onto the grid (Natural ↔ Strict).

## Why It Matters
`W` is the only thing the renderer obeys. Every audible timing result (drift removed, groove kept or destroyed, detector noise injected or not) comes from how `W` is built. Every catastrophic failure (time reversal, a skipped beat, a 2× stretch) is a property of `W` that can be checked before any audio is rendered.

---

## 1. Basic mapping (brief §7.4)

Given anchor pairs `(x_i, T_i)`, with `x_i` the source anchor (after refinement and optimisation, §3–4) and `T_i` the target grid time (TARGET_BEAT_GRID.md):

```
r_i = (T_{i+1} − T_i) / (x_{i+1} − x_i) = P / (x_{i+1} − x_i)
```
`r_i` is the **time ratio**, output duration over input duration, for segment `i`.

| r | meaning | example |
|---|---|---|
| r > 1 | stretch: the source segment was too fast (shorter than P) | source 112 BPM to target 110: r = 112/110 = 1.018 |
| r < 1 | compress: the source was too slow | source 108 to target 110: r = 0.982 |
| r = 1 | unchanged | |

For a constant-tempo segment, `r = B_source / B_target`.

**Library conventions differ. Always convert at the engine adapter boundary:**
Rubber Band "time ratio" = r. A Bungee-style "speed" = 1/r. Signalsmith Stretch takes input and output sample counts per call, so the ratio is implicit (output/input = r). See TIME_STRETCH_ENGINE.md.

## 2. Interpolation between anchors (brief §7.5)

### 2.1 Piecewise linear (MVP)
```
W(t) = T_i + r_i · (t − x_i)          for t ∈ [x_i, x_{i+1}]
W⁻¹(τ) = x_i + (τ − T_i) / r_i         for τ ∈ [T_i, T_{i+1}]
```
- Exact, closed-form inverse, O(log n) lookup by binary search on `x` or `T`.
- The ratio is piecewise constant and steps at each anchor. After the smoothing in §4, adjacent `r_i` differ by about 0.01–0.3 % in Natural mode. That is far below audibility for a time-stretch engine whose ratio is quantised per analysis hop anyway.
- In Strict mode the steps can reach several percent. They occur **at beats**, which are transients, where TSM engines reset phase anyway. Whether this is audible is an **open question**, tested in SUBJECTIVE_LISTENING_TESTS.md.

### 2.2 Monotone cubic (PCHIP, Fritsch–Carlson). Upgrade path.
C¹-continuous `W`, so `r(t) = W'(t)` is continuous. Monotonicity is guaranteed by construction when the data are strictly increasing. The inverse needs a per-block Newton or bisection solve, which is cheap. **Adopt only if** listening tests show ratio steps are audible in Strict mode.

### 2.3 Rejected
- Unconstrained cubic or high-order splines and polynomials: they overshoot, can make `W' ≤ 0` (non-monotone), and offer no audible benefit.
- Global polynomial tempo models: they cannot represent local fixes.

## 3. Why W must be strictly monotonic (and bounded)

1. **Causality.** If `W` decreased anywhere, some output instant would come from a *later* source instant than the output just before it. Audio would play backwards or repeat.
2. **Invertibility.** The renderer is pull-based. For each output block it asks which source span to read, `W⁻¹`. That is only a function if `W` is strictly increasing.
3. **Engines.** TSM engines accept only ratios in (0, ∞). r → 0 is infinite compression and r → ∞ infinite stretch.
4. **Musical order.** Beat i must stay before beat i+1.

Enforced invariants (checked by `WarpMap::validate()`, unit-tested, and checked again before every render):
```
x_{i+1} − x_i ≥ Δ_min           (Δ_min = 0.25·P_source_local, guards against collapsed anchors)
T_{i+1} − T_i = P > 0
r_hard_min ≤ r_i ≤ r_hard_max   (0.5 .. 2.0 hard. Outside this range, refuse and point to the half/double fix)
r_warn_lo  ≤ r_i ≤ r_warn_hi    (default 0.92 .. 1.08. Warn and highlight the segment)
```

## 4. Anchor optimisation: Natural ↔ Strict (brief §7.6, §7.7)

### 4.1 The problem with "strict" alignment of raw detections
Let the true beat be `s_i` and the detection `S_i = s_i + ε_i`, where `ε_i` is detector error, σ ≈ 5–15 ms on full mixes (BEAT_TRACKING_RESEARCH.md). Mapping `S_i → T_i` exactly puts the *true* beat at
```
W(s_i) = T_i − r·ε_i
```
**Strict alignment of noisy anchors copies the detector noise into the output.** The output then has jitter that the source never had. This is the main reason the default cannot be 100 % strict.

### 4.2 Formulation
Estimate smoothed anchors `x` by minimising:
```
E(x) = Σ_i  w_i · ρ( S_i − x_i )                  # fidelity to detections (robust)
     + λ · Σ_i  g_i · ( x_{i+1} − 2x_i + x_{i−1} )²  # smoothness of tempo (2nd difference of positions)
subject to x strictly increasing and the ratio bounds of §3
```
- `w_i ∈ [0,1]` is the anchor confidence (BEAT_TRACKING.md). Low-confidence beats (drumless intros, breakdowns, inferred beats) barely pull, so `x` follows the smooth tempo across gaps. **This handles drumless intros and fills without special cases.**
- `ρ` is the Huber loss, solved by IRLS (3–5 iterations, scale = 1.4826·MAD of residuals, Huber k = 1.345). A single mis-detected beat is down-weighted instead of bending the curve.
- `g_i ∈ {0,1}` is set to 0 across a detected **intentional tempo change point** (TEMPO_STABILITY_ANALYSIS.md), so a deliberate step is not smeared into a ramp.
- With ρ quadratic this is the **Whittaker–Eilers smoother**. Each IRLS step solves the banded system
  ```
  (W + λ Dᵀ G D) x = W S          # D = second-difference matrix, pentadiagonal ⇒ O(n)
  ```
  This is deterministic, needs no iterative optimiser and takes about 1 ms for 1 000 beats.

### 4.3 What λ means: a cutoff in beats
For uniform weights, the smoother is a low-pass filter on the beat-position sequence with response
```
H(ω) = 1 / (1 + 16 λ sin⁴(ω/2))      (ω in radians per beat)
```
Choosing a cutoff period of `N_c` beats (H = 0.5) gives `λ = 1 / (16 sin⁴(π / N_c))`:

| N_c (beats) | λ | Behaviour |
|---|---|---|
| — | 0 | **Strict**: x = S, every detected beat lands on the grid |
| 4 (1 bar) | 0.25 | Corrects everything slower than a bar |
| 8 (2 bars) | 2.91 | **Default "Natural"**: removes drift, keeps per-beat micro-timing |
| 16 (4 bars) | 43.2 | Very gentle; only slow ramps are corrected |
| 32 (8 bars) | 677 | Close to "global tempo change only" |
| ∞ | ∞ | x = weighted linear fit, giving one constant ratio for the whole song |

Deviations **faster** than N_c beats (groove, swing micro-timing, detector noise) are *kept as in the source*. Deviations **slower** than N_c (drift, ramps, section-level offsets) are *corrected*. The single UI control "Natural ↔ Strict" maps to six detents N_c ∈ {∞ (Global), 32, 16, 8 (default), 4, Strict} (WAVEFORM_VIEW.md / MAIN_WINDOW.md). N_c = 2 is close to Strict and is not exposed.

**Default = Natural (N_c = 8).** This is an **assumption**, to be validated by subjective tests on AI-generated material (SUBJECTIVE_LISTENING_TESTS.md, test S-3). Rationale: AI-music drift is reported to be slow (ramps and section offsets; see BEAT_TRACKING_RESEARCH.md). Natural is also robust to detector noise (§4.1).

### 4.4 Output timing error budget
After warping, a beat's distance from the grid is:
```
e_out,i = r_i · (s_i − x_i)        # = kept groove + kept detector-noise-free true deviation
```
In Strict mode, `e_out,i = −r_i ε_i` (pure detector noise). This decomposition is what the QA metric "beat alignment error" measures (METRICS.md).

### 4.5 Alternatives considered
| Option | Why not (for MVP) |
|---|---|
| Strict mapping of raw detections | Injects detector noise (§4.1) and destroys groove |
| Constrained QP over a spline W with ratio bounds and transient penalties `μ·transient_distortion + ν·phase_risk` | No validated model links W to the engine's transient or phase artifacts, so those terms would be unsupported heuristics. Ratio bounds are enforced by checking instead. Transient handling belongs in the stretch engine (TRANSIENT_PRESERVATION.md). |
| Joint beat-tracking + warp HMM/DP | Couples two replaceable modules. Harder to test. |
| DTW alignment to a synthetic click | Equivalent to strict with extra cost. Not robust to missing beats. |
| Kalman (constant-velocity period) + RTS smoother | Equivalent to a 2nd-order smoother with a physical noise model. Kept as an alternative if per-segment adaptive smoothing is needed. |

## 5. Anchor refinement (sub-frame precision)

Beat-tracker activations are frame-quantised (Beat This!: 50 fps, so 20 ms frames, giving a uniform quantisation error with σ = 20/√12 ≈ 5.8 ms). Before §4, each `S_i` is refined:
1. parabolic interpolation of the activation peak, which gives a sub-frame position;
2. optional onset snap: within ±25 ms, find the peak of a high-resolution onset function (hop 128 samples ≈ 2.9 ms at 44.1 kHz) in **one fixed band per song**. The band is chosen once by the analysis stage (low band if kick evidence is strong, otherwise broadband) and kept fixed, so the reference does not switch between kick and snare from beat to beat. The snap is applied only if the onset peak exceeds a salience threshold, and otherwise the interpolated position is kept.
3. global offset correction: compute the median of (onset peak − activation peak) over all confident snaps and apply it as one constant shift to *every* anchor, including the unsnapped ones. This removes the systematic bias between the model's beat position and the acoustic attack without adding per-beat jitter.

Steps and splices detected by TEMPO_STABILITY_ANALYSIS.md set `g_i = 0` (§4.2). A splice is absorbed within one beat and never smeared across N_c beats.

## 6. Edges (pre-roll and post-roll)
```
t < x_0:  W(t) = T_0 + r_0 · (t − x_0)
t > x_n:  W(t) = T_n + r_{n−1} · (t − x_n)
```
This keeps the edge ratio continuous. DAW-ready start (TARGET_BEAT_GRID.md §3) prepends silence *after* warping and never changes W.

## 7. Feeding the renderer without drift

For an output block `[τ_a, τ_b)` the renderer reads source `[W⁻¹(τ_a), W⁻¹(τ_b))`. Sample counts are always computed from **absolute** positions:
```
src_start = round(W⁻¹(τ_a) · fs);  src_end = round(W⁻¹(τ_b) · fs)
n_in = src_end − src_start         # never accumulate n_in incrementally
```
Incremental accumulation of rounded counts drifts by up to 0.5 sample per block, which adds up to tens of ms over a song.

## 8. Worked example
Anchors from TARGET_BEAT_GRID.md §6, Strict mode (x = S), P = 0.545455:

| seg | x_i → x_{i+1} | r_i |
|---|---|---|
| 0 | 1.000 → 1.556 | 0.981 (compress 1.9 %) |
| 1 | 1.556 → 2.111 | 0.983 |
| 2 | 2.111 → 2.660 | 0.994 |
| 3 | 2.660 → 3.205 | 1.001 |

- `W(1.8) = 1.545455 + 0.982800·(1.8 − 1.556) = 1.785258 s`
- `W⁻¹(2.0) = 1.556 + (2.0 − 1.545455)/0.982800 = 2.018505 s`
- The segment-to-segment ratio steps are 0.2 %, 1.1 % and 0.7 %. These are Strict mode values. Natural mode would spread them over 8 beats.

## 9. Pseudocode
```
function buildWarp(S, conf, changePoints, B, policy, N_c):
    S  = refineAnchors(S)                         # §5
    w  = conf
    λ  = (N_c == STRICT) ? 0 : 1 / (16 * sin(π/N_c)^4)
    g  = ones(n-1); g[changePoints] = 0
    x  = S
    repeat 4 times:                               # IRLS
        x = solveBanded(diag(w) + λ Dᵀ diag(g) D,  diag(w) S)
        res = S - x;  s = 1.4826 * MAD(res)
        w = conf * huberWeight(res / s, k=1.345)
    P  = 60 / B
    φ  = placePhase(x, policy, P)                 # TARGET_BEAT_GRID §3
    T  = [φ + i*P for i in 0..n]
    W  = PiecewiseLinear(x, T)
    W.validate()                                  # §3. Throws with segment list
    return W
```

## Risks
| Risk | Mitigation |
|---|---|
| Natural default under-corrects fast drift in some AI outputs | Slider to Strict. Residual-drift metric shown after render. |
| Huber IRLS converges to the wrong curve when >30 % of anchors are wrong | Gate: if robust residual scale > 0.15·P, report "beat analysis unreliable" and do not auto-render |
| Change-point detection misses an intentional step | Detected change points are shown in the UI. The user can add or remove them (post-MVP manual edit; MVP shows a warning) |

## Implementation Implications
- `WarpMap` is immutable data: anchors x, T, ratios, segment metadata. The functions are pure (`W`, `Winv`, `ratioAt`, `validate`).
- Unit tests: monotonicity under random anchor jitter; inverse round trip `|W⁻¹(W(t)) − t| < 1e-9`; λ = 0 gives x = S exactly; λ → large gives a linear fit; cutoff response measured on a synthetic sinusoidal tempo modulation within ±10 % of the H(ω) formula; no drift in absolute sample counts over a 20-minute render.
