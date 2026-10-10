# Half-Time / Double-Time Resolution (Metrical Level)

Status: algorithm spec, 2026-10-08. Decision: ADR-004 (octave candidates ×{1/3, 1/2, 2/3, 1, 3/2, 2, 3} in 40–250 BPM, Green/Amber/Red margin, ask on Amber/Red; [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1). Sources: [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) T1–T8, §2–3; [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) §1.5, F7; brief §7.8.

## Objective
Choose the metrical level (the "beat") the whole song is tracked and gridded at, with an explicit margin, and hand ambiguous cases to the user as one-click alternatives. Worked cases: 55 vs 110, 70 vs 140, 110 vs 220.

## Why It Matters
Even SOTA tempo estimation is octave-correct only ~83–87 % of the time (T1–T2). But for BeatFix the cost of an octave error depends entirely on its *consistency*:

### A consistent octave error is (nearly) harmless to the render
Suppose the true beats are `s_i` and the tracker consistently reports the ×2 level: `S'_{2i} = s_i`, `S'_{2i+1} = (s_i + s_{i+1})/2 + δ_i` (δ_i = off-beat micro-timing, e.g. swing). The displayed BPM doubles, the suggested target becomes `2B`, so `P' = P/2`. For straight eighths (δ = 0):
```
r'_{2i} = r'_{2i+1} = (P/2) / ((s_{i+1} − s_i)/2) = P / (s_{i+1} − s_i) = r_i
```
The warp W is the same function, the render is the same audio. Only the label differs (BEAT_TRACKING_RESEARCH "Why It Matters": Low severity). Two second-order effects, documented so nobody is surprised:
- **Strict mode at the ×2 level quantises off-beats too.** With swing (δ_i ≠ 0), Strict pulls every off-beat to the half-period and straightens the swing. Natural mode keeps it, because a period-2 deviation is faster than any N_c ≥ 4 ([WARP_FUNCTION.md](WARP_FUNCTION.md) §4.3).
- **N_c is counted in beats of the chosen level.** At the ×2 level, N_c = 8 means 4 true beats (a stiffer correction); at ½ it means 16 true beats. Small effect; noted as an open question.

At the ½ level every other true beat is unanchored and linearly interpolated: fine for drift, slightly less precise for jitter.

### A mid-song level switch is catastrophic
If the decoded level switches from c to 2c at beat j, every later IOI halves while the grid period stays P, so `r ≈ P/(P/2) = 2`: the rest of the song is stretched ~2× (tempo halves, duration grows). This breaks the hard ratio bound (WARP_FUNCTION §3: 0.5..2.0) and the render is refused, or, for a short switch, produces an audible lurch plus a beat-count shift. Hence the design: **choose the level once, then decode inside a corridor that cannot reach another octave** ([BEAT_TRACKING.md](BEAT_TRACKING.md) §2).

## 1. Candidate generation
From [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) §2:
```
x     = p_beat (50 fps)  or  obs_dsp (fallback)
G     = sqrt(norm(ACF tempogram) · norm(Fourier tempogram))   # opposite octave biases cancel (T8)
          log-BPM bins, 1 % spacing, 40–250 BPM (≈ 184 bins), window 10 s, hop 0.5 s
g     = median over time of G                                  # breakdowns don't dominate
peaks = top-5 local maxima of g, min separation 4 %
C     = { p · m : p ∈ peaks, m ∈ {1/3, 1/2, 2/3, 1, 3/2, 2, 3} } ∩ [40, 250]
merge candidates within 2 % (keep the higher salience); salience g(c) by interpolation
```
Closing the set under metrical relations guarantees the true level is a candidate even when its own tempogram peak is weak. ×3 and ÷3 cover 6/8, 12/8 and triplet-feel confusions. The 40 BPM floor matters: a 55 BPM floor forces double tempo on ~21 % of slow SMC tracks (T3).

## 2. Provisional decode per candidate
For each c ∈ C: decode with constant `τ = 50·60/c` (the DP of BEAT_TRACKING §3, no repair, no refinement) → beats `b_j(c)`. Cost: |C| ≤ ~15 decodes × 5·10⁵ ops. Bar phase for features: the best (M, phase) under DOWNBEAT §2 emissions without transitions (cheap argmax).

## 3. Scoring features
| Feature | Definition | What it detects |
|---|---|---|
| `sal(c)` | combined tempogram salience g(c) | periodic energy at this level |
| `act(c)` | mean p_beat at `b_j(c)` minus mean p_beat at midpoints `b_j + IOI/2` | ML's own metrical preference: a ×2 grid puts half its beats on low-activation frames |
| `down(c)` | fraction of p_down peaks (p > 0.5) within ±35 ms of every M-th beat for the best phase, M ∈ {3,4} | bars must contain an integer number of beats at the true level |
| `bb(c)` | `mean_bars[ S(b₂)+S(b₄) − S(b₁)−S(b₃) ] / (mean S + ε)  +  mean_bars[ L(b₁)+L(b₃) − L(b₂)−L(b₄) ] / (mean L + ε)`, S = `snareAt`, L = `kickAt` ([ONSET_DETECTION.md](ONSET_DETECTION.md) §6) | rock/pop/hip-hop backbeat: kick on 1 & 3, snare on 2 & 4. At 2c the snare lands on every 4th beat (bb ≈ 0) |
| `mid(c)` | `mean S(b_j + IOI/2) / mean S(b_j)` | > 1: the snare falls *between* beats ⇒ evidence for 2c |
| `prior(c)` | `−(log2(c/120))² / 2` (log-normal, σ = 1 octave) | weak perceptual tie-breaker (T4, T5) |
| `ioi(c)` | robust CV of decoded IOIs: 1.4826·MAD(IOI)/median(IOI) | wrong levels decode irregularly (fills hit some beats) |

```
Score(c) = w · [ z(sal), z(act), z(down), z(bb), −z((mid − 1)₊), prior, −z(ioi) ]
```
z(·) = standardisation across the candidate set (median/MAD). Initial weights `w = [1, 1.5, 1, 1, 1, 0.5, 0.5]` (placeholder), then fitted by multinomial logistic regression on the annotated corpus (label: annotated level). This generalises the 3-term score in [BPM_ESTIMATION.md](BPM_ESTIMATION.md) §6 (salience + prior + downbeat consistency). Weights and prior are logged per analysis.

Fallback (DSP) mode drops `act` and `down` and caps the result at Amber.

## 4. Margin → Green / Amber / Red
```
c1 = argmax Score
c2 = argmax Score over candidates metrically related to c1 (ratio ∈ {2, 3, 3/2} or inverse, ±2 %)
m  = Score(c1) − Score(c2)
```
| Margin | Level | Engine behaviour | UI (MAIN_WINDOW.md §4.4–4.5, UX_BEHAVIOR.md §4–5) |
|---|---|---|---|
| m ≥ m_hi (placeholder 2.0) | **Green** | accept c1 | High (if beat confidence also High); ½ / ×2 visible |
| m_lo ≤ m < m_hi (placeholder 0.5) | **Amber** | accept c1, c2 offered | Medium: "Check: 59.9 BPM (half-time) is also plausible" chip |
| m < m_lo | **Red** | accept c1 provisionally (ties → candidate nearer 120 BPM) | Low: candidate picker expanded, "Play with click to compare"; export asks for confirmation |

m_hi and m_lo are calibrated so that Green has an octave-error rate < 2 % on the annotated corpus (BPM_ESTIMATION §7) and every miss on the corpus is Amber or Red (target ≥ 95 % octave-correct, TEMPO research §9).

Only c1 is decoded with the corridor and full pipeline. A forced corridor decode at c2 is run once for `pathScore` margin (BEAT_TRACKING §5.2) and is cached so the ×2/÷2 click is instant.

## 5. Worked examples (illustrative feature values, not measurements)
Priors are exact: prior(55) = −0.633, prior(110) = −0.008, prior(220) = −0.382, prior(70) = −0.302, prior(140) = −0.025.

**55 vs 110 — slow soul ballad, true tactus 55 BPM, piano eighths at 110.**
| Feature | 55 | 110 | Reading |
|---|---|---|---|
| sal | lower | higher | eighth-note density favours 110 |
| act | +0.45 | +0.10 | Beat This fires mainly on the 55-level beats |
| down | 0.90 | 0.40 | at 110, downbeats fall every 8 beats, so no M ∈ {3,4} fits |
| bb | high | ≈ 0 | snare on 2 & 4 at 55 = beats 3 and 7 at 110 |
| mid | 0.3 | 0.2 | no off-beat snare at either level |
| prior | −0.633 | −0.008 | favours 110 |
Result: 55 wins on act, down and bb despite sal and prior; margin typically Amber because sal and prior disagree. User sees "55.0 BPM · Check: 110 (double-time) also plausible". Render is identical either way (consistent level).

**70 vs 140 — trap, hi-hats in 16ths at 140, snare on beat 3 of each 140-BPM bar ("halftime").**
At the 140 level a bar is 4 beats and the snare sits on beat 3, so `bb(140) < 0`. At the 70 level the same snare hits fall on beats 2 and 4, so `bb(70) > 0`. Beat This activation is often split between the levels. The backbeat convention therefore picks **70**, while producers label the song **140** (T-research §3 genre caveats). Expected outcome: Amber, "70.0 BPM · Check: 140 (double-time) also plausible". Since the render is octave-invariant, this is purely labelling; the user's choice is stored per project, never globally. Whether the default label should follow the producer convention is an open UX question (TEMPO research OQ4).

**110 vs 220 — pop at 110 with 16th-note hats.**
| Feature | 110 | 220 | Reading |
|---|---|---|---|
| sal | high | high | hats create strong 220 periodicity (Fourier tempogram) |
| act | +0.60 | +0.05 | ML strongly prefers 110 |
| down | 0.95 | 0.45 | bars of 8 beats at 220 |
| bb | high | ≈ 0 | |
| mid | 0.2 | — | no snare between 110-level beats ⇒ no evidence for 220 |
| prior | −0.008 | −0.382 | |
Result: 110, Green. This is why hats are excluded from `obs_dsp` and why the geometric-mean tempogram is used: hats inflate the faster level's salience but nothing else supports it.

**3:2 / ×3 case.** A 6/8 ballad: dotted-quarter pulse 60 BPM, eighth-note pulse 180 BPM. Candidates include 60, 90, 120 and 180. At 180, `down` fits M = 3 (bars of 3 eighths = half a 6/8 bar) or M = 6 (not in MVP); at 60, bars of 2 beats do not fit M ∈ {3, 4}. Expected outcome: Amber or Red with 60 and 180 offered. MVP meter support is 3 and 4 only ([DOWNBEAT_ESTIMATION.md](DOWNBEAT_ESTIMATION.md) scope), so bar labels for 6/8 may be wrong; the render is unaffected as long as the level is consistent.

## 6. Relabelling (user ×2 / ÷2 / candidate pick)
- An interpretation edit forces level c' and re-runs period path → decode → repair → refine → downbeats → stability → confidence on the persisted activations (BEAT_TRACKING §8). The activation model and DSP features never re-run. UX_BEHAVIOR.md §5.2 calls this "re-runs the warp build (not the analysis)": the expensive analysis is not repeated.
- ÷2 is not "drop every other beat": the re-decode chooses the phase that the activation and backbeat support (the dropped half are the off-beats).
- Target follows the interpretation when `targetMode = auto` (`round(c')`). If the user types a target with `|log2(B/B̂)| > 0.6`, the UI offers to relabel instead of stretching 2× ([TARGET_BEAT_GRID.md](TARGET_BEAT_GRID.md) §2).
- Edits persist as inputs ([WARP_MAP.md](WARP_MAP.md)) and are re-applied deterministically on load.

## 7. Engine → UI hand-off
`tempo.candidates[{bpm, score, relation ∈ {1, 1/2, 2, 1/3, 3, 2/3, 3/2}, label}]`, `tempo.level`, `tempo.margin`, `tempo.octaveLevel ∈ {Green, Amber, Red}`, `tempo.reason` (top two contributing features in plain words, e.g. "snare backbeat supports 70; hats support 140"). Candidates within the ambiguity margin are never hidden (UX_BEHAVIOR.md §5.1).

## Failure modes
| Failure | Mitigation |
|---|---|
| Non-backbeat genres (Latin, Afro, some house) make `bb` misleading | feature mix; margin; ask on Amber/Red |
| Halftime genres labelled against producer convention | labelling only; per-project choice |
| Both levels decode well (DnB 87/174) | Amber by design |
| Very slow ballads below 40 BPM | reported at 2× level; user can type a target (40 BPM floor is a known limit) |
| Weights over-fit the corpus | held-out evaluation, logged weights |

## Open Questions
1. Should N_c be expressed in beats of the chosen level (current WARP_FUNCTION definition) or in true-tactus beats, so that a ×2 relabel does not stiffen Natural mode? Decide after listening test S-3.
2. Default label for halftime genres: backbeat convention (slower) or producer convention (faster)? UX decision.
3. Calibrated m_hi, m_lo and feature weights (needs ≥ 50 annotated Suno songs).

## Tests
| ID | Input | Assertion |
|---|---|---|
| HD-1 | S01 clicks at 60/90/120/174 BPM | candidate set contains the true tempo |
| HD-2 | S08 rock backbeat at c and the same pattern relabelled at 2c | `bb(c) > 0.5`, `bb(2c) ≈ 0` (±0.1) |
| HD-3 | S08 half-time (snare on 3) at 140 | `bb(70) > bb(140)`; top-2 contains 70 and 140 (A14, BENCHMARK category 7) |
| HD-4 | S08 with 16th hats at 110 | c1 = 110, Green |
| HD-5 | Consistent ×2 relabel on S03 ramp, Natural N_c = 8 | warp ratios at true beats differ ≤ 0.1 % from the 1× warp; Strict at ×2 with 60 % swing: off-beats straightened (documented behaviour) |
| HD-6 | Corridor guard: S01 at 120 with 25 % dropouts | no decoded IOI outside [0.75, 1.33]·P |
| HD-7 | Relabel round trip ×2 then ÷2 | beats identical to the original decode (bit-exact) |
| HD-8 | Corpus (U, C tiers) | octave-correct ≥ 95 %, all misses Amber/Red (A8/A14) |
