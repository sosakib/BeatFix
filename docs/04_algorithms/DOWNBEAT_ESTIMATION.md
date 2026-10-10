# Downbeat and Meter Estimation

Status: algorithm spec, 2026-10-08. Sources: [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) §4, F3, F13; [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) §3.

## Objective
Assign every decoded beat a bar position `k_i ∈ {0..M−1}` (0 = downbeat) and a meter M, with a confidence, using beat-synchronous decoding over the already-decoded beats ([BEAT_TRACKING.md](BEAT_TRACKING.md)). Support the user corrections "Set bar 1" and meter 3/4 ↔ 4/4.

## Why It Matters
With all beats used as anchors, downbeats **do not change the per-beat warp** (BEAT_TRACKING_RESEARCH §4). They matter for:
1. Bar numbers, bar lines and the bar.beat display (WAVEFORM_VIEW.md).
2. The phase reference `d_0` for the grid: Preserve-start and DAW-ready start place `d_0` ([TARGET_BEAT_GRID.md](TARGET_BEAT_GRID.md) §3). A wrong `d_0` gives a bar-misaligned DAW-ready export.
3. Evidence for metrical level: `down(c)` in [HALF_DOUBLE_TIME_RESOLUTION.md](HALF_DOUBLE_TIME_RESOLUTION.md) §3.
4. Change-point and splice reporting in bars ("tempo change at bar 33", [TEMPO_STABILITY_ANALYSIS.md](TEMPO_STABILITY_ANALYSIS.md)).

So a downbeat error is a **labelling** error (Low severity for the render) unless DAW-ready start is used, where it becomes a visible bar offset.

## Scope
| Meter | MVP | Notes |
|---|---|---|
| 4 (4/4) | Default | Prior favours 4 |
| 3 (3/4) | Detected | Switched on when the evidence wins |
| 2, 5, 6, 7, mixed per section | **Post-MVP** | 2/4 is displayed as 4/4 with half the bars (harmless); 6/8 felt in 2 is reported as M = 4 at the ×2 level or flagged Low |
| Irregular bars (2-beat bar at a Suno seam, UNVERIFIED) | Detected and flagged | Not "corrected"; bars after it renumber |

## 1. Inputs
- Refined beats `S_i`, confidences `c_i` ([BEAT_TRACKING.md](BEAT_TRACKING.md)).
- Downbeat probability `p_down` at 50 fps (persisted).
- `kickAt`, `snareAt` band evidence ([ONSET_DETECTION.md](ONSET_DETECTION.md) §6).

Beat-synchronous downbeat evidence:
```
d_i = max p_down(t) for |t − S_i| ≤ 35 ms           (0 if no frame inside; ε-clipped to [0.01, 0.99])
```
This replaces Beat This's "snap each downbeat to the nearest beat" post-processing (F3), which has no bar-length constraint and can emit irregular bars.

## 2. Bar-phase decoding (beat-synchronous Viterbi)
Hidden state per beat: `(M, k)`, M ∈ {3, 4}, k ∈ 0..M−1 → 7 states. Emission log-score:
```
e_i(M, k) = w_d · [ k = 0 ] · ln d_i  +  w_d · [ k ≠ 0 ] · ln(1 − d_i)          downbeat activation
          + w_b · bbTerm_i(M, k)                                                backbeat (weak)
bbTerm_i(4, k) = +snareAt(S_i) if k ∈ {1,3};  +kickAt(S_i) if k ∈ {0,2};  −(other) otherwise, scaled to [−1, 1]
bbTerm_i(3, k) = +kickAt(S_i) if k = 0, 0 otherwise
emissions are multiplied by c_i (low-confidence beats carry little evidence)
```
Transitions (costs in nats, placeholders, calibrated on corpus category 19):

| From → to | Cost | Meaning |
|---|---|---|
| (M,k) → (M,(k+1) mod M) | 0 | regular bar |
| (M,k) → (M,k'), k' ≠ (k+1) mod M | c_irr = 6 | irregular bar (shortened, lengthened, or a phase jump) |
| (M, M−1) → (M', 0), M' ≠ M | c_meter = 10 | meter change at a bar line |

Initial log-prior: `ln π(4, k) = 0`, `ln π(3, k) = −2` (MVP favours 4/4), uniform over k. Viterbi in float64, ties → lowest state index (M = 3 before 4, then k ascending; fixed order, documented). Cost ≈ 7² × n ≈ 3·10⁴ operations per song.

Why beat-synchronous rather than frame-level bar-pointer HMM: beats are already decided, so the state space is 7, not ~10⁴–10⁵ (Krebs 2015). It is exact and trivially testable. Why Viterbi rather than window voting (research §4 proposes 16-bar windows with a Viterbi over windows): the same idea at beat resolution localises irregular bars to one beat instead of one window. **For legal review**: the unverified "DBN is patented" claim (BEAT_TRACKING_RESEARCH §2.3) targets bar-pointer DBNs. If counsel considers any HMM over bar position covered, replace the Viterbi with per-window argmax voting (same emissions, no transitions, irregular bars inferred where the winning phase changes).

Output: `k_i`, `M_i`, `isDownbeat_i = (k_i == 0)`, `barIndex_i` (counting from the bar containing `d_0`, negative for pickup bars), `irregularBars[]` (bar index, length in beats).

## 3. Confidence
Phase margin per bar window (4 bars, hop 1 bar):
```
m_w = Σ_{i ∈ w} [ e_i(M*, k*_i) − max_{s ∈ 1..M*−1} e_i(M*, (k*_i + s) mod M*) ] / |w|
```
i.e. how much better the decoded phase explains the window than the best shifted phase. Meter margin: the same with the best path under the other meter (forced decode).

| Output | Rule (placeholders) |
|---|---|
| `downbeatConf_bar` | σ(a·m_w + b), a, b fitted on corpus downbeat F (A7) |
| `downbeatConf_global` | median of `downbeatConf_bar` over bars with mean c_i > 0.5 |
| `d_0` | first downbeat whose `downbeatConf_bar ≥ 0.6`. Earlier downbeats are extrapolated backward and marked `inferred` |
| `meterConf` | σ(meter margin per beat). < 0.6 → "Meter uncertain (3/4 or 4/4)" Issues entry |

Low `downbeatConf_global` (< 0.5) with DAW-ready start active raises an Issues entry asking the user to check bar 1 (TARGET_BEAT_GRID Risks).

## 4. User corrections (MVP edits, undoable; UX_BEHAVIOR.md §5.4–5.5)
- **Set bar 1 at cursor**: the beat j nearest the cursor (within ±½ beat) becomes a downbeat and the phase reference. Implementation: re-run §2 with state at beat j clamped to (M_j, 0) (all other evidence unchanged), then set `d_0 = j`. A clamped re-decode keeps irregular bars detected elsewhere, which a global modulo shift would destroy. Deterministic, < 1 ms.
- **Shift downbeat ±1 beat** (keyboard): equivalent to Set bar 1 at beat `d_0 ± 1`.
- **Meter 3/4 / 4/4**: re-run §2 with M fixed for the whole song (transitions to the other meter removed). Bar labels change. The warp map does not (anchors are beats). DAW-ready padding is recomputed because `L_bar = M·P` changes.
- Edits are stored as inputs ([WARP_MAP.md](WARP_MAP.md) "user edits") and re-applied after a ×2/÷2 interpretation edit only if still valid. After ÷2, a clamped beat that no longer exists is dropped and the user is told.

## 5. Interaction with metrical level
- After ×2 relabelling, an M = 4 bar spans 4 new beats = 2 old beats: the decoder will usually find downbeats every 8 new beats, which shows up as M = 4 with every other bar "weak" or a low margin. Downbeat consistency is therefore a level feature (`down(c)`, HALF_DOUBLE §3), and §2 always runs **after** the level is chosen.
- Halftime sections (TEMPO_STABILITY "Level switch") keep the song's beat level, so bars there still have M beats of the song level. They are labelled "half-time feel", not re-metered.

## 6. Fallback (DSP mode, no downbeat activation)
`d_i` is replaced by a kick/backbeat proxy: `d_i = σ(β·(kickAt(S_i) − snareAt(S_i)))`, β = 4 (placeholder). The meter is fixed to 4 unless the user sets 3. Downbeat confidence is capped at 0.5, so the UI always asks the user to confirm bar 1 in DSP mode.

## 7. Failure modes
| Failure | Cause | Effect | Mitigation |
|---|---|---|---|
| Off-by-1/2-beat bar phase | weak downbeat activation, kick on every beat (four-on-the-floor) | wrong bar numbers; DAW-ready export one or two beats off | margin → Issues; Set bar 1 |
| Pickup (anacrusis) | song starts mid-bar | bar 1 placed on the pickup | `d_0` = first *confident* downbeat, negative bar indices for pickup |
| 3 vs 4 confusion | waltz-like accent patterns, 6/8 ballads | wrong bar length | meter prior + margin; meter override |
| Irregular bar at a seam | Suno Extend splice (UNVERIFIED) | phase shift after the seam | detected by c_irr transition, flagged with time; cross-referenced with TEMPO_STABILITY splice events |
| Drumless intro | no downbeat evidence | extrapolated bars | `inferred`, confirm if DAW-ready start |
| Halftime / double-time level error | wrong level upstream | 8-beat or 2-beat bars | resolved upstream by HALF_DOUBLE; low meter margin is a symptom |

## Alternatives Considered
| Option | Verdict | Why |
|---|---|---|
| Beat This minimal PP (snap downbeats to nearest beat) | Reject | No bar-length constraint, irregular bars (research §4) |
| madmom `DBNDownBeatTrackingProcessor` | Reject | madmom runtime dependency, fixed meters, patent claim |
| Frame-level joint beat+bar HMM | Reject | Couples modules, large state space, same patent question |
| Kick/snare templates only (Goto 2001) | Fallback only | Genre-dependent, fails on non-backbeat music |
| All-In-One segment labels to explain bar irregularities | Post-MVP | Demucs/NATTEN dependency |

## Open Questions
1. How often do Suno outputs contain irregular bars at Extend seams? Needs annotated U-tier data.
2. Is 6/8 common enough in Suno output to justify MVP support beyond the flag?
3. Calibrated values for c_irr, c_meter, w_d, w_b.

## Tests
| ID | Input | Assertion |
|---|---|---|
| DB-1 | S08 rock backbeat 4/4 with synthetic downbeat activation (1 on bar starts, 0.2 elsewhere) | all `k_i` correct; `downbeatConf_global` ≥ 0.9 |
| DB-2 | M-tier 3/4 waltz | M = 3 throughout; downbeat F ≥ 0.9 (A7) |
| DB-3 | S08 with one 2-beat bar inserted at bar 17 | exactly one irregular bar reported at bar 17 (± 0); bars after it correct |
| DB-4 | Set bar 1 at beat 5 on DB-1 input | beat 5 is a downbeat, `d_0 = 5`, bar labels of the rest follow, irregular bars elsewhere unchanged |
| DB-5 | Meter override 3 on a 4/4 song | M = 3 everywhere; warp map byte-identical to before (anchors unchanged) |
| DB-6 | Pickup: song starts on beat 4 | `barIndex` of the first beat = −1, `d_0` = beat 1 |
| DB-7 | DSP mode on S08 | meter 4, confidence ≤ 0.5, kick-on-1 phase found |
| DB-8 | Determinism | identical `k_i` for identical inputs; tie-breaking order covered by a constructed tie case |
