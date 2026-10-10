# Acceptance Criteria (user-visible test matrix)

Brief §44, expanded. Each test runs on the release candidate on REF-MID. Inputs refer to [BENCHMARK_CORPUS.md](BENCHMARK_CORPUS.md) item IDs (S = synthetic, M = MIDI-rendered, C = CC-licensed, U = Suno with permission, local only). Metrics are defined in [METRICS.md](METRICS.md). Thresholds marked † are PROVISIONAL and frozen after the Phase 0 calibration ([DSP_TEST_STRATEGY.md](DSP_TEST_STRATEGY.md) §7). The steps column assumes default settings (Natural N_c = 8, Smooth, Preserve start) unless stated.

**MVP release = every AT below passes.** A failure needs either a fix or a recorded waiver in [OPEN_QUESTIONS.md](../10_implementation/OPEN_QUESTIONS.md) signed off by the owner.

## 1. Analysis and correction behaviour

| ID | Scenario | Input | Steps | Expected | Pass criteria | REQ |
|---|---|---|---|---|---|---|
| AT-01 | Stable song | S01 click+drums 110.00 BPM; C stable pop | Open | BPM ≈ 110, High confidence, Stable, waveform almost all grey | \|BPM − 110\| ≤ 0.05; class-0 share ≥ 95 %†; output re-detected beats vs grid median ≤ 5 ms†; output nulls vs source ≤ −60 dBFS where r = 1† | BEAT-001, TEMPO-001, VIS-003 |
| AT-02 | Slight drift | S02 ramp 108→113 | Open, target 110, render | Drifting class; smooth correction; no seams | Path R median ≤ 5 ms, P95 ≤ 15 ms†; max adjacent ratio step ≤ 0.5 %†; 0 clicks (discontinuity detector) | TEMPO-004, WARP-004/006 |
| AT-03 | Severe drift | S03 ramp 100→120; U drift set | Open, render | Drifting class; segments ≥ 8 % flagged | Same Path R thresholds as AT-02; every segment with \|r/g−1\| ≥ 8 % appears in Issues | WARP-002, UI-010 |
| AT-04 | Intentional tempo step | S04 120→126 step at bar 32 | Open | Stepped class; flag + ask; never auto-smeared | Default leaves choice pending; "keep" → r = 1 ± 1e-9 in section; "flatten" → step absorbed ≤ 1 beat | TEMPO-007, WARP-005 |
| AT-05 | Splice / phase jump | S05 30 ms and 150 ms jumps | Open, render | 30 ms absorbed in one beat; 150 ms flagged "possible splice" | all other r_loc within 0.5 %; Issues entry with time and size | TEMPO-008 |
| AT-06 | Half-time ambiguity | S06 70/140 trap pattern; C halftime | Open, type 140 without relabel | Amber/Red margin, picker auto-expanded; out-of-range ratio refused | Render refused with half/double offer; after relabel, correction ≤ 1 % | TEMPO-005, WARP-002, UI-004 |
| AT-07 | Drumless intro | S07 16-bar pad intro + drums | Open, render | Intro beats low confidence (dotted), tempo interpolated, no wobble | Intro r_loc within 1 %† of neighbours; first drum downbeat on grid ≤ 5 ms† | WARP-005, VIS-002 |
| AT-08 | Drum-heavy | M drum set; C EDM | Render, listen | Transients remain sharp | Onset sharpness ratio ≥ 0.8† (METRICS); listening non-inferior (LIS) | STR-001 |
| AT-09 | Vocal-heavy | C vocal; U vocal | Render, listen | Pitch preserved, no warble | f0 deviation median ≤ 5 cents; listening non-inferior | STR-002 |
| AT-10 | Sustained pads | S10 sines/chords | Render | No phasiness or pitch drift | f0 ≤ 5 cents; no amplitude modulation > 1 dB† | STR-002 |
| AT-11 | Wide stereo | S11 known ITD + decorrelated reverb | Render | Image preserved | ITD ± 1 sample; correlation Δ within STEREO_PHASE_PRESERVATION limits† | STR-003, AUDIO-010 |
| AT-22 | No steady beat | S14 silence, −90 dBFS noise, C ambient | Open | "No steady beat found"; export disabled; manual BPM offered | Confidence None; no render | BEAT-007 |

## 2. Fidelity, formats and export

| ID | Scenario | Input | Steps | Expected | Pass criteria | REQ |
|---|---|---|---|---|---|---|
| AT-12 | Dense master near 0 dBFS | C/U master at −0.1 dBFS | Render, export MP3 | Peak gate appears with 3 choices | "Lower gain" output ≤ −1.0 dBTP; "export anyway" reports clip count; nothing silent | EXP-003, AUDIO-008 |
| AT-13 | MP3 source gapless | S click MP3 CBR320/V0/V2 @ 44.1/48 | Open, export WAV | No offset | 0-sample offset vs original; length exact | AUDIO-003, EXP-001 |
| AT-14 | Decode once | any | Open, analyse, 5 target changes, export | — | Decoder invoked once; reopen with valid cache → 0 decodes | IMP-007, AUDIO-004 |
| AT-15 | Target change re-renders from source | U drift song | Export MP3 at 110; change to 112; export | Second export built from source + state, not from first MP3 | Second export equals a fresh direct render at 112 within D2; no MP3 decode in the trace | RND-001, AUDIO-002 |
| AT-16 | Mode / strictness change | U drift song | Toggle Natural→Strict→Natural; Smooth↔Resample (Global) | Cache hits on return; Resample shows cents | Return to previous state re-renders 0 chunks; Resample label shows ±cents | RND-004, STR-004, UI-006 |
| AT-20 | Export validation | fault-injected writer | Export | Failure detected and reported; no file left | Validation catches length/NaN/offset fault; temp removed | EXP-007/008 |

## 3. Robustness and recovery

| ID | Scenario | Steps | Expected | Pass criteria | REQ |
|---|---|---|---|---|---|
| AT-17 | Source moved / renamed / replaced | Save project, move source, reopen; then replace with a different file of the same name | Relink flow finds the moved file; replaced file triggers hash-mismatch warning | Relink by search succeeds; mismatch never silently accepted | PROJ-006 |
| AT-18 | Corrupt file | Open truncated/damaged MP3/FLAC fixtures | Damaged frames skipped with warning or clear refusal | No crash; length equals readable content; Issues entry with time | IMP-009, SEC-007 |
| AT-19 | Unsupported codec | Open M4A, Opus, 6-ch WAV, non-audio | Rejected at probe with guidance | Message names the property; no project/cache created | IMP-003 |
| AT-21 | Crash recovery | Kill process during editing and during export | Recovery dialog on restart; no partial export | Edits since last autosave ≤ 60 s lost; target path has no partial file | PROJ-005, REL-002/004 |
| AT-23 | Cancel | Cancel analysis, render and export mid-way | Previous valid state restored | Each cancel ≤ 250 ms†; no leaked temp files | REL-001, PERF-008 |
| AT-24 | Offline and privacy | Run AT suite with network monitor | No connections; logs contain no paths/audio | 0 outbound connections; log grep clean | SEC-001/003 |
| AT-25 | Accessibility | Scripted keyboard-only + NVDA session: open, pick candidate, set target, render, A/B, export | Fully operable | All steps completed without pointer; all controls announced | ACC-001/002 |

## 4. Performance acceptance
Run PERF-* from [PERFORMANCE_BENCHMARKS.md](PERFORMANCE_BENCHMARKS.md) on REF-MID with a 4-min and a 10-min song: REQ-PERF-001…014 targets met.

## 5. Listening acceptance
Release-gate MUSHRA round per [SUBJECTIVE_LISTENING_TESTS.md](SUBJECTIVE_LISTENING_TESTS.md): Smooth within −5 points of the Rubber Band R3 reference on drums, vocals and dense mix; the Natural default preferred or equal to Strict on U-tier drift items (test S-3).
