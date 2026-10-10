# Risk Register

Likelihood and impact: H/M/L. Owner area: DSP, ANA (analysis), ENG (engine/arch), UI, QA, LEGAL, PROD, PM. Status: Open unless noted. The phase numbers refer to [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md).

## Technical

| ID | Risk | L | I | Area | Mitigation | Early warning / trigger |
|---|---|---|---|---|---|---|
| R-01 | Beat continuity failures (mid-song level/phase switch, missed beat) produce 2×/½ local stretches | M | H | ANA | Octave chosen before decoding; ±40 % corridor; IOI repair; hard ratio bound refuses render; ≥ 12 % "extreme" flag | Spurious/missing anchor rate (A9) above threshold on U-tier |
| R-02 | Signalsmith quality on dense AI masters is audibly worse than R3 (smearing, phasiness) | M | H | DSP | Phase 0 listening spike; Bungee alternate; Phase 1.5 Transient; option to license Rubber Band commercially | MUSHRA gap > 5 points in Phase 0/4 |
| R-03 | Signalsmith variable-chunk drive misaligns events (latency semantics) | M | H | DSP | Phase 0 impulse test < 0.5 ms; absolute sample accounting | O1 P95 > 3 ms |
| R-04 | Chunk seams audible | M | M | ENG | low-RMS cuts, overlap tuning, null test vs single-pass | null > −90 dBFS |
| R-05 | Strict mode ratio steps audible at beats | L | M | DSP | PCHIP upgrade path (WARP_FUNCTION §2.2) | listening S-tests |
| R-06 | ONNX Runtime CPU results differ across machines | H | L | ANA | persist activations + model hash; D2 tolerance; explicit re-analyse only | D2 failures in CI matrix |
| R-07 | Beat This! too slow on mid-range x86 | M | M | ANA | small model fallback; inference ≤ 0.10 × RT gate in Phase 0/3 | Phase 0 timing |
| R-08 | minimp3 gapless handling incomplete (offset ≠ 0) | M | H | ENG | Phase 0 gate; libmpg123 fallback | click-track test |
| R-09 | Peak overs after stretch clip exports | H | M | DSP | peak gate with user choice; TP measurement | gate frequency in tests |
| R-10 | Memory blow-up on long/high-SR files | M | M | ENG | memory-mapped mode; duration tiers; pre-decode estimate | PERF-LONG tests |
| R-11 | Natural default (N_c = 8) wrong for real Suno drift | M | M | PROD | listening test S-3; detents easy to change | user tests, S-3 |
| R-12 | Confidence thresholds miscalibrated (over-confident Beat This!) | M | M | QA | calibrate on S/M/C; reliability diagram (A13) | ECE above target |
| R-13 | JUCE custom UI slows delivery; accessibility gaps | M | M | UI | engine C API keeps a Tauri fallback; early NVDA spike | Phase 7 slips > 30 % |
| R-14 | Realtime glitches from paging or locks | L | H | ENG | pre-touch pages; RT detector; ASan | xrun counters |

## Product

| ID | Risk | L | I | Area | Mitigation | Early warning |
|---|---|---|---|---|---|---|
| R-20 | Suno Studio's Manual BPM / warp markers make a standalone fixer redundant | M | H | PROD | differentiate: offline, any source, confidence, groove-keeping, lossless export, DAW-ready start | user interviews (5–8), Suno release notes |
| R-21 | Producers distrust automatic correction | M | H | PROD | visible confidence, heatmap, A/B with click, never silent changes | trust questions in user tests |
| R-22 | Evidence for AI-music drift magnitudes is anecdotal | H | M | PROD | annotated drift corpus (≥ 50 songs) before freezing thresholds | corpus stats |
| R-23 | Target users lack a DAW (P1) and find tempo concepts hard | M | M | UI | one-click path, plain wording, progressive disclosure | onboarding tests |

## Legal (for legal review; not legal advice)

| ID | Risk | L | I | Mitigation |
|---|---|---|---|---|
| R-30 | JUCE licence tier/terms (JUCE 9 terms, revenue thresholds) | M | H | commercial tier, never AGPL; confirm JUCE 9 EULA |
| R-31 | Beat This! training-data caveat ("up to the user to assess") | M | H | legal review before release; DSP fallback exists |
| R-32 | Fraunhofer US 9,230,558 B2 (transient remove/reinsert) until 2029 | M | H | no Slice mode; HPSS hybrid reviewed before Phase 1.5 |
| R-33 | LGPL compliance (LAME, libmpg123) | L | M | DLLs, relinking notice, source offer |
| R-34 | AAC licensing (Via LA) if added | M | M | excluded from MVP |
| R-35 | Suno ToS (item 10) on using outputs for development or calibration | H | M | use S/M/C tiers for fitting; Suno files local-only, with permission, evaluation-only, pending review |
| R-36 | Trademark: "BeatFix" not cleared; competitor feature names (Warp, Flex, NewTime…) | M | M | formal search; original UI naming |
| R-37 | Dataset licences (Ballroom/Hainsworth NC, GTZAN/SMC unlicensed, RWC research-only) | M | L | local-only use; never shipped or used for fitting without review |

## Schedule

| ID | Risk | L | I | Mitigation |
|---|---|---|---|---|
| R-40 | DSP calibration and listening tests take longer than coding | H | M | listening infrastructure built in Phase 0; rolling A/B per change |
| R-41 | Single developer bus factor | M | H | docs-first planning (this package); CI enforces gates |
| R-42 | Upstream churn (JUCE 9 API, ORT, Signalsmith) | M | L | pinned versions; adapters isolate APIs |
