# Benchmark Corpus

Status: v1.0, 2026-10-10. Licence statements are research findings, **not legal advice**; every Tier P/U use is flagged **for legal review**.

## Objective
A reusable, versioned, licence-tracked corpus covering every category of brief §25, with per-item source, expected BPM, beat annotation method, expected behaviour, acceptable error and known failure modes.

## Why It Matters
Thresholds can only be frozen against a corpus that represents the target material (Suno/AI-generated songs) and the known failure modes (octave errors, drumless sections, drift, loose grooves). Licensing determines what can be shared, put in CI, or only used locally.

## Key Findings
- Commercially safe sources exist for engineering gates: own synthetic signals, Groove MIDI / E-GMD (CC BY 4.0), CC BY / CC0 tracks from FMA and ccMixter (per-track licences).
- The standard beat datasets are NC, unlicensed or research-only, and most were used to train Beat This (contamination); GTZAN is its held-out test set.
- The product-relevant set is user-supplied Suno material; Suno ToS restricts use for "develop or improve competing products" and ML training → evaluation-only, local-only, legal review.

## Technical Details

### 1. Tiers

| Tier | Source | Licence | Storage / CI | Ground-truth quality |
|---|---|---|---|---|
| S | `tools/gen` synthetic signals (DSP_TEST_STRATEGY §3) | own | regenerated; CI | exact |
| M | Groove MIDI Dataset (1,150 MIDI, CC BY 4.0) and E-GMD audio (CC BY 4.0); own MIDI songs (drums + bass + pads + lead) rendered with own synthesis or CC0 samples | CC BY 4.0 (attribution) / own | render scripts in repo; audio cached; CI | exact (MIDI times), plus native-tempo reference renders |
| C | FMA (`fma_full`, filter per-track licence = CC BY / CC0), ccMixter (CC BY uploads), Freesound CC0 one-shots | per track; **NC and ND excluded** | LFS/release assets with manifest; CI | manual annotation (method §3) |
| P | GTZAN (beats via Beat This annotations v1.1, MIT), Ballroom, Hainsworth, SMC, Harmonix (annotations MIT; audio via YouTube — not acquired without legal review), RWC (research pledge), Isophonics Beatles (own CDs), GiantSteps Tempo (annotations CC BY-SA, Beatport audio) | NC / none / research-only | developer machine only; never committed; no CI | published annotations (known errors; Sturm GTZAN faults excluded) |
| U | User-supplied Suno tracks: paid-tier (Pro/Premier) outputs, written permission naming the use (QA evaluation of BeatFix), local-only | owner's rights per Suno ToS; ToS item 10 restricts "develop or improve competing products" and any ML training | encrypted local folder; never committed; no CI; **never used for training/fine-tuning** | manual annotation (§3) |

Minimum viable sizes (before freezing thresholds): S full catalogue; M ≥ 40 items; C ≥ 30 items across categories; U ≥ 50 songs (matches BEAT_TRACKING rec. 8); P optional.

### 2. Category matrix (brief §25)
Expected BPM for S/M items is exact by construction; for C/U items it is the annotated value (regression over corrected beats) with the ambiguity set recorded. "Acceptable error" values are PROVISIONAL targets keyed to METRICS IDs.

| # | Category | Primary source(s) | Expected BPM | Beat annotation method | Expected behaviour | Acceptable error (PROVISIONAL) | Known failure modes |
|---|---|---|---|---|---|---|---|
| 1 | Stable 110 BPM | S01/S08 @110; M song @110; U items near 110 | 110.000 | generator / MIDI / manual | BPM ≈ 110, minimal warp, waveform "mostly grey", output effectively unchanged | A11 ≤ 0.01 (S) / ≤ 0.05 (real); max\|r−1\| ≤ 0.5 %; O18 transparent (ABX) | tracker jitter creating needless micro-warps → require smoothing/dead-band |
| 2 | Stable 120 BPM | S01/S08 @120; C EDM/pop | 120.000 | as above | as #1 | as #1 | 60/240 octave confusion on hat-driven material |
| 3 | Fractional BPM | S02 (109.73, 120.5, 127.999); M rendered at 97.3 | exact fractional | generator / MIDI | fractional estimate shown; user may round target; no false "drift" | A11 ≤ 0.01 (S); O3 ≤ 5 ms end-of-song | estimator snapping to integers; drift misreport from rounding |
| 4 | Slight drift | S03 108→113; M with synthetic wobble ±1.5 %; U Suno with audible drift | curve known (S/M), annotated (U) | generator / known V / manual | drift detected, gradual correction to grid, no seams | O2 median ≤ 3 ms (S) / ≤ 10 ms (real); O15 zero new clicks | over-smoothing leaves residual drift; under-smoothing tracks jitter |
| 5 | Severe drift | S03 100→120; M wobble ±5 % | known | generator / V | corrected within clamp; warning when r outside comfortable range | O2 as #4; r within clamp, flagged if clamped | audible TSM artifacts at larger ratios; listening decides |
| 6 | Tempo ramp (intentional accel/rit) | S03 90→80; C classical/acoustic rit | curve | generator / manual | detected as ramp; user choice: keep or flatten; default per UX spec | A12 class correct; O2 if flattened | flattening musical rubato sounds unnatural (expected behaviour = ask) |
| 7 | Half-time feel | S08 half-time (snare on 3); C trap/hip-hop | e.g. 140 with half-time 70 feel | generator / manual with metric-level note | candidate list shows 70/140; ambiguity surfaced; target chosen by user or rule | A14 correct level in top-2 candidates 100 % (S) | picks half tempo → grid every other beat |
| 8 | Double-time feel | S08 double-time hats; C DnB/footwork 170/85 | 85/170 | as #7 | as #7 | as #7 | double-tempo pick → anchors on off-beats |
| 9 | Weak kick | S08 kick −18 dB under pad; C lo-fi | exact / annotated | generator / manual | beats still tracked via snare/harmony; confidence lower | A5 CMLt ≥ 0.9 (S) | anchors drift to hats/snare phase |
| 10 | Strong kick | S08 four-on-floor; C techno | exact / annotated | generator / manual | very high confidence, tight anchors | A10 median ≤ 2 ms (S) | none expected; sanity baseline |
| 11 | Heavy snare | S08 backbeat with loud snare, quiet kick | exact | generator | beat level not confused with snare-only (2/4) half tempo | A14 correct | half-tempo error locking on snare |
| 12 | No drums in intro | M song: 8-bar pad/vocal intro then drums; U items | known | MIDI / manual | intro anchored by harmonic/vocal onsets or by extrapolating grid backward; low-confidence region flagged | intro O2 ≤ 25 ms or region flagged as "uncorrected/extrapolated" | spurious anchors in intro; phase jump when drums enter |
| 13 | Drumless outro | M song with 16-bar drumless outro; U | known | MIDI / manual | grid extrapolated; no wild ratios; flagged | r within clamp; no anchor density collapse | tracker hallucinating beats in reverb tail |
| 14 | Vocal-heavy | C ccMixter a cappella + sparse beat; U ballads | annotated | manual | pitch preserved, no warble | O7 ≤ 5 cents median; listening (vocals category) | PV phasiness/"robotic" vocals; formant artifacts |
| 15 | Dense EDM | C CC BY EDM; U EDM | annotated | manual | tight alignment, transients intact, stereo intact | O2, O8–O10, O13 | sidechain pumping misread as beats (OK if periodic); smeared transients |
| 16 | Ambient | C CC BY ambient; S10 pads | often undefined | manual (may be "no reliable beat") | low confidence → recommend no correction / user decision | confidence ≤ low threshold; no forced warp | confident-but-wrong tracking (SMC Blind Spot pattern) |
| 17 | Acoustic | C CC BY acoustic guitar/piano, live drums | annotated | manual | natural micro-timing handled by smoothing; drift corrected | O2 real thresholds; listening | over-quantization destroys feel |
| 18 | Syncopated rhythms | S08 syncopated; C funk/reggaeton | exact / annotated | generator / manual | anchors on beats, not syncopated hits | A9 FP ≤ 2 % (S) | off-beat phase lock |
| 19 | Unusual meters (if supported) | M 3/4, 6/8, 7/8 songs; Ballroom waltz (P) | known | MIDI / published | beats correct; downbeats per meter or flagged unsupported | A7 downbeat F ≥ 0.9 (M) or explicit unsupported flag | 4/4 prior forcing wrong bar phase |
| 20 | Abrupt tempo changes | S04; M song with tempo change at section | known | generator / MIDI | sections detected; per-section handling or single grid choice per UX | change located ± 1 beat; no ratio spike beyond clamp | smoothing blurs step into a ramp |
| 21 | Intentionally loose groove | Groove MIDI human performances (swing, laid-back); C live funk | MIDI times; "intended" grid = quantized MIDI | MIDI | grid alignment of beats only; micro-timing within beat preserved (between anchors) | O2 on beats; within-beat offsets unchanged (\|Δ\| ≤ 2 ms Path K) | over-correction (per-onset anchoring) kills groove |
| 22 | Suno-typical artifacts (extra) | U; S16 | annotated | manual | robust to codec smear, vocal-bleed hats, fade-ins | U-set CMLt and O2 tracked over releases | MP3 offset; generator "breath" sections with drift |
| 23 | Long song (extra) | S01 60 min; M 12-min extended mix | exact | generator / MIDI | completes within perf budget; no cumulative drift | O3 ≤ 5 ms at 60 min | float time accumulation, memory blow-up |
| 24 | Extreme ratio (extra) | S01/S08 forced to 0.5/2.0 | exact | generator | renders without crash; warning shown | O16, O17 hard; quality listening only | phasing/smearing (expected), NaN on edge frames |

### 3. Annotation protocol (Tier C / U, and verification of P)
1. Seed: production analyzer beats + downbeats (Beat This + refinement) and DSP fallback beats.
2. Correction: annotator A corrects beats/downbeats in Sonic Visualiser (GPL; used as a tool, not linked) on a spectrogram + onset view at 1:1 zoom; marks regions "no reliable beat", "tempo change", "meter change".
3. Second annotator B (or the same person ≥ 1 week later, blind to A's file) repeats on a 20 % sample.
4. Agreement: F(±20 ms) ≥ 0.95 and median offset ≤ 5 ms; disagreements adjudicated; items failing agreement marked "ambiguous" and excluded from precision metrics (kept for robustness).
5. Tap-along is not used as the primary method (motor latency/jitter of tens of ms).
6. Each annotation file records annotator, tool version, date, method, ambiguity flags.

### 4. Manifest schema (one JSON per item, `corpus/manifest/<id>.json`)
```json
{
  "id": "C-EDM-007",
  "tier": "C",
  "category": [15],
  "source": {"origin": "FMA", "url": "...", "artist": "...", "title": "...",
             "licence": "CC BY 4.0", "licence_url": "...", "attribution": "..."},
  "audio": {"file": "c/edm007.flac", "sha256": "...", "sr": 44100, "channels": 2,
            "duration_s": 213.4, "codec_history": "mp3->flac"},
  "truth": {"beats": "c/edm007.beats.txt", "downbeats": "c/edm007.downbeats.txt",
            "method": "seeded+manual(2 annotators)", "agreement_f20": 0.97,
            "tempo": {"type": "stable", "bpm": 126.02, "ambiguity": [63.01, 252.04]},
            "meter": "4/4", "regions": [{"start": 0, "end": 15.2, "flag": "no_drums"}]},
  "expected": {"behaviour": ["minimal_warp"], "target_bpm": 126,
               "acceptable": {"A11_bpm": 0.05, "O2_median_ms": 10, "O2_p95_ms": 25}},
  "known_failure_modes": ["sidechain pumping", "half-time breakdown 2:10-2:40"],
  "allowed_use": ["ci", "local"],
  "notes": ""
}
```
`allowed_use` is enforced by the bench runner: Tier P/U items carry `["local"]` and are refused when `CI=true`.

### 5. Splits and contamination rules
- Tune parameters (smoothing λ, clamps, thresholds) on a **dev split** (≈ 60 %) of C/U; report on a **test split** (≈ 40 %) touched only at milestones.
- Beat This must not be evaluated on its training datasets as evidence of generalization; GTZAN (held-out in its paper) and Tiers C/U are the clean tests.
- No item of any tier is used for ML training in MVP (MVP trains nothing). If training is ever added: Tier U and Suno-derived audio are excluded by ToS.

### 6. Versioning
Corpus version = semver; results files reference corpus version + manifest hashes; adding items is minor, changing annotations is major (invalidates comparisons).

## Alternatives Considered
- Public datasets as primary corpus: rejected (licence, contamination, genre mismatch).
- Generating "Suno-like" material with Suno ourselves for the corpus: same ToS issues as Tier U plus generation-account terms; deferred to legal review.
- Crowdsourced annotations: unnecessary at this scale.

## Evidence / Sources
QA_RESEARCH.md F5–F19 (datasets, licences, Suno ToS), BEAT_TRACKING_RESEARCH rec. 8.

## Recommendation
Build S and M immediately (zero legal risk, exact truth); curate 30 Tier C tracks across categories 2, 9, 12–18, 21; collect ≥ 50 Tier U tracks only after legal review of Suno ToS item 10; use Tier P selectively (GTZAN for tracker regression vs paper numbers).

## Risks
- Tier U blocked by legal review → product-relevant accuracy must be estimated from C + M with synthetic drift and Suno-like processing (S16), a weaker proxy.
- CC BY tracks with mis-declared licences on FMA → keep provenance (URL, retrieval date, licence snapshot).

## Open Questions
- Legal: Tier P and Tier U usage (see QA_RESEARCH Open Questions 1–2).
- Whether unusual meters are in MVP scope (category 19 expected behaviour).
- Default behaviour for intentional ramps (#6) — UX decision.

## Implementation Implications
- `tools/eval/bench.py` reads manifests, enforces `allowed_use`, emits per-item and per-category metrics with bootstrap CIs.
- Attribution file `corpus/ATTRIBUTION.md` generated from manifests (CC BY obligation for redistributed items).
