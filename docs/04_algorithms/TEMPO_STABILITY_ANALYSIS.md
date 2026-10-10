# Tempo Stability Analysis

Status: algorithm spec, 2026-10-08. Decision: intentional tempo changes are **flagged and the user asked** ("flatten to target" or "keep that section unwarped, r = 1"), never auto-smeared ([DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §3.6, §6). Sources: [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) T9–T10, §5–8; [BPM_ESTIMATION.md](BPM_ESTIMATION.md) §2–4; [WARP_FUNCTION.md](WARP_FUNCTION.md) §4.2, §5.

## Objective
Classify the song, and each region between detected events, as **Stable, Drifting, Stepped, Spliced, Level-switch or Unreliable**; separate real drift from detector noise; locate tempo steps and phase jumps; set the smoother break flags `g_i`; produce a stability score and the UI readouts.

## Why It Matters
The class decides what the warp should do (TEMPO research "Why It Matters"):
- Smooth drift → Whittaker correction at the user's N_c.
- A tempo **step** → a break in the smoother so the step is not smeared into a multi-bar ramp (an audible surge), plus a user decision.
- A **splice** (phase jump) → no smooth warp can hide it; spreading it over N_c beats creates a surge. It must be absorbed in one IOI or surfaced.
- Detector noise mistaken for drift → the warp injects wobble into a steady song (the "stable 110 BPM song must stay unchanged" acceptance test fails).

## 1. Signal model
Refined anchors (index i, seconds), after repair ([OUTLIER_REJECTION.md](OUTLIER_REJECTION.md)):
```
S_i = a + b·i        constant tempo
    + d_i            drift: slow, correlated over ≫ N_c beats
    + g_i^groove     groove/micro-timing: periodic with the bar, intentional, kept
    + ε_i            detector noise: ≈ i.i.d., σ ≈ 3–10 ms after refinement, rare outliers
    + J_i            steps (kinks in S vs i) and splices (jumps in S)
```
Computed quantities (shared Whittaker/IRLS code, no new smoother):
| Symbol | Definition |
|---|---|
| `(â, b̂)`, `e_i` | Huber-IRLS line fit and its residuals ([BPM_ESTIMATION.md](BPM_ESTIMATION.md) §3) |
| `x_i` | Whittaker/IRLS smooth at N_c = 8 (display cutoff, independent of the user slider) |
| `D_i = x_i − (â + b̂·i)` | drift estimate |
| `r_i = S_i − x_i` | groove + noise |
| `σ̂ = 1.4826·MAD(r)` | noise scale |

## 2. Drift vs detector noise (brief Q3)
Three statistics, each with a known value under the null hypothesis "constant tempo + white detector noise":

| Test | Statistic | Null value | Drift signature |
|---|---|---|---|
| **Lag-1 IOI autocorrelation** | `ρ₁ = Σ d_k d_{k−1} / Σ d_k²`, d = IOI − mean IOI | **−0.5** (consecutive IOIs share one detection error with opposite sign, BPM_ESTIMATION §2). Bartlett SE for MA(1), θ = −1: `sqrt(0.5/n)` | ρ₁ rises toward 0 or positive. Reject null if `ρ₁ > −0.5 + 3·sqrt(0.5/n)` |
| **Variance ratio** | `VR = V(16)/V(1)`, `V(k) = Var_i(S_{i+k} − S_i − k·b̂)` | **1** (pure position noise gives `V(k) = 2σ²` for every k) | VR ≫ 1 (grows ~k² for smooth drift). Placeholder threshold 2 |
| **Residual trend** | `maxDrift = max_i |D_i|`; `ΔBIC` of quadratic vs constant fit of S | `maxDrift < max(3σ̂, 10 ms)`, ΔBIC < 10 | T10: a linear ramp of relative size ε over N beats leaves a residual ≈ ε·P·N/12 (ε = 0.1 %, N = 400, P = 0.5 s → 17 ms) |

Caveats:
- A period-2 groove (laid-back snare on 2 and 4) makes IOIs alternate, pushing ρ₁ **below** −0.5 and VR below 1. Neither test confuses groove with drift; groove is recognised by residual autocorrelation peaks at lag 2 or 4 beats and is kept (it lies above the N_c cutoff).
- Detector errors are confidence-correlated and local; drift appears in high-confidence beats **and** in the independent DSP tracker. Drift seen only by one tracker is suspect: compute D from `B_dsp` too and require agreement within 2σ̂ before reporting "Drifting" with High confidence.
- "Is there drift?" is easy over a whole song (T10). "Where and how fast?" needs windows: local tempo finer than ~0.5 % needs ≥ 16-beat windows (TEMPO research §5 table).

## 3. Change-point detection (tempo steps)
**Method: two-sided windowed regression scan + ΔBIC confirmation**, on positions S_i (not on IOIs).

Why not PELT on log-IOIs (research §5 option): IOI noise is MA(1) (lag-1 −0.5), which violates the i.i.d. segment cost that PELT/binary segmentation assume, and a mean-shift cost splits a smooth ramp into many fake steps. Position residuals are close to i.i.d., and a ramp vs step is decided explicitly by model comparison.

For every boundary j (between anchors j and j+1), with L = 16 beats per side (min 8 near edges):
```
left : S_i ≈ a_L + b_L·(i − j),        i ∈ [j−L+1, j]
right: S_i ≈ a_R + b_R·(i − j − 1),    i ∈ [j+1, j+L]
SE_b    = σ̂ · sqrt(12 / (L(L² − 1)))                      # per side
z_step  = ln(b_R / b_L) / (sqrt(2) · SE_b / b̂)
Δ_jump  = (a_R − a_L) − (b_L + b_R)/2                       # extra time at the boundary IOI
SE_jump = σ̂ · sqrt(2 · (4L − 2) / (L(L + 1)))              # ≈ 0.68 σ̂ at L = 16
```
Step candidate at j if all hold (placeholders, calibrate on S04 and corpus category 20):
1. `|z_step| > 5` and is a local maximum of |z_step| within ±8 beats (non-maximum suppression; ties → lower j);
2. `|ln(b_R/b_L)| > 1.5 %`;
3. confirmation: in the 2L window, the "two lines with a kink at j" model beats the "single quadratic" (ramp) model by ΔBIC > 10.

Condition 2 rejects ramps (a 108→113 ramp over 400 beats changes the period by only ~0.2 % across a 16-beat boundary); condition 3 rejects fast ramps that pass condition 2.

## 4. Splice / phase-jump detection
Spliced at boundary j if (research §5, thresholds placeholders):
- `|Δ_jump| > max(4σ̂, 30 ms)` and `|Δ_jump| > 5·SE_jump`, local maximum within ±8 beats;
- windowed tempo on both sides differs by < 0.5 % (`|ln(b_R/b_L)| < 0.005`), otherwise it is a step;
- both sides have ≥ 8 beats with mean c_i > 0.5.

**Order matters**: splices and steps are detected first, on robust residuals, then the smoother runs per segment. A splice that is not broken out is partly absorbed by the smoother and hides itself (worked example §9: an unbroken 40 ms splice looks "Stable", with R = 0.990).

## 5. Level switch and Unreliable
- **Level switch (half-time section)**: the main decoder cannot switch level (corridor), so a switch is detected as a *feel* change: in sliding 16-bar windows (hop 4 bars), recompute `act`, `bb` and `mid` ([HALF_DOUBLE_TIME_RESOLUTION.md](HALF_DOUBLE_TIME_RESOLUTION.md) §3) at the song level c1 and at c1/2 and 2·c1 on the decoded beats. A window prefers the other level when its score exceeds c1's by the Amber margin **and** `bb` supports it. ≥ 8 consecutive bars of preference form a level-switch section. (In the fallback path, OUTLIER_REJECTION §2's sustained-IOI-ratio rule is the second detector.) Render: the song's single level is kept; the section is labelled "half-time feel". Re-centring the corridor for that section happens only if the user asks.
- **Unreliable / rubato**: `σ̂ > 25 ms`, or tracker agreement A < 0.6, or > 30 % inferred beats, or robust residual scale > 0.15·P (WARP_FUNCTION gate). Red; no automatic render; suggest Natural with large N_c.

## 6. Classification
Per segment between accepted events (and for the song as a whole, by majority duration):
```
Unreliable  if §5 test fires
Stable      if maxDrift < max(3σ̂, 10 ms) and ΔBIC(quad vs const) < 10 and ρ₁ test not rejected
Drifting    otherwise
Song-level events: Stepped (≥ 1 step), Spliced (≥ 1 splice), Level-switch (≥ 1 section)
```
No-pulse regions (intro, breakdown) contain only `inferred` beats and are excluded from σ̂, ρ₁ and VR.

## 7. How results feed the warp (`g_i`, WARP_FUNCTION §4.2)
`g_k` multiplies the second-difference term centred at anchor k: `(x_{k+1} − 2x_k + x_{k−1})²`.

| Event | Setting | Effect |
|---|---|---|
| Step at anchor j (kink) | `g_j = 0` | allows a slope (tempo) change at j without smearing; both sides still smooth |
| Splice in IOI (j, j+1) (jump) | `g_j = g_{j+1} = 0` | fully decouples the two sides; the jump is absorbed in one IOI |
| Level-switch section, no user action | none | single level, normal smoothing |
| User "keep section unwarped" on [a, b] | anchors a..b use `T_i = T_a + (x_i − x_a)` (r = 1); after b, `T_i = T_b + (i − b)·P` | section untouched; grid resumes with a new phase after it |

Why two zeros for a jump: a position jump Δ produces second differences +Δ at anchor j and −Δ at j+1; zeroing only one leaves the other coupling the sides.

Ratio check after a splice break: the absorbing IOI has `r = P/(P + Δ)`. Inside the warn band ([TARGET_BEAT_GRID.md](TARGET_BEAT_GRID.md) §2, default 0.92..1.08) it is absorbed silently with a flag; outside, the segment is highlighted and the user is told ("possible splice at 1:42, +45 ms").

**Defaults before the user answers**: steps and splices are **flattened** with the breaks above (never smeared). Steps > 5 % (placeholder) and all splices outside the warn band add a Review entry in the Issues list (UX owns the prompt, UX_BEHAVIOR.md). "Keep section unwarped" breaks DAW-ready bar alignment after the section; the UI must say so.

## 8. Stability score and UI readouts
| Output | Definition | UI |
|---|---|---|
| `stabilityScore` | phase coherence `R = |mean_i exp(j·2π·e_i/b̂)|` of the Huber-line residuals (same quantity as [TARGET_BEAT_GRID.md](TARGET_BEAT_GRID.md) §4 R, wrap-free) ∈ [0,1] | input to BPM confidence (BPM_ESTIMATION §7) |
| `maxDriftMs` | `max |D_i|` | "max drift X ms vs constant fit" (WAVEFORM_VIEW.md §7) |
| `ibiCvPct` | `100 · 1.4826·MAD(IOI)/median(IOI)`, raw refined IOIs | "IBI CV Y %" |
| `bpmRange` | p5–p95 of local tempo (BPM_ESTIMATION §3) | "108.2 – 112.9 BPM" |
| `stability.segments[]`, `stability.events[]` | class per segment; `{beat, bar, time, type: step|splice|levelSwitch, size}` | tempo-lane colouring, event markers with keep/flatten |

Category labels (MAIN_WINDOW.md DETECTED row; thresholds placeholders):
| Condition | Label |
|---|---|
| Stable | "Stable" |
| Drifting, maxDrift ≤ 100 ms | "Slight drift (max 84 ms)" |
| Drifting, maxDrift > 100 ms | "Drifting (max 820 ms)" |
| any step | "Tempo change at bar 33 (+3.6 %)" |
| any splice | "Possible splice at 1:42 (+45 ms)" |
| level switch | "Half-time feel, bars 49–64" |
| Unreliable | "Unsteady tempo — check beats" |

Multiple events: the label shows the most severe (Unreliable > splice > step > level switch > drift), and the Issues list shows all.

## 9. Worked example (simulated, seed 7, 400 beats, σ = 8 ms detector noise, P = 0.5455 s)
| Case | BPM fit | σ̂ | ρ₁ (null −0.5, reject if > −0.394) | VR(16) | maxDrift | R | ΔBIC | Scan | Class |
|---|---|---|---|---|---|---|---|---|---|
| Stable 110 | 109.999 | 6.0 ms | −0.470 | 1.07 | 12.6 ms | 0.996 | −5.6 | none | Stable |
| Ramp 108→113 | 110.488 | 5.9 ms | −0.031 | 73.7 | 821 ms | 0.263 | 3080 | max |z| 4.1, Δ 0.46 % → rejected | Drifting |
| Step 110→114 at beat 200 | 111.957 | 6.4 ms | +0.085 | 96.7 | 958 ms | 0.080 | 1105 | z = −31 at j = 199, Δ = 3.5 % | Stepped (2 Stable segments) |
| Splice +40 ms at beat 200 | 109.970 | 5.6 ms | −0.465 | 1.52 | 28.5 ms | 0.990 | −5.9 | Δ_jump = 51.7 ms at j = 199 (SE 5.4 ms) | Spliced (2 Stable segments) |

Splice handling: with `g_199 = g_200 = 0`, the absorbing IOI is 590.5 ms vs 544–547 ms neighbours (r = 0.924, inside the warn band, flagged). Without the break, the smoother spreads the jump over ~8 beats (IOIs 547→558→550 ms), an audible surge. Note that the splice case passes the global drift tests: only §4 finds it.

## Alternatives Considered
| Option | Verdict | Why |
|---|---|---|
| PELT / binary segmentation on log-IOI | Reject (as primary) | MA(1) noise violates the cost; ramps split into steps |
| Smoother first, then look for events in residuals | Reject | The smoother absorbs steps and splices (§4 order) |
| Interacting-multiple-model filter | Reject for MVP | Change points + per-segment smoother is simpler and testable (TEMPO research) |
| Bar-pointer HMM with tempo states | Defer | State space, fixed meters, patent question (legal review) |
| Auto "keep" for steps | Reject | Product decision: flag + ask; never auto-smear |

## Risks
- Splice detector flags intentional pushes/pulls in live-feel generations: output is a flag, not an action.
- Thresholds from synthetic data may not transfer to Suno drift (magnitudes UNVERIFIED, T9). Freeze nothing before the annotated corpus exists.
- Steps inside no-pulse regions are undetectable; reported as Unreliable span edges.

## Open Questions
1. Real distribution of Suno drift, steps and splices (TEMPO research OQ1).
2. Default for Stepped regions: flatten vs keep (subjective test).
3. TARGET_BEAT_GRID.md §2 slot rule `T_k = φ + kP` needs a "kept section" clause (§7 above) — owner of that doc to confirm.

## Tests
| ID | Input | Assertion |
|---|---|---|
| TS-1 | S01 at 110 with jitter σ = 0/5/10 ms (S05) | class Stable; no events; ρ₁ test not rejected in ≥ 99 % of 200 seeds at σ ≤ 10 ms |
| TS-2 | S03 ramps 108→113, 120→125, 90→80 | Drifting; no step or splice events |
| TS-3 | S04 steps 120→126 at bar 33, 120→90 at bar 17 | step located ± 1 beat; `g` zero at that anchor only |
| TS-4 | Splices 20/30/50/100/200 ms injected at a known beat | located ± 1 beat for |Δ| ≥ 30 ms; no false splice on TS-1 inputs |
| TS-5 | Groove: alternating ±12 ms on beats 2 and 4 | Stable (groove not called drift); lag-2 residual autocorrelation reported |
| TS-6 | Splice break warp | absorbing IOI equals `P + Δ` ± 1 ms; neighbours within ±1 ms of P |
| TS-7 | Keep section [a, b] | r = 1 exactly inside; grid after b has spacing P; WarpMap.validate passes |
| TS-8 | Synthetic drift injection (TEMPO research §9) | classification accuracy, localisation ± 1 beat, tempo RMSE reported per release (A12) |
| TS-9 | Determinism | identical events and classes across runs; ties by lowest index |
