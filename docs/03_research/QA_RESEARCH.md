# QA / Testing / Benchmarking Research (Agent G)

Date: 2026-10-08. Inputs: full brief §§16, 23, 24, 25, 44; `_baseline_research_log.md`; sibling research (TIME_STRETCH, BEAT_TRACKING, ARCHITECTURE) read for consistency. Deliverables derived from this research live in `docs/07_quality/` (QA_STRATEGY, DSP_TEST_STRATEGY, BENCHMARK_CORPUS, PERFORMANCE_BENCHMARKS, SUBJECTIVE_LISTENING_TESTS, METRICS). `ACCEPTANCE_CRITERIA.md` is left to the orchestrator.

## Objective
Establish, from primary sources, how BeatFix's analysis and corrected output can be measured: which beat/tempo metrics are standard and how they are computed; which public datasets exist and which may legally be used for internal benchmarking; how to design synthetic ground-truth signals; which objective quality measures exist for time-scale-modified (TSM) audio and where they fail; how to run subjective listening tests; and what performance targets are realistic.

## Why It Matters
BeatFix's core claim ("beats land on the grid, sound stays intact") is only credible if it is measured. Two failure sources must be separable: detector (wrong anchors) and engine (bad render). Off-the-shelf MIR metrics are too lax for warp anchors; off-the-shelf audio-quality metrics are invalid on time-scaled audio. Dataset licensing directly constrains what can sit in the repo/CI.

## Key Findings

### Verified facts

| # | Claim | Evidence | Conf. | Implication | Recommendation |
|---|---|---|---|---|---|
| F1 | mir_eval 0.8.2 (2025-02-25), MIT, provides f_measure (0.07 s), cemgil (σ 0.04), goto (0.35/0.2/0.2), p_score (0.2), continuity (17.5 %/17.5 %, variations: off-beat, double, half-odd, half-even), information_gain (41 bins) | PyPI JSON; readthedocs beat API; beat.py source | High | Standard, licence-clean evaluation tool (dev-only) | Use mir_eval for all standard beat metrics |
| F2 | `mir_eval.beat.evaluate()` trims beats < 5 s by default; IG normalized to [0,1] ("beat evaluation toolbox does not normalize") | beat.py lines 641–647; docs | High | Intro beats (which BeatFix warps) excluded by default; IG not comparable to bit-valued papers | Report trimmed and untrimmed; state IG convention |
| F3 | `mir_eval.tempo.detection(ref_tempi[2], weight, est_tempi[2], tol=0.08)` → p_score, one_correct, both_correct | tempo API docs | High | No Acc1/Acc2 in mir_eval | Implement Acc1/Acc2 ourselves (trivial) |
| F4 | Acc1 = within 4 %; Acc2 = within 4 % also accepting ×2, ×3, ×½, ×⅓ | ISMIR 2004 contest page; Gouyon et al. 2006 | High | ±4 % = ±4.4 BPM at 110 → too coarse | Add fine tempo error in BPM (METRICS A11) |
| F5 | Beat This!: 22.05 kHz mono, hop 441 → 50 fps; GTZAN (993 pieces) test-only; final model GTZAN beat F1 89.1, CMLt 79.8, AMLt 89.8, downbeat F1 78.3; CMLt below Hung et al. (81.2) because it "introduces non-periodic beats" on complex pieces; trained on Ballroom, Beatles, Hainsworth, Harmonix, RWC, SMC, Simac, HJDB, Groove MIDI, GuitarSet, Candombe, etc. (8-fold CV: SMC F1 62.7, Ballroom 97.5, Hainsworth 91.9, Harmonix 95.8) | arXiv 2407.21658, Tables 1–2 (text extracted) | High | (a) 20 ms frame quantization → need sub-frame refinement (BEAT_TRACKING rec. 3); (b) evaluating Beat This on its training datasets is contaminated — only GTZAN and our own Suno/CC corpus are clean tests; (c) spurious non-periodic beats = spurious warp anchors | Gate on CMLt + spurious-anchor rate, not F1 alone; use GTZAN + own corpus as held-out |
| F6 | Beat This annotations repo (16 datasets incl. corrected GTZAN/Harmonix/RWC), MIT, no audio | github CPJKU/beat_this_annotations | High | Best available cleaned annotations | Use v1.1 annotations for Tier P |
| F7 | Harmonix Set: 912 pop tracks, beats/downbeats/segments, annotation repo MIT; audio not distributed (YouTube URLs + DTW alignment script; mel spectrograms on Dropbox) | github urinieto/harmonixset | High | Annotations free; acquiring audio via YouTube is a ToS/copyright question | Do not download YouTube audio without legal review; mel-spectrograms only usable for spectrogram-input models |
| F8 | Ballroom (698 excerpts, ISMIR 2004 MTG data) and Hainsworth (222 × ~1 min): mirdata loader docstrings state CC BY-NC-SA 4.0; mirdata quick-reference table says CC0 | mirdata ballroom.py / hainsworth.py vs quick_reference | Med (sources disagree) | Disagreement: docstring is more specific and conservative; audio is from commercial recordings regardless | Treat as **NC, research-only, local-only, legal review** |
| F9 | GTZAN: 1000 × 30 s, no licence, audio of commercial recordings; known faults: repetitions, mislabelings, distortions (Sturm 2013); Beat This authors list further beat-annotation errors | arXiv 1306.1461; Beat This §5 | High | Usable as the community's held-out beat test, but legally grey and partly faulty | Local-only; exclude Sturm's faulty/duplicate items from aggregate scores |
| F10 | SMC: 217 × 40 s "difficult" excerpts; available to researchers on request; licence not published | csd.uoc.gr datasets page; SMC Blind Spot arXiv 2605.12287 | Med | Stress test for expressive tempo; Beat This trained on it | Local-only, request access, legal review |
| F11 | RWC Music Database: AIST copyright; research-use-only pledge; online download since Feb 2026 (Zenodo); "may not be sold, leased, published or distributed"; AIST annotations on GitHub rwc-music/rwc-annotations | staff.aist.go.jp/m.goto/RWC-MDB | High | "Research purposes" by a company building a commercial product is unclear | Legal review before use; never in repo |
| F12 | Isophonics Beatles annotations (179/180 songs): audio not available (commercial CDs) | mirdata | High | Requires owning the CDs | Optional, local-only |
| F13 | GiantSteps Tempo: 664 × 2 min Beatport previews, annotations CC BY-SA 4.0, audio DIY from Beatport | GiantSteps repo; mirdata | High | Good EDM tempo test (octave errors); audio rights belong to labels | Local-only for audio; annotations ok |
| F14 | Groove MIDI Dataset (1,150 MIDI, 13.6 h, human tempo-aligned drumming) and E-GMD (444 h audio, 43 kits): **CC BY 4.0** (Google) | magenta.tensorflow.org/datasets/groove, e-gmd; Zenodo | High | Commercially usable with attribution; MIDI can be re-rendered at *any* tempo → true reference for TSM | Core of Tier M corpus; generates native-tempo references |
| F15 | GuitarSet (360, beats/tempo): mirdata lists MIT | mirdata quick reference | Med (single source) | Possibly usable | Verify licence on Zenodo before use |
| F16 | FMA: code MIT, metadata CC BY 4.0, audio under per-track artist licences ("We do not hold the copyright"); full 106,574 tracks / 879 GiB | github mdeff/fma | High | Can be filtered to CC BY / CC0 tracks | Source for Tier C (filter licence field = CC BY or CC0; record per-track) |
| F17 | MTG-Jamendo: 55,525 tracks, per-track CC audio licences, metadata CC BY-NC-SA, dataset "solely for non-commercial research and academic use" | github MTG/mtg-jamendo-dataset | High | Dataset packaging NC even if some tracks are CC BY | Prefer obtaining individual CC BY tracks from original sources, not the dataset bundle |
| F18 | ccMixter: per-upload CC licences; ~5000 plain CC BY tracks plus BY-NC/BY-NC-SA | Wikipedia ccMixter; CC wiki | Med | Stems + a cappellas useful for vocal-heavy items | Tier C source, CC BY only |
| F19 | Suno ToS (rev. 2026-08-10, effective 2026-09-03): Pro/Premier — Suno assigns its rights in Output to user; free tier — "lawful, personal and non-commercial purposes"; User Conduct item 10 prohibits using "the Services (and any Output or Voice Model) to create, develop or improve any competing products or services or to power, enable or train other artificial intelligence and machine learning models, tools or technologies" | suno.com/terms (fetched 2026-10-08) | High (text) / Low (interpretation) | (a) Never train/fine-tune any model on Suno outputs; (b) whether BeatFix is a "competing product" and whether using a user's Suno track for testing counts as "develop or improve" → **legal review**; (c) free-tier tracks are personal-use only | Tier U: paid-tier tracks only, written permission from the account holder, evaluation-only, local-only, no ML training — pending legal review |
| F20 | Roberts & Paliwal TSM dataset: 88 train + 20 test sources, 6+3 TSM methods, 42,529 ratings / 633 sessions; experts ≈ non-experts; published objective measures are poor indicators of subjective quality | arXiv 2006.00848; IEEE DataPort (~4.4 GB, licence not stated) | High | Non-expert remote panels are acceptable for TSM quality; objective measures cannot gate | Listening tests decide quality; objective metrics = regression detectors |
| F21 | Their double-ended measure (PEAQ basic/advanced features + 9 TSM features, alignment by interpolating reference magnitude spectrum): PCC 0.865, RMSE 0.487; single-ended CNN/BGRU: PCC 0.771/0.794 | arXiv 2006.06153, 2009.02940 | High | Best available, still ~25 % unexplained variance | Optional research tool only |
| F22 | github zygurt/TSM (MATLAB + PyTorch; measures + TSM implementations): **no licence** (GitHub API `license: null`), last push 2025-07-24 | GitHub API | High | All rights reserved by default | Do not copy/ship; internal use only after legal review |
| F23 | PEAQ cannot be applied directly to TSM output (alignment loss); ViSQOL has patch alignment but is designed for codec/network degradations | arXiv 2006.06153; ViSQOL docs | High | Full-reference metrics need a truly aligned reference | Use full-reference metrics only vs MIDI native-tempo renders (METRICS O12) |
| F24 | ITU-R BS.1534-3 (10/2015, in force) MUSHRA: 0–100 scale (bad/poor/fair/good/excellent), hidden reference, low (3.5 kHz LP) and mid (7 kHz LP) anchors, ~10–12 s items, ≤ ~9–12 conditions per trial, 15–20 experienced listeners usually sufficient, post-screening: exclude if hidden ref < 90 on > 15 % of items (or mid anchor > 90 on > 15 %) | ITU rec page; Wikipedia; TestDevLab summary | High (version) / Med (numbers via secondary sources) | Standard is for intermediate impairments; LP anchors do not resemble TSM artifacts | Use MUSHRA structure with TSM-specific anchors (documented deviation) |
| F25 | ITU-R BS.1116-3 (02/2015) for small impairments (double-blind triple-stimulus hidden reference) | ITU rec page | High | Appropriate for "is the ratio≈1 output transparent?" | Use ABX / BS.1116-style for transparency claims |
| F26 | webMUSHRA (AudioLabs/Fraunhofer): MUSHRA + BS.1116 AB + Likert + ranking; custom licence permitting use/modification/redistribution without fee, no patent grant | github audiolabs/webMUSHRA LICENSE.txt | High | Free to use internally | Use as listening-test runner (not shipped) |
| F27 | Variable-rate HPS-TSM paper (2026): pairwise ratings, 24 participants, 1,114 judgements | arXiv 2609.18999 | High | Pairwise preference is a current, cheap protocol for TSM variants | Use paired-comparison for engine A/B decisions |
| F28 | ITU-R BS.1770-5 (2023-11) Annex 2 true-peak: 4× oversampling at 48 kHz | ITU | High | Defines O14 | Implement/verify true-peak per Annex 2 |
| F29 | Rubber Band R3 "uses much more CPU than R2"; `OptionWindowShort` far lower CPU; no published realtime-factor numbers found. Signalsmith: "quite CPU efficient", ~50 ms buffering; no published numbers | Rubber Band CHANGELOG/R3 docs; HISE forum | Med | Render-speed targets cannot be sourced | Performance targets are PROVISIONAL, measured in Phase-0 spike |
| F30 | Beat This Rust port: 4.5-min MP3 ≈ 4.6 s on Apple M4 (≈ 0.017× RT) | baseline log (port young; single source) | Low | Mid-range x86 likely several × slower | Analysis target ≤ 0.15× RT provisional |

### Derived facts (computed)
- Memory, interleaved float32 stereo: 10 min @ 48 kHz = 600·48000·2·4 B = **230.4 MB (219.7 MiB)**; @ 44.1 kHz 211.7 MB; @ 96 kHz 460.8 MB; 20 min @ 96 kHz 921.6 MB. float64 doubles these.
- Tempo drift budget: D ≈ T·|ΔB|/B. At 110 BPM over 240 s: ΔB 0.01 → 22 ms; 0.05 → 109 ms; 0.1 → 218 ms.
- ABX one-sided binomial (p < 0.05) minimum correct: 9/10, 10/12, 12/16, 15/20, 18/25.
- 108→113 BPM to a 110 grid → ratios 0.982–1.027 (≈ −1.8 % … +2.7 %). Typical Suno correction lies in ±3 %; extreme ratios (0.5, 2.0) are robustness tests, not quality targets.
- Beat This frame quantization ±10 ms uniform → RMS 5.8 ms; at 110 BPM a 20 ms anchor error = 3.7 % local ratio error.

## Technical Details
See `07_quality/METRICS.md` (all formulas), `DSP_TEST_STRATEGY.md` (signal generators, golden tests, determinism), `BENCHMARK_CORPUS.md` (corpus tiers and items), `PERFORMANCE_BENCHMARKS.md`, `SUBJECTIVE_LISTENING_TESTS.md`.

Key methodological ideas this research adds:
1. **Two-path output verification**: Path K (known ground truth through W — isolates engine/time-map) and Path R (re-detection vs target grid — end-to-end). Attribution rule: K pass + R fail ⇒ detector; K fail ⇒ engine.
2. **Native-tempo reference from MIDI** (Groove MIDI CC BY 4.0 + own MIDI): render the same performance at the target tempo/grid → the only legitimately time-aligned reference for full-reference metrics and a true MUSHRA hidden reference for drum material.
3. **Synthetic-drift corpus**: apply a known, smooth inverse warp to grid-locked CC/MIDI material → realistic music with exact ground-truth beat positions and a known ideal correction (also recommended by BEAT_TRACKING_RESEARCH).
4. **Licence tiers** for test data: S (synthetic) / M (MIDI-rendered) / C (CC BY / CC0) may live in the repo/CI; P (public research datasets) and U (user-supplied Suno) are local-only, never committed, never used for ML training.

## Alternatives Considered
- Using public MIR datasets as the primary benchmark: rejected — licensing (NC/no licence/commercial audio), training-set contamination for Beat This, and genre mismatch with Suno material. Kept as secondary, local-only.
- Using an objective TSM metric as release gate: rejected (F20–F23).
- Full BS.1534-compliant MUSHRA for every engine change: too expensive; use pairwise/ABX for development, MUSHRA-style for milestone decisions.
- Tap-along annotation for real music: rejected (motor latency/jitter tens of ms); use detector-seeded manual correction in a spectrogram editor with two annotators.

## Evidence / Sources
- mir_eval: https://mir-eval.readthedocs.io/latest/api/beat.html · https://mir-eval.readthedocs.io/latest/api/tempo.html · https://github.com/mir-evaluation/mir_eval · https://pypi.org/project/mir_eval/
- Beat This!: https://arxiv.org/abs/2407.21658 · https://github.com/CPJKU/beat_this_annotations
- Datasets: https://github.com/urinieto/harmonixset · https://mirdata.readthedocs.io/en/latest/source/quick_reference.html · https://mirdata.readthedocs.io/en/latest/_modules/mirdata/datasets/ballroom.html · .../hainsworth.html · https://arxiv.org/abs/1306.1461 (Sturm GTZAN) · https://www.csd.uoc.gr/~hannover/Datasets.html (SMC) · https://arxiv.org/abs/2605.12287 (SMC Blind Spot) · https://staff.aist.go.jp/m.goto/RWC-MDB/ · https://github.com/GiantSteps/giantsteps-tempo-dataset · https://magenta.tensorflow.org/datasets/groove · https://magenta.tensorflow.org/datasets/e-gmd · https://github.com/mdeff/fma · https://github.com/MTG/mtg-jamendo-dataset · https://en.wikipedia.org/wiki/CcMixter
- Tempo metrics: https://mtg.upf.edu/ismir2004/contest/tempoContest/node7.html
- Suno: https://suno.com/terms (rev. 2026-08-10)
- TSM quality: https://arxiv.org/abs/2006.00848 · https://arxiv.org/abs/2006.06153 · https://arxiv.org/abs/2009.02940 · https://github.com/zygurt/TSM · https://ieee-dataport.org/open-access/time-scale-modification-dataset-subjective-quality-labels · https://arxiv.org/abs/2609.18999 · https://arxiv.org/abs/2602.16421 (SELEBI, percussion smearing)
- Listening standards: https://www.itu.int/rec/R-REC-BS.1534 · https://www.itu.int/rec/R-REC-BS.1116-3-201502-I · https://en.wikipedia.org/wiki/MUSHRA · https://www.testdevlab.com/blog/how-we-conduct-listening-tests-mushra-methodology-from-itu-r-recommendations · https://github.com/audiolabs/webMUSHRA
- Loudness/true peak: https://www.itu.int/rec/R-REC-BS.1770-5-202311-I
- Tools licences (GitHub API 2026-10-08): ViSQOL Apache-2.0, librosa ISC, pyloudnorm MIT, beat_this MIT, mir_eval MIT
- Pitch: Context7 `/websites/librosa_doc` (pyin defaults: resolution 0.1, fmin/fmax guidance)

## Recommendation
1. Adopt the METRICS.md set; gate analysis on CMLt + spurious/missing-anchor rate + anchor precision + fine tempo error; gate rendering on engine time-map fidelity (Path K), end-to-end alignment (Path R), stereo invariants, signal-hygiene checks (NaN/clip/click/silence/length/export). (High)
2. Build the corpus synthetic-first: generators in repo (Tier S), Groove-MIDI-based renders with native-tempo references (Tier M), ≥ 30 CC BY/CC0 real tracks manually annotated (Tier C), plus ≥ 50 user-permitted Suno tracks (Tier U, local-only, as BEAT_TRACKING recommends) — the Suno set is the product-relevant test. (High)
3. Public research datasets only local-only after legal review; GTZAN (minus Sturm-faulty items) as clean held-out test for Beat This. (Med)
4. Golden tests are tolerance-based with a three-level determinism policy (bit-exact same build/platform/threads; numeric tolerance across CPUs/threads; metric tolerance across algorithm versions). (High)
5. Quality decisions by listening: pairwise/ABX during development, MUSHRA-style with TSM-specific anchors (and native-MIDI hidden reference where available) at milestones; Rubber Band R3 as quality reference condition (per TIME_STRETCH rec. 9). (High)
6. Performance targets provisional until Phase-0 measurement on a named reference laptop; memory budget computed, not guessed. (High)

## Risks
- Legal: Tier P/U usage could be found non-permissible for a commercial developer → corpus would shrink to S/M/C (still sufficient for engineering gates; weaker for real-world tracker accuracy).
- Annotation noise on real music limits how tight real-music thresholds can be.
- Listening panel availability (solo developer): small panels → wide CIs; mitigated by pairwise designs and repeated items.
- Provisional thresholds may be unattainable with Signalsmith at some ratios; must be measured not assumed.

## Open Questions (for orchestrator / legal)
1. Legal review: internal benchmarking of NC/unlicensed research datasets (Ballroom, Hainsworth, GTZAN, SMC, RWC) by a commercial developer.
2. Legal review: Suno ToS item 10 ("develop or improve any competing products") vs testing BeatFix with users' Suno tracks; free-tier tracks.
3. Licence of IEEE DataPort TSM dataset and zygurt/TSM code for internal research.
4. Reference laptop model for performance targets (proposal in PERFORMANCE_BENCHMARKS.md).
5. Which independent verification detector (librosa ISC vs madmom models CC BY-NC-SA) for Path R.
6. Tail/length policy (affects O17) and whether "Passthrough" at r≈1 is bit-exact (affects O18).

## Implementation Implications
- `tools/eval/` dev-only Python package + `tools/gen/` deterministic signal generators (seeded); C++ core tests in Catch2 (per ARCHITECTURE rec.) with optional RapidCheck property tests.
- Engine/render API must serialize the exact W (or W⁻¹) used per render for Path K.
- Corpus manifest (JSON) with licence, tier, hashes; CI only touches tiers S/M/C.
- Single-pass reference render path (ARCHITECTURE rec. 5) doubles as the null-test oracle for chunk-parallel rendering.
