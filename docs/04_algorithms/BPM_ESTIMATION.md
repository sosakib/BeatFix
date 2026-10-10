# BPM Estimation: Statistics of Tempo from Beat Positions

## Objective
Turn a sequence of beat times into (a) a global BPM with an honest uncertainty, (b) a local tempo curve, and (c) a ranked list of tempo candidates (brief §7.1, §7.2). Detection itself is in BEAT_TRACKING.md, octave resolution in HALF_DOUBLE_TIME_RESOLUTION.md, and drift classification in TEMPO_STABILITY_ANALYSIS.md.

## Why It Matters
"The BPM" of a drifting song is not well defined. A careless estimator (mean of IOIs, median of quantised IOIs) is off by whole BPM. At 110 BPM, a 0.1 BPM error accumulates to 0.27 s over 5 minutes (TARGET_BEAT_GRID.md §6).

## 1. Basics
```
BPM = 60 / Δt                      Δt = beat interval in seconds
Δt_i = S_i − S_{i−1}               (i = 1..n)
BPM_i = 60 / Δt_i                  (instantaneous / per-beat tempo)
```
Error propagation: `σ_BPM ≈ 60 · σ_Δt / Δt²`.

## 2. Why per-beat BPM_i is nearly useless on its own
With independent detection error σ per beat, `σ_Δt = √2·σ`. For σ = 10 ms at 110 BPM (Δt = 0.5455 s):
```
σ_BPM_i ≈ 60 · 0.01414 / 0.2975 ≈ 2.85 BPM
```
So a perfectly steady 110 BPM song shows per-beat readings scattered roughly 107–113. Consecutive IOIs share one detection error with opposite sign, so with pure detector noise **lag-1 autocorrelation of IOIs ≈ −0.5**. Real tempo drift produces *positive* lag-1 autocorrelation. This signature separates noise from drift (TEMPO_STABILITY_ANALYSIS.md §2).

Where BPM_i fails: frame-quantised beats, missed or extra beats (which halve or double one IOI), swing or syncopated anchors, and fills.

## 3. Global tempo estimators compared

| Estimator | Formula | Problem | Verdict |
|---|---|---|---|
| Mean IOI | `mean Δt = (S_n − S_0)/n` | The sum **telescopes**: only the first and last beat matter. One bad endpoint (100 ms off over 400 beats) shifts the result by 0.05 BPM | Reject |
| Median IOI | `median(Δt)` | Robust to outliers, but with frame-quantised beats (20 ms frames) IOIs cluster on the frame lattice and the median **snaps to 0.54 or 0.56 s (111.1 / 107.1 BPM)** | Use only on refined anchors, and only as a sanity check |
| **Robust linear fit** (recommended) | `S_i ≈ a + b·i`, Huber IRLS; `BPM = 60/b` | Assumes constant tempo. On drifting songs it returns the best constant fit, which is what a fixed-grid target wants | **Use** |
| Theil–Sen | `b = median_{j<k} (S_k − S_j)/(k − j)` | O(n²): about 2×10⁵ pairs for 600 beats, which is fine. 29 % breakdown point | Use as a cross-check. A disagreement > 0.05 BPM is flagged |
| Tempogram / autocorrelation peak | from onset envelope | Bin resolution is coarse unless interpolated, and it doesn't use beat phase | Candidate generation only |

### Uncertainty of the linear fit
For N = n+1 beats with i.i.d. residual σ:
```
SE(b) = σ · sqrt( 12 / (N (N² − 1)) )
```
N = 400, σ = 10 ms gives SE(b) ≈ 4.3 µs, so **SE(BPM) ≈ 0.0009 BPM**. Jitter is therefore *not* what limits global BPM accuracy. **Model mismatch (drift) is.** Report two things:
- `bpmGlobal ± CI` using the residual-bootstrap 95 % CI (block bootstrap with block = 8 beats, because drift makes residuals correlated);
- `bpmRange = [p5, p95]` of the smoothed local tempo curve (§4). For a drifting song this range is the honest answer, and the UI shows it ("108.2 – 112.9 BPM, drifting").

## 4. Local tempo curve
Computed from the smoothed anchors `x_i` produced by the same Whittaker/IRLS smoother used for warping (WARP_FUNCTION.md §4), evaluated with a fixed display cutoff N_c = 8 beats regardless of the user's correction setting, so the graph doesn't change when the slider moves:
```
localBpm_i = 60 / (x_{i+1} − x_i)
```
Alternatives evaluated:
- moving median of BPM_i (window 8): simple, but stair-steppy and biased at quantised anchors;
- Kalman constant-velocity on (phase, period) with RTS smoothing: equivalent quality, more parameters;
- LOESS: fine, but slower and with no advantage.
The Whittaker smoother is chosen so the warp and the display share one implementation (no divergent "displayed vs applied" tempo).

The time-domain curve for plotting is `localBpm(t)`, piecewise constant on `[x_i, x_{i+1})`, or linearly interpolated at segment midpoints.

## 5. Integer vs fractional BPM
- Always estimate fractionally (float64).
- **Default target suggestion** = `round(bpmGlobal)`. The snap is shown as "detected 109.96 → suggested 110". Rationale: fixed-BPM production workflows use integer tempi, and AI generators are *believed* to be prompted or conditioned at integer tempi (**UNVERIFIED**, see BEAT_TRACKING_RESEARCH.md).
- If `|bpmGlobal − round(bpmGlobal)| > 0.25` and the song is stable, also offer the 0.5 grid value (e.g. 127.5).
- The user can type any value with 0.01 resolution.

## 6. Candidate ranking
Candidates are `B̂ × {1/3, 1/2, 2/3, 1, 3/2, 2, 3}`, where B̂ comes from the beat tracker / tempogram, filtered to the **40–250 BPM** range. A floor of 55 BPM would force double-tempo answers on about 21 % of slow SMC tracks (BEAT_TRACKING_RESEARCH.md). Each candidate gets a score (HALF_DOUBLE_TIME_RESOLUTION.md):
```
score(c) = α·metricalSalience(c) + β·log prior(c) + γ·downbeatConsistency(c)
```
The UI shows the top candidate, plus any candidate whose score is within the ambiguity margin as a one-click alternative.

## 7. BPM confidence (High / Medium / Low)
Combines:
| Feature | Meaning |
|---|---|
| `meanActivation` at anchors | how strongly the tracker believed in its beats |
| `residualScale / P` (robust) | how well a smooth tempo explains the beats |
| `R` phase coherence (TARGET_BEAT_GRID.md §4) | constant-grid fit |
| candidate margin | octave ambiguity |
| fraction of `inferred` beats | gaps |

Thresholds are **not fixed by assumption**. They are calibrated on the benchmark corpus (BENCHMARK_CORPUS.md) so that "High" means < 2 % octave-error rate and median |Δ BPM| < 0.1 on the stable-tempo subset. The initial placeholder values live in a single config file and are logged with each analysis.

## 8. Pseudocode
```
function estimateTempo(S_refined, conf):
    i = 0..n
    (a, b) = huberIRLS_linefit(i, S_refined, weights=conf)
    bTS    = theilSen(i, S_refined)
    bpmGlobal = 60 / b
    flag crossCheck if |60/bTS − bpmGlobal| > 0.05
    ci = blockBootstrapCI(residuals, block=8, B=500)  # deterministic seed
    x  = whittakerIRLS(S_refined, conf, N_c=8)        # shared with warp
    localBpm = 60 / diff(x)
    bpmRange = percentile(localBpm, [5, 95])
    return {bpmGlobal, ci, bpmRange, localBpm, suggested=round(bpmGlobal)}
```
Fixed RNG seed: the bootstrap must be deterministic so that analysis results reproduce.

## Tests
- Synthetic metronome at 110.000, 109.96 and 127.5 BPM with σ = 0/5/15 ms jitter: |error| < 0.01 BPM for σ ≤ 15 ms over ≥ 200 beats.
- Endpoint outlier (+100 ms on the last beat): estimate moves < 0.005 BPM (shows the mean-IOI failure is avoided).
- Linear ramp 108 → 113: `bpmRange` covers [108.3, 112.7] within ±0.3 and bpmGlobal is about 110.5.
- Frame-quantised input (20 ms lattice) after refinement: no snapping to 107.1/111.1.
