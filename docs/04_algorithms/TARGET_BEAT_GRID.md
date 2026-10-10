# Target Beat Grid

## Objective
Define the fixed-BPM target grid `T_k`, how its phase `φ` is chosen, and how source beats are assigned to grid slots.

## Why It Matters
The grid is the "answer" the render is forced onto. A wrong phase shifts the whole song; a wrong beat-to-slot assignment adds or removes a beat (audible as a skipped/duplicated beat or a ~1/BPM time jump). Both are catastrophic, unlike small ratio errors.

## Definitions

| Symbol | Meaning | Unit |
|---|---|---|
| `B` | target tempo | BPM |
| `P = 60 / B` | target beat period | s |
| `S_i`, `i = 0..n` | refined source anchor times (see WARP_FUNCTION.md §3) | s, source timeline |
| `k_i` | integer grid slot assigned to anchor `i` | – |
| `T_k = φ + k·P` | target grid line `k` | s, output timeline |
| `d_0` | index of the first confident downbeat | – |
| `M` | beats per bar (MVP: 4; 3 supported if detected) | – |

Times are always `float64` seconds or `int64` sample frames. **Never `float32`**: at t = 600 s the float32 spacing is 2^-14 s ≈ 61 µs ≈ 2.7 samples at 44.1 kHz, which shows up as anchor jitter.

## 1. Grid equation

```
P   = 60 / B
T_k = φ + k · P          k ∈ ℤ
```

## 2. Slot assignment (beat count preservation)

MVP rule: **one source beat → one target slot, in order**: `k_i = i - i_0` with `i_0` = index of the anchor chosen as phase reference. The number of beats is preserved. Tempo correction therefore changes durations but never adds or deletes musical beats.

Preconditions, enforced by the beat-tracking stage and not repaired here:
- the anchor sequence has had missing beats inferred and spurious beats removed (OUTLIER_REJECTION.md);
- the half/double decision has been made (HALF_DOUBLE_TIME_RESOLUTION.md), so that `S` and `B` refer to the same metrical level.

Consistency check, which must hold or the render is refused with a diagnostic:
```
source_bpm_local_i = 60 / (S_{i+1} - S_i)
assert  0.5 ≤ (B / source_bpm_local_i) ≤ 2.0   for all i       (hard bound)
warn if ratio outside [r_warn_lo, r_warn_hi]  (default 0.92 .. 1.08, see TIME_STRETCH_ENGINE.md)
```
If the user types a target that is close to half or double the detected tempo (|log2(B / B̂)| > 0.6), the UI offers to re-interpret the beat level instead of stretching 2×. Typing 140 for a song detected at 70 is a labelling choice, not a request to compress the song by 50 %.

### 2.1 Kept sections (intentional tempo change, "keep" decision)
If the user chooses **keep** for a detected step section `[i_a, i_b]` (TEMPO_STABILITY_ANALYSIS.md §7), its anchors map with r = 1:
`T_i = T_{i_a} + (S_i − S_{i_a})` for `i_a ≤ i ≤ i_b`. After the section the grid **resumes** with period P and a new phase: `T_i = T_{i_b} + (i − i_b)·P`.
The grid is therefore piecewise: one φ per fixed-tempo stretch. Bar counting continues across the section. The UI draws no grid lines inside the kept section, only its own beats.

## 3. Choosing the phase φ (render placement)

The output timeline is new, so φ is a policy choice rather than something to estimate. Three policies:

| Policy | Rule | Use |
|---|---|---|
| **Preserve start** (default) | `φ` such that `T_{k(d_0)} = S_{d_0}` | The first downbeat stays at the same wall-clock time as in the source. Pre-roll is untouched. Least surprising. |
| **DAW-ready start** | `T_{k(d_0)} = L_pre` where `L_pre = ceil(W_pre / L_bar) · L_bar`, `L_bar = M·P`, `W_pre` = warped pre-roll length | First downbeat lands exactly on a bar line counted from t = 0. Leading digital silence is prepended if needed (lossless). The file drops onto bar 1 or 2 of a DAW at tempo B. |
| **Least-squares** | `φ* = argmin Σ w_i (φ + k_i P − S_i)²` ⇒ `φ* = Σ w_i (S_i − k_i P) / Σ w_i` | Minimises total displacement. Used for **analysis plots only** (drift residuals), not for rendering. |

DAW-ready start is a deliberate product differentiator (see COMPETITIVE_ANALYSIS.md). It costs nothing in fidelity because it only prepends zeros.

## 4. Estimating phase robustly (analysis use)

To visualise "how far is each beat from an ideal constant grid at the detected tempo B̂", we need a robust phase for a period `P̂ = 60/B̂`. A naive mean of `S_i mod P̂` breaks at wrap-around. Use circular statistics:

```
θ_i     = 2π · (S_i mod P̂) / P̂
C       = Σ w_i cos θ_i ,  Sn = Σ w_i sin θ_i
θ̄       = atan2(Sn, C)
φ̂       = P̂ · θ̄ / 2π
R       = sqrt(C² + Sn²) / Σ w_i          # mean resultant length ∈ [0,1]
```
- `R` near 1 means the beats are tightly phase-locked to a constant grid, so a constant tempo is plausible. `R` falling below about 0.9 over a song means drift or tempo change. R is one input to the **tempo stability score** (TEMPO_STABILITY_ANALYSIS.md).
- Robustness: compute the weighted circular **median**, or repeat the estimate after discarding anchors with `|wrap(θ_i − θ̄)| > π/4` (≈ 1/8 beat). This is one IRLS pass.
- Weights `w_i` are the anchor confidences (BEAT_TRACKING.md).

Phase is only meaningful with the period. With drift, use the residual of a robust linear fit instead (BPM_ESTIMATION.md §3): `e_i = S_i − (a + b·i)`.

## 5. Pre-roll and post-roll

Audio before the first anchor `S_0` and after the last anchor `S_n` has no anchors. The warp is extended with a constant ratio equal to the smoothed edge ratio (WARP_FUNCTION.md §6). The grid itself is defined for all `k`, so the corrected waveform view still shows grid lines across intros.

### 5.1 Output origin
Output sample 0 always corresponds to source sample 0 (no audio is cut). With the edge ratio `r_0`, `W(0) = T_0 − r_0·x_0`:
- `W(0) > 0` (pre-roll compressed): `[0, W(0))` of the output is digital silence. It is tiny, since `|W(0)| ≤ |1 − r_0|·x_0`.
- `W(0) < 0` (pre-roll stretched): the whole output is shifted later by `|W(0)|`, so Preserve start holds to within `|W(0)|` (typically < 20 ms). The shift is reported in the warp-map diagnostics.

## 6. Worked example

Source anchors (s): `S = [1.000, 1.556, 2.111, 2.660, 3.205]`, so IOIs are 0.556, 0.555, 0.549, 0.545 (local 107.9 → 110.1 BPM). Target B = 110, so P = 0.545455 s. Preserve start with d_0 = 0 gives φ = 1.000.

| i | S_i | T_i = 1 + i·P | T_i − S_i |
|---|---|---|---|
| 0 | 1.000 | 1.000000 | 0 |
| 1 | 1.556 | 1.545455 | −10.5 ms |
| 2 | 2.111 | 2.090909 | −20.1 ms |
| 3 | 2.660 | 2.636364 | −23.6 ms |
| 4 | 3.205 | 3.181818 | −23.2 ms |

Why fractional precision matters: an error of only 0.1 BPM in B at 110 BPM adds up over a 5-minute song (≈ 550 beats) to `550 · (60/110 − 60/110.1) = 0.27 s`. That is half a beat. The target must keep full `float64` precision, and the UI accepts at least 2 decimals.

## Risks
- Bad `d_0` (downbeat) produces a bar-misaligned DAW-ready export. Mitigation: downbeat confidence gate, plus a one-click "shift downbeat ±1 beat" control (DOWNBEAT_ESTIMATION.md).
- Beat-count assignment fails if outlier repair is wrong. Mitigation: before render, check `max |T_i − S_i|` against expectations and warn on any single-segment ratio outside the warn band.

## Open Questions
- Should DAW-ready start pad to bar 1 (t = 0) or to bar 2 (one bar of silence lead-in, which DAW users often prefer)? Proposed: a setting, defaulting to "next bar line ≥ warped pre-roll".

## Implementation Implications
- `TargetGrid { double bpm; double phase; int beatsPerBar; int64 downbeatSlot; }` is pure data. The functions are `slotTime(k)`, `nearestSlot(t)` and `phaseFromAnchors(S, w, P)`.
- Unit tests: grid spacing exactness over 10 000 beats (error < 1e-9 s), circular phase across the wrap boundary, DAW-ready padding arithmetic.
