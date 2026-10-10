# Outlier Rejection: Missing, Extra and Unreliable Beats

Status: algorithm spec, 2026-10-08. Sources: [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) §5–6; [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) §5; brief §7.9.

## Objective
Repair the decoded beat sequence so that **exactly one anchor exists per musical beat at the chosen metrical level**, and down-weight beats that exist but are poorly placed. Covers missing kicks, double-detected transients, ghost notes, fills, toms, claps, snares, syncopation and non-percussive onsets.

## Why It Matters
[TARGET_BEAT_GRID.md](TARGET_BEAT_GRID.md) §2 maps beat i to slot i. A missing beat makes one IOI ≈ 2P, so that segment is compressed ~2× **and every later beat lands one slot early**: the rest of the song is shifted by a beat relative to the grid. An extra beat does the opposite. Both are catastrophic and neither can be fixed by smoothing. Small misplacements (jitter, a beat pulled toward a fill hit) are not catastrophic. They are handled by weights, not by insertion or deletion.

## 1. Principle: anchors come from the decoder, not from onsets
- The beat set is produced by the DP decoder on Beat This activations with a period prior ([BEAT_TRACKING.md](BEAT_TRACKING.md) §3). Raw onsets never create or remove beats.
- Onsets (ONSET_DETECTION.md) only (a) refine a decoded beat within ±25 ms, (b) supply evidence and weights.
- So ghost notes, hi-hat 16ths, syncopated pushes and tom runs cannot become anchors unless the activation and the DP both put a beat there.

Local period used by all tests:
```
P_loc,i = median{ IOI_j : |j − i| ≤ 8, j ≠ i, IOI_j not flagged }      (≥ 5 values, else τ(S_i)/50 from the period path)
IOI_i   = S_i − S_{i−1}
q_i     = IOI_i / P_loc,i
```
The median over ±8 beats tolerates up to 50 % bad IOIs in the window. The fallback to the period path τ covers sparse regions.

## 2. Repair rules (applied after decoding, before refinement)
Frame-quantised times are adequate here: the ratio windows are ±0.3 wide versus a quantisation error of ≤ 0.04 at 120 BPM.

| Pattern | Test | Action | Flags / weight |
|---|---|---|---|
| **Missing beat(s)** | `q_i ∈ [m − 0.3, m + 0.3]` for integer m = 2..4 | Insert m−1 beats. Beat k at `S_{i−1} + k·IOI_i/m`, then moved to the max of `p_beat` within ±15 % of P_loc if that max is a local peak with p > 0.2 | `inferred`; `c ≤ 0.2` |
| **Long gap** | `q_i > 4.3` (breakdown, drumless section) | Should not occur: the DP bridges gaps at ~τ spacing. If it does, insert `round(q_i) − 1` beats at equal spacing | `inferred`; region flagged "no pulse" |
| **Extra beat** | `q_i < 0.65` | Candidate removals: `S_{i−1}` or `S_i`. Remove the one that minimises `Σ |ln q|` over the affected IOIs after removal. Ties → remove lower `p_beat`; then the higher index | neighbours `repaired` |
| **Ambiguous IOI** | `q_i ∈ (0.65, 0.85) ∪ (1.15, 1.7)` | **No insertion or deletion.** Beat kept | `irregularIOI`; c_i lowered via `ioi_i`. A run of these is a splice/step candidate for TEMPO_STABILITY_ANALYSIS |
| **Sustained level switch** | ≥ 8 consecutive `q ≈ 2` or `≈ 0.5` (within ±0.2) | Re-decode the span with the corridor re-centred on the song level. Should be impossible in the main path (corridor, BEAT_TRACKING §2). Mainly guards the fallback path and user-forced levels | `levelSwitch` event |

Rules run in one left-to-right pass, then the whole pass repeats until no change (max 3 passes; deterministic order). A repair never moves an existing beat by more than ±15 % P_loc. The dead zones between rule windows are deliberate: an IOI of 1.4·P is more likely a mis-placed beat or a splice than a missing beat, and guessing wrong would shift the whole song by a slot.

## 3. Ghost notes, fills, toms, claps, syncopation, non-percussive onsets
| Situation | Expected ML / DP behaviour | BeatFix handling |
|---|---|---|
| Ghost snares, hi-hat 16ths | ignored by the learned metrical salience | `ghostRatio` keeps ghost peaks out of the snap candidates: a snap peak must have salience ≥ 0.5 of the median backbeat peak |
| Drum fills / tom runs | activation may weaken or add off-grid peaks | the DP period penalty keeps spacing. Bars with `fillScore` > 2× median: **no transient snap**, `c_i × 0.5`. The smoother then follows the surrounding tempo through the fill |
| Claps / snare flams (double transients within ~10–30 ms) | single activation peak | snap refused when two hr peaks ≥ 80 % salience lie within ±10 ms (BEAT_TRACKING §4); flag `flam` |
| Double-detected transient (decoder picks both hits of a flam 20–40 ms apart) | rare | `q < 0.65` extra-beat rule removes one |
| Syncopation / anticipations (push on the "and") | usually correct with ML | DP + long context. Snap limited to ±25 ms, so a push 1/8 note early (≥ 200 ms at 150 BPM) can never capture the anchor |
| Missing kick (breakdown) | activation drops | the DP bridges, repair inserts if needed, `inferred` beats with low weight; warp follows the smoothed tempo |
| Non-percussive intro (pads, vocals) | low or ambiguous activation | period path carries the tempo backward from the first confident region; beats `inferred`; first downbeat needs confirmation (DOWNBEAT_ESTIMATION §3) |
| Fade-out / reverb tail | activation decays | trailing beats with p_beat below threshold for ≥ 2 bars are dropped; edge ratio extends W (WARP_FUNCTION §6) |

## 4. Remaining outliers: weights, not deletion
After repair and refinement, isolated misplaced anchors are handled by the robust smoother in [WARP_FUNCTION.md](WARP_FUNCTION.md) §4.2:
```
w_i = c_i · huberWeight( (S_i − x_i) / s ),   s = 1.4826 · MAD(S − x),   k = 1.345,   3–5 IRLS iterations
```
- A beat 30 ms off in an otherwise steady passage gets weight ≈ k·s/30 ms and barely bends the curve.
- Gate (WARP_FUNCTION Risks): if the robust residual scale s > 0.15·P, the analysis is marked unreliable and no automatic render starts.
- Confidence weights `c_i` (BEAT_TRACKING §5.1) already include `ioi_i` and `res_i`, so `inferred`, `flam`, fill and `irregularIOI` beats pull weakly.

Flags written to the anchor record ([WARP_MAP.md](WARP_MAP.md) `inferred`, plus the analysis-side flag set): `inferred`, `repaired`, `snapped`, `flam`, `inFill`, `irregularIOI`, `levelSwitch`, `damaged`.

## 5. Pseudocode
```
function repairBeats(S, pBeat, tau):
    for pass in 1..3:
        changed = false
        for i in 1..len(S)−1:                               # ascending, deterministic
            q = (S[i] − S[i−1]) / Ploc(S, i, tau)
            if q > 4.3 or (round(q) in 2..4 and |q − round(q)| ≤ 0.3):
                m = round(q); insert m−1 beats in (S[i−1], S[i]) snapped to pBeat peaks within ±0.15·Ploc
                flag inferred; changed = true; break        # restart scan after an edit
            if q < 0.65:
                drop = argmin over {i−1, i} of Σ|ln q| after removal  (ties: lower pBeat, then higher index)
                remove S[drop]; flag neighbours repaired; changed = true; break
            if q in (0.65, 0.85) ∪ (1.15, 1.7): flag irregularIOI(i)
        if not changed: break
    checkSustainedLevelSwitch(S)                            # ≥ 8 consecutive q ≈ 2 or 0.5 → re-decode span
    return S, flags
```
Worst case O(n²) with restarts (n ≤ ~2000 beats): negligible.

## Alternatives Considered
| Option | Verdict | Why |
|---|---|---|
| Onset-driven anchors (every strong onset a beat) | Reject | Ghosts, fills and syncopation become anchors (brief §7.9) |
| Delete any beat with a large residual | Reject | Deleting shifts all later slots; weights are safe, deletion is not |
| Insert on any IOI > 1.5·P | Reject | A mis-placed beat or splice would get a spurious extra slot |
| Global re-decode with a stricter α instead of repair | Partial | Over-tight α erases genuine steps; repair is local and auditable |

## Risks
- A genuine 2-beat pause (stop-time, fermata) with no activation gets beats inserted. Acceptable: the warp follows the smoothed tempo through it, the beats are low-weight, and the gap is flagged.
- Correlated failures (whole fills misdecoded for a bar) defeat the ±8-beat median. Mitigation: the fill down-weighting plus the smoother gate.

## Tests
| ID | Input | Assertion |
|---|---|---|
| OR-1 | S06: 5/10/20 % random beats removed from the activation | after repair, beat count = ground truth; inserted beats within ±10 ms (A9 FN = 0) |
| OR-2 | S06: 4 full bars removed (breakdown) | 16 inferred beats, max ratio deviation in the region ≤ 1 % after smoothing |
| OR-3 | Extra peaks injected midway between beats on 5 % of beats | all removed; no true beat removed (A9 FP = 0, FN = 0) |
| OR-4 | S07 flams +15 ms | no extra beats; `flam` flags on flam beats; anchors at the ML time ±2 ms |
| OR-5 | IOI of 1.4·P constructed (one beat displaced 200 ms) | no insertion/deletion; `irregularIOI` flagged; Huber weight of the displaced beat < 0.3 |
| OR-6 | S08 syncopated (kick on "and of 2") | no anchors on the syncopated hits; A9 FP ≤ 2 % |
| OR-7 | M-tier song with a 2-bar tom fill | no snaps inside the fill; max |x_i − true beat| in the fill ≤ 15 ms |
| OR-8 | Determinism | repeated runs and permuted thread scheduling give identical results; tie cases covered |
