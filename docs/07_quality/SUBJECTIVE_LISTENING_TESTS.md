# Subjective Listening Tests

Status: v1.0, 2026-10-10.

## Objective
Define listening-test protocols that decide perceived quality questions objective metrics cannot: engine/preset choice, Transient-mode go/no-go, transparency of near-identity corrections, release sign-off, across drums, vocals, sustained synths, dense mixes and extreme stretch ratios.

## Why It Matters
Published objective measures are poor predictors of perceived TSM quality (Roberts & Paliwal; best PCC 0.865 even for their purpose-built measure). Phasiness, warble, transient smearing/doubling and "robotic" vocals are judged by ear. A professional user's trust is lost on the first audible artifact.

## Key Findings
- ITU-R BS.1534-3 (MUSHRA, 10/2015, in force) targets intermediate impairments: 0–100 scale, hidden reference, 3.5 kHz and 7 kHz low-pass anchors, ~10–12 s items, ~15–20 experienced listeners typically sufficient, post-screening (hidden reference < 90 on > 15 % of items ⇒ exclude). (High version / Med numbers via secondary sources)
- ITU-R BS.1116-3 (02/2015) targets small impairments → right model for "is a ≈1.0-ratio correction transparent?".
- TSM-specific problems: (1) no natural hidden reference (the correct stretched signal does not exist for real recordings); (2) low-pass anchors do not resemble TSM artifacts. Solutions below.
- Roberts & Paliwal (42,529 ratings): expert and non-expert ratings of TSM quality were equivalent; lab vs remote differences minimal → remote non-expert panels with screening are acceptable for TSM.
- Pairwise comparisons with ~24 listeners were used in a 2026 TSM study (arXiv 2609.18999) — cheap and sensitive for A/B engineering decisions.
- webMUSHRA (AudioLabs) supports MUSHRA, BS.1116 AB, Likert, ranking; licence permits free use/modification (no patent grant). ABX needs a small custom script.

## Technical Details

### 1. Protocol overview
| ID | Protocol | Question | When | Panel |
|---|---|---|---|---|
| LIS-ABX | ABX (BS.1116 spirit: blind, hidden identity) | Is corrected output distinguishable from source when ratio ≈ 1 (stable song, passthrough seams, export round-trip)? | per release for transparency claims | ≥ 5 listeners × 16 trials per clip |
| LIS-PAIR | Paired comparison (2AFC + optional "no difference") | Which of A/B has fewer artifacts / sounds more natural (same W)? | every engine/preset/parameter change | ≥ 8 listeners × ≥ 10 clips, both orders |
| LIS-MUSHRA | MUSHRA-like (BS.1534-3 structure, TSM anchors) | Absolute quality ranking of engines/modes per category | milestones (engine choice, Transient-mode ship decision, 1.0) | formal 15–20; internal minimum 8 (report wide CIs) |
| LIS-ACR | Absolute Category Rating 5-point MOS (ITU-T P.800 style) on full corrected songs | Overall acceptability to target users | beta | 20+ beta users, opt-in, offline survey |

### 2. References and anchors for TSM
| Material | Hidden reference | Low anchor | Mid anchor |
|---|---|---|---|
| Tier M (MIDI-rendered) | **native render of the same MIDI at the target grid** — a true "perfectly stretched" signal (one-shot samples are not stretched, transients pristine) | naive fixed-hop OLA (no waveform-similarity search) at same W: strong doubling/phasiness | phase vocoder without phase locking/transient handling at same W |
| Tier C/U real recordings | the **original source** (tempo differs by the correction; at typical ratios ±3 % this is barely noticeable) — listeners told to judge artifacts, not tempo | as above | as above |
| Optional | — | BS.1534 3.5 kHz LP of reference (kept only for cross-test calibration) | 7 kHz LP |

The TSM anchors are a documented deviation from BS.1534-3 and must be reported as such. Their "mid" placement is **UNVERIFIED** until a pilot shows they land in the intended score ranges.

### 3. Conditions under test (typical)
Signalsmith "Smooth" default; Signalsmith "High quality" preset; Rubber Band R3 with channels-together (reference engine, internal only, per TIME_STRETCH rec. 9); "Transient" HPSS mode (when it exists); "Resample" mode only in a separate test (pitch changes — not comparable). ≤ 8 stimuli per trial including hidden reference and anchors.

### 4. Stimuli
- Clip categories (≥ 3 clips each, 8–12 s, start on a bar line, loop-safe, no fade inside the evaluation window): **drums** (solo drum performance, Tier M + C), **vocals** (vocal-forward / a cappella), **sustained synth/pad**, **piano/acoustic** (decaying transients), **bass-heavy**, **dense mix** (EDM/rock full mix), **stereo-wide/reverberant**, **Suno-typical** (Tier U, only if legal review clears), **extreme stretch** (separate block).
- Ratios: product-typical warp maps from real drift corrections (variable W, |r−1| ≤ 3 %); fixed 0.94/1.06; edge 0.90/1.10; extreme 0.8/1.25 and 0.5/2.0 (extreme block judged separately, labelled out-of-range, never pooled with typical results).
- Critical-material selection: pilot with 2 listeners to choose items that expose differences (per BS.1534 practice), but keep at least one "typical Suno-like" item per category so results are not only worst-case.
- Level: all stimuli loudness-matched to −23 LUFS integrated (BS.1770) ± 0.5 LU after processing; true-peak ≤ −1 dBTP; fixed playback gain per session.

### 5. Procedure
1. Screening: self-reported hearing; for formal tests, BS.1534/BS.2300 pre-screening as feasible.
2. Training (≤ 10 min): demonstrate each artifact type with exaggerated examples — transient smearing/pre-echo, transient doubling ("flamming"), phasiness/"underwater", warble/vibrato artifacts on vocals, clicks at seams, stereo image collapse.
3. Sessions ≤ 30 min; breaks enforced; randomized trial order and condition-to-slider assignment per listener; blind labels; sample-accurate switching with short crossfade (webMUSHRA provides).
4. Equipment: closed or open-back studio headphones with flat response, quiet room; remote participants must pass a headphone-check (e.g. antiphase-tone test) and post-screening.
5. Optional attribute sliders after overall rating: transient sharpness, pitch stability (warble), spatial/phasiness, clicks.

### 6. Statistics and decision rules
- LIS-ABX: per listener binomial one-sided test; significance thresholds 9/10, 10/12, **12/16**, 15/20 correct (p < 0.05, computed). Claim "transparent" only if no listener is significant **and** the pooled count is not significant; report pooled proportion with 95 % CI and state that non-significance is not proof of transparency (low power).
- LIS-PAIR: sign test / Bradley–Terry over all judgements; consistency check (order-swapped repeats; listeners inconsistent on > 30 % of repeats excluded); decision: B adopted if preferred with p < 0.05 and no category significantly worse.
- LIS-MUSHRA: post-screen (exclude if hidden ref < 90 on > 15 % of items or mid anchor > 90 on > 15 %); per condition × category mean with 95 % CI (bootstrap or t); paired Wilcoxon signed-rank vs reference engine with Holm correction; optional repeated-measures ANOVA. **Non-inferiority rule (proposal):** the default engine passes if, on drums, vocals and dense-mix categories at product-typical ratios, the lower 95 % CI bound of (default − reference) ≥ −5 points. Failing ⇒ escalate: tune preset, Transient mode, or licensing Rubber Band (orchestrator/legal).
- Transient mode ships only if LIS-PAIR shows significant preference on drum-heavy clips without significant loss on vocals/pads (TIME_STRETCH rec. 4).
- Stable-song acceptance (brief §44): LIS-ABX on category-1 items must not show audible difference.

### 7. Records
Store per session: listener ID (pseudonymous), equipment, stimuli hashes, engine versions, W files, raw ratings, screening results. Results feed `ACCEPTANCE_CRITERIA.md` and release notes. All local; no cloud services required (privacy rule §37).

## Alternatives Considered
- Strict BS.1534-3 with low-pass anchors only: rejected as sole design — anchors unrelated to TSM artifacts, and no real hidden reference.
- ACR/MOS for engine decisions: rejected — low sensitivity for small differences; kept for beta acceptability.
- Crowdsourced online panels: acceptable for TSM per Roberts & Paliwal, but music licensing of stimuli (Tier P/U) forbids distributing them; only Tier S/M/C stimuli may go to external listeners.

## Evidence / Sources
- https://www.itu.int/rec/R-REC-BS.1534 (BS.1534-3, 10/2015) · https://en.wikipedia.org/wiki/MUSHRA · https://www.testdevlab.com/blog/how-we-conduct-listening-tests-mushra-methodology-from-itu-r-recommendations
- https://www.itu.int/rec/R-REC-BS.1116-3-201502-I
- https://arxiv.org/abs/2006.00848 (expert ≈ non-expert, remote ≈ lab for TSM)
- https://arxiv.org/abs/2609.18999 (pairwise TSM evaluation, 24 participants)
- https://github.com/audiolabs/webMUSHRA (licence: free use/modification, no patent grant)
- https://www.itu.int/rec/R-REC-BS.1770-5-202311-I (loudness matching, true peak)

## Recommendation
Use LIS-PAIR continuously during engine development, LIS-ABX for every transparency claim, and one LIS-MUSHRA round at engine-selection milestone using Tier M native-tempo references plus Tier C clips, with Rubber Band R3 as reference condition.

## Risks
- Small internal panels → wide CIs; mitigate with paired designs and more clips per listener rather than more listeners.
- Listener fatigue/learning effects → randomization, short sessions, repeats.
- Original-as-reference on real material may leak tempo cues at larger ratios → restrict real-material MUSHRA to |r−1| ≤ 6 %.

## Open Questions
- Who forms the panel (owner + collaborators?) and whether a paid formal panel is budgeted for 1.0.
- Legal clearance to use Tier U clips even in internal listening.
- Whether Rubber Band R3 may be used as an internal reference without a commercial licence (GPL internal-use question — legal review).

## Implementation Implications
- CLI batch mode to render all conditions for a clip list from one W file; stimulus packager (trim, loudness-match, hash) in `tools/listening/`.
- webMUSHRA config generator + small ABX runner script (local HTML/JS or Python).
