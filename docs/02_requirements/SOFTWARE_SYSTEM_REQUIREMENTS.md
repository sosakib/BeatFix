# Software System Requirements (SRS)

Status: v1.0, 2026-10-08. Brief §32 (plus §13, §15, §16, §21, §22). This file **contains** the functional, non-functional, performance and audio-fidelity requirements; there are no separate FUNCTIONAL/NON_FUNCTIONAL/PERFORMANCE/AUDIO_FIDELITY files ([DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §7). If anything here contradicts DECISION_SUMMARY, DECISION_SUMMARY wins and the conflict is listed in §24.

## 1. Scope

BeatFix is a Windows desktop application that imports one mono/stereo song, detects its beats and tempo, builds a monotonic warp map onto a fixed-BPM grid, renders a pitch-preserving corrected version from the immutable source, shows original vs corrected waveforms with correction heat, offers A/B preview, and exports WAV/FLAC/MP3 behind a peak gate with post-export validation (DECISION_SUMMARY §5). Everything listed in DECISION_SUMMARY §4 is out of scope for the MVP and appears here only as **Post-MVP**.

## 2. Conventions

### 2.1 Requirement block
Each requirement has: **ID** · title · **priority** · **verification method**, then *Shall* (description), *Why* (rationale), *Accept* (measurable acceptance criteria), *Deps* (dependencies).

| Priority | Meaning |
|---|---|
| **MVP-Must** | Release 1.0 is blocked without it |
| **MVP-Should** | Planned for 1.0; may slip to 1.x with a recorded decision; must not be half-shipped |
| **Post-MVP** | Not built in 1.0. Recorded so the architecture does not preclude it |

| Verification | Meaning |
|---|---|
| **Test** | Automated (Catch2 / CLI + `tools/eval`), pass/fail from numbers |
| **Analysis** | Calculation, static analysis, measurement report or code-path review with numbers |
| **Inspection** | Review of code, config, UI copy, SBOM or installer contents against a checklist |
| **Demonstration** | Scripted manual run (acceptance matrix, screen reader session) |
| **Listening** | Blind listening protocol from [SUBJECTIVE_LISTENING_TESTS.md](../07_quality/SUBJECTIVE_LISTENING_TESTS.md) |

### 2.2 Thresholds
**PROVISIONAL** = proposed number to be calibrated in the Phase-0 spike and frozen per [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) §7: threshold = max(perceptual bound, reference-engine P95 × 1.2); a threshold tighter than the reference engine achieves is escalated, not silently loosened. Unmarked numbers are hard (contractual, never re-baselined). Metric IDs A*, O*, P* are defined in [METRICS.md](../07_quality/METRICS.md); test IDs (DSP-*, GOLD-*, PERF-*, LIS-*, R*, D*) in the quality and algorithm docs; acceptance tests AT-* in [ACCEPTANCE_CRITERIA.md](../07_quality/ACCEPTANCE_CRITERIA.md).

### 2.3 Notation
`s` source seconds, `t` target seconds, `W(s)=t` warp map, `W⁻¹`, `r` segment time ratio (output/input; >1 slowed), `g` global ratio (B_detected/B_target), `r_loc = r/g`, `B` target BPM, `P = 60/B`, `N_c` Natural↔Strict cutoff in beats, RT = real-time duration of the source, REF-MID = 4C/8T AVX2 16 GB laptop ([PERFORMANCE_BENCHMARKS.md](../07_quality/PERFORMANCE_BENCHMARKS.md) §1).

### 2.4 Ratio thresholds (DECISION_SUMMARY §6)
Four different quantities, never conflated:

| Quantity | Threshold | Consequence |
|---|---|---|
| per-segment local deviation `\|r/g − 1\|` | ≥ 8 % | quality warning, segment highlighted |
| per-segment local deviation `\|r/g − 1\|` | ≥ 12 % | "extreme" flag: likely missed/extra beat, Issues entry |
| global change `\|g − 1\|` | ≥ 15 % | large-overall-change warning |
| any segment `r` | outside [0.5, 2.0] | render refused, half/double fix offered |

All four are PROVISIONAL except the hard [0.5, 2.0] bound.

---

# FUNCTIONAL REQUIREMENTS

## 3. REQ-IMP: Import and decode

**REQ-IMP-001 · Supported import formats** · MVP-Must · Test
- *Shall:* The system shall import WAV (PCM u8/s16/s24/s32, IEEE float 32/64, `WAVE_FORMAT_EXTENSIBLE`), RF64/BW64, AIFF/AIFF-C, FLAC, MP3 (MPEG-1/2/2.5 Layer III, CBR/VBR/ABR) and Ogg Vorbis.
- *Why:* Suno delivers MP3/WAV; refusing a user's file is the worst first-run failure. AAC/M4A/Opus are excluded pending licensing review (DECISION_SUMMARY §3.7).
- *Accept:* one fixture per variant in the compatibility matrix decodes; lossless variants bit-exact vs generator buffer (DSP-DEC-1); RF64 > 4 GiB sparse fixture opens.
- *Deps:* REQ-LIC-005; [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md).

**REQ-IMP-002 · Probe before decode** · MVP-Must · Test
- *Shall:* The system shall determine container, codec, sample rate, channel count, bit depth and duration without a full decode, and decide supported/unsupported at probe time.
- *Why:* Users must not wait through a long decode to learn a file is unsupported (UX_BEHAVIOR §15.1).
- *Accept:* probe ≤ 200 ms on a 10-min file from local SSD (PROVISIONAL, REQ-PERF-002); every unsupported fixture rejected before any PCM is produced.
- *Deps:* REQ-IMP-003.

**REQ-IMP-003 · Unsupported input rejected with guidance** · MVP-Must · Test, Inspection
- *Shall:* The system shall reject unsupported codecs, > 2 channels, unsupported sample formats/rates and over-limit durations with a message naming the detected property, the supported list, and a lossless-conversion suggestion; it shall never transcode silently or open partially.
- *Why:* Brief §21 "Unsupported codec"; no hidden quality loss.
- *Accept:* M4A/AAC, Opus, 6-ch WAV, non-audio file → ERROR(load) card with [Choose another…][Copy details]; no project/cache entries created (AT-19).
- *Deps:* UX_BEHAVIOR §15.

**REQ-IMP-004 · Channel layout** · MVP-Must · Test
- *Shall:* The system shall process mono and stereo sources at their native channel count and never up- or down-mix silently.
- *Why:* Multichannel is Post-MVP; silent downmix changes the audio the user asked to fix.
- *Accept:* mono in → mono render/export; stereo in → stereo out; 3+ ch rejected (REQ-IMP-003).
- *Deps:* REQ-AUDIO-014.

**REQ-IMP-005 · MP3 gapless decode** · MVP-Must · Test
- *Shall:* The system shall skip the Xing/Info/VBRI frame, trim `enc_delay + 529` leading and `enc_padding − 529` trailing samples from the LAME tag (iTunSMPB as fallback), and, when no delay info exists, decode untrimmed and flag "encoder delay unknown".
- *Why:* Untrimmed LAME MP3 is offset by 1105 samples (25.06 ms @ 44.1 kHz), enough to put every beat off a DAW grid (AUDIO_QUALITY_RESEARCH F1, F2).
- *Accept:* Phase-1 gate: LAME CBR320/V0/V2 click tracks at 44.1 and 48 kHz → first click at the exact source sample (offset 0, ±0 samples) and length equal to the source (DSP-DEC-2, GOLD-08); non-LAME fixture shows the flag.
- *Deps:* minimp3 `minimp3_ex` (ADR-009).

**REQ-IMP-006 · Ogg Vorbis sample-exact decode** · MVP-Must · Test
- *Shall:* The system shall honour granule positions so Vorbis decode is sample-exact.
- *Why:* Untrimmed Vorbis carries +128 samples (AUDIO_QUALITY_RESEARCH E1).
- *Accept:* click fixture offset 0, length diff 0.
- *Deps:* JUCE Ogg reader.

**REQ-IMP-007 · Decode once into a PCM cache** · MVP-Must · Test, Inspection
- *Shall:* The system shall decode each source exactly once per session into float32 PCM at native rate and channel count, cache it keyed by the source fingerprint and decoder id+version, and serve analysis, playback, render and export from that buffer.
- *Why:* One decode path makes analysis, render and validation share identical sample timing and avoids repeated lossy decoding drift (AUDIO_QUALITY_RESEARCH Risks).
- *Accept:* decoder invocation counter = 1 across open → analyse → 5 target changes → export (AT-14); reopening a project with a valid cache performs 0 decodes.
- *Deps:* REQ-RND-004, REQ-AUDIO-004.

**REQ-IMP-008 · Source fingerprinting** · MVP-Must · Test
- *Shall:* The system shall compute a file-bytes hash and a decoded-PCM hash (recording decoder name+version, sample rate, channels, frames, codec, encoder-delay-trimmed flag) and store them in the project.
- *Why:* Distinguishes "tags changed, audio identical" from "different audio" for relocation and cache invalidation (AUDIO_QUALITY_RESEARCH §4.10).
- *Accept:* tag-only edit → file hash differs, PCM hash equal; 1-sample change → PCM hash differs. Hash function: SHA-256 via Windows CNG (BCrypt), native and no extra dependency.
- *Deps:* REQ-PROJ-006.

**REQ-IMP-009 · Progressive load feedback and early original playback** · MVP-Must · Demonstration
- *Shall:* The system shall show probed metadata immediately, fill the original waveform left→right during decode at ≥ 10 updates/s, and enable original playback as soon as decode completes; Cancel during LOADING returns to EMPTY.
- *Why:* UX_BEHAVIOR §2.3–2.6; Doherty threshold.
- *Accept:* scripted run on a 10-min MP3 shows ≥ 10 waveform updates/s; Space plays the original before analysis finishes.
- *Deps:* REQ-ANA-001.

**REQ-IMP-010 · Damaged stream handling** · MVP-Must · Test
- *Shall:* The system shall continue past skippable decode errors by inserting silence of the exact skipped length, record each damaged span, add an Issues entry per span cluster, mark ruler warning bands, continue with truncated files (decoded length < header length by > 1 s) with an Issues entry, replace decoded NaN/Inf by 0 with an Issues entry, and fail to ERROR(load) only when no audio decodes.
- *Why:* Timing must stay correct around damage (brief §22 "corrupted file"; UX_BEHAVIOR §14).
- *Accept:* AT-18 fixtures: corrupted MP3 frames → total length equals the undamaged length ± 1 frame and following clicks keep their exact positions; truncated WAV → issue text with both durations; float WAV with NaN → zeros + issue; garbage header → ERROR(load).
- *Deps:* REQ-BEAT-008.

**REQ-IMP-011 · Source metadata read** · MVP-Should · Test
- *Shall:* The system shall read ID3v1/v2.2–2.4, APEv2, Vorbis comments, RIFF LIST/INFO and `id3 ` chunks and normalise them to title, artist, album, album artist, track/disc, date, genre, comment, ISRC, front-cover artwork.
- *Why:* Needed for tag carry-over (REQ-EXP-009).
- *Accept:* fixture per tag family round-trips the normalised fields.
- *Deps:* REQ-EXP-009.

**REQ-IMP-012 · Source quality report** · MVP-Should · Test
- *Shall:* The system shall report codec, bitrate, sample rate, bit depth and estimated effective bandwidth, labelling likely lossy origin.
- *Why:* Honest quality claims (REQ-AUDIO-013); Suno WAV may be lossy-origin (AUDIO_QUALITY_RESEARCH F14, UNVERIFIED).
- *Accept:* MP3 192 kbps fixture low-passed at 16 kHz reports "content ends at ~16 kHz" within ±1 kHz.
- *Deps:* REQ-AUDIO-013.

**REQ-IMP-013 · Re-import guard** · MVP-Should · Test
- *Shall:* The system shall detect the `BEATFIX_RENDER` tag on import and tell the user to open the original project to avoid another lossy generation.
- *Why:* Each lossy generation degrades audio (AUDIO_QUALITY_RESEARCH F13).
- *Accept:* importing a BeatFix MP3 export shows the notice; opening still allowed.
- *Deps:* REQ-EXP-010.

**REQ-IMP-014 · One song per document** · MVP-Must · Demonstration
- *Shall:* The system shall hold one song per window; opening another file with unsaved edits asks Save / Don't save / Cancel; dropping several files opens the first audio file with a notice.
- *Why:* MVP scope (single track).
- *Accept:* UX_BEHAVIOR §2 step 0 scripted.
- *Deps:* REQ-UI-002.

**REQ-IMP-015 · Duration and size limits** · MVP-Must · Test
- *Shall:* The system shall accept sources up to the supported maximum duration and reject longer ones at probe with a message stating the limit.
- *Why:* Memory is predictable only up to a stated bound (brief §16).
- *Accept:* limit value PROVISIONAL (proposal: performance targets guaranteed ≤ 20 min; 20–60 min supported in memory-mapped mode; > 60 min refused). PERF-LONG-3/4 pass (complete or clear refusal, no crash/OOM). Owner decision open (§24).
- *Deps:* REQ-PERF-014.

**REQ-IMP-016 · Sample rates** · MVP-Must · Test
- *Shall:* The system shall accept 44.1, 48, 88.2, 96 and 192 kHz sources (tested tiers) and any other rate the decoder supports up to 192 kHz (untested tier); it shall process at the native rate.
- *Why:* Native-rate processing (DECISION_SUMMARY §1); brief §16.
- *Accept:* S01/S16 at each tested rate pass GOLD-01 and O3 ≤ 5 ms; > 192 kHz rejected at probe (PROVISIONAL limit).
- *Deps:* REQ-AUDIO-005.

## 4. REQ-ANA: Analysis pipeline

**REQ-ANA-001 · Background auto-analysis** · MVP-Must · Demonstration
- *Shall:* The system shall start analysis automatically after decode when "Analyse on open" is on (default on), on a worker thread, keeping original playback, zoom and scroll available.
- *Why:* Zero required decisions on the primary path (UI_UX_PRINCIPLES P1).
- *Accept:* during ANALYSING, play/zoom respond; P5 stall limits hold (REQ-PERF-010).
- *Deps:* REQ-PERF-004.

**REQ-ANA-002 · Staged, honest progress** · MVP-Must · Demonstration
- *Shall:* The system shall report stages Reading file → Decoding audio → Finding beats → Estimating tempo & bars → Building correction → (background) Rendering preview, each determinate where possible; the card appears only after 400 ms and stays ≥ 500 ms; ETA appears after 2 s and is hidden when unstable (variance > 50 %).
- *Why:* MAIN_WINDOW §4.2; prevents flicker and false precision.
- *Accept:* scripted run timestamps match the constants (UX_BEHAVIOR §16).
- *Deps:* —

**REQ-ANA-003 · Separate analysis branch** · MVP-Must · Test
- *Shall:* The system shall derive a 22.05 kHz mono analysis copy with a zero-phase analysis-tier resampler; this copy never feeds render or export.
- *Why:* Beat times in seconds must be identical in both domains ([RESAMPLING.md](../04_algorithms/RESAMPLING.md) R-A1).
- *Accept:* R-A1: onset times equal within 0.05 ms; render path code review shows no dependency on the analysis buffer.
- *Deps:* REQ-AUDIO-005.

**REQ-ANA-004 · Beat activation model** · MVP-Must · Test, Inspection
- *Shall:* The system shall compute beat and downbeat activations with Beat This! (full model by default) exported to ONNX and run on ONNX Runtime 1.30 CPU EP, and shall switch the default to the small model if the Phase-3 performance gate fails.
- *Why:* DECISION_SUMMARY §1 (ADR-003).
- *Accept:* model SHA-256 verified at load (REQ-SEC-008); REQ-PERF-004 met with the chosen model on REF-MID.
- *Deps:* REQ-SEC-002, REQ-LIC-007.

**REQ-ANA-005 · DSP fallback tracker** · MVP-Must · Test
- *Shall:* The system shall always compute the DSP tracker (HPSS + SuperFlux onset function → same decoder) for cross-checking and shall use it alone when the ONNX path is unavailable or fails.
- *Why:* Brief §36 requires a fallback; DSP-vs-ML agreement backs confidence ([BEAT_TRACKING.md](../04_algorithms/BEAT_TRACKING.md)).
- *Accept:* with the model file removed, analysis completes on S01/S08 with A10 median ≤ 5 ms (PROVISIONAL) and a "fallback analyser" note.
- *Deps:* REQ-BEAT-006.

**REQ-ANA-006 · Persisted analysis, explicit re-analysis** · MVP-Must · Test
- *Shall:* The system shall persist activations, refined beats, confidences, parameters, model hash and analyser version, and shall re-analyse only on an explicit user action.
- *Why:* ORT CPU inference is not bit-exact across machines; renders must not change silently (DECISION_SUMMARY §3.4).
- *Accept:* reopening a project on a second machine reproduces the stored beats exactly (no inference run); re-analysis requires Ctrl+Alt+R/menu.
- *Deps:* REQ-PROJ-002.

**REQ-ANA-007 · Analysis determinism** · MVP-Must · Test
- *Shall:* The system shall produce identical analysis results for identical input, build, CPU path and thread settings (D1), and results within D2 tolerance across machines.
- *Why:* Reproducible bug reports and goldens (DSP_TEST_STRATEGY §6).
- *Accept:* D1: 10 runs → identical beat arrays; D2: beats ±0.5 ms, BPM ±0.001; bootstrap uses a fixed seed; ORT thread counts and graph optimisation level fixed.
- *Deps:* REQ-TEST-005.

**REQ-ANA-008 · Analysis cancellation** · MVP-Must · Test
- *Shall:* The system shall cancel analysis (including an in-flight ONNX `Run` via `RunOptions::SetTerminate`) and return to DECODED with an [Analyse] button, keeping no partial results.
- *Why:* Brief §21 "Analyze: cancellation".
- *Accept:* cancel latency ≤ 250 ms over 100 random cancels (PERF-CANCEL-1); no analysis data persisted.
- *Deps:* REQ-REL-001.

**REQ-ANA-009 · Analysis failure** · MVP-Must · Test
- *Shall:* The system shall return to DECODED on engine error or out-of-memory and show "Analysis failed: <plain reason>" with [Retry] [Copy details]; details contain engine, version, stage and error code, local only.
- *Why:* Brief §22; never crash on analysis failure.
- *Accept:* fault injection (ORT error, allocation failure) → card shown, app responsive, playback still works.
- *Deps:* REQ-REL-003.

**REQ-ANA-010 · Immutable analysis results** · MVP-Must · Test
- *Shall:* The system shall treat each analysis result as immutable; re-analysis creates a new result as an undoable command and asks for confirmation if interpretation edits exist.
- *Why:* Undo/redo correctness (UX_BEHAVIOR §3.6).
- *Accept:* undo after re-analysis restores the previous result and its edits.
- *Deps:* REQ-UI-008.

**REQ-ANA-011 · Unreliable-analysis gate** · MVP-Must · Test
- *Shall:* The system shall mark the analysis "beat analysis unreliable" and not auto-render when the robust residual scale exceeds 0.15·P or the stability class is Unreliable (σ̂ > 25 ms, tracker agreement < 0.6, or > 30 % inferred beats).
- *Why:* Huber IRLS converges to a wrong curve when > 30 % of anchors are wrong ([WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md) Risks; [TEMPO_STABILITY_ANALYSIS.md](../04_algorithms/TEMPO_STABILITY_ANALYSIS.md)).
- *Accept:* S05 σ = 20 ms + 30 % FP injection → flag shown, no preview render started. Thresholds PROVISIONAL.
- *Deps:* REQ-TEMPO-006.

## 5. REQ-BEAT: Beat and downbeat detection

**REQ-BEAT-001 · Beat detection** · MVP-Must · Test
- *Shall:* The system shall estimate beat times (float64 seconds) over the whole file, including intros and outros.
- *Why:* Beats are the warp anchors.
- *Accept:* DSP-ANA-1 on S01–S08: A10 median ≤ 2 ms, P95 ≤ 5 ms after refinement (PROVISIONAL); real music: corpus CMLt (A5, untrimmed) reported per category, release blocked if worse than the previous release beyond the bootstrap CI (QA_STRATEGY §6).
- *Deps:* REQ-ANA-004/005.

**REQ-BEAT-002 · Downbeats and meter** · MVP-Must (4/4) / MVP-Should (3/4) · Test
- *Shall:* The system shall estimate downbeats and bar phase for 4/4, and for 3/4 when detected; other meters are flagged as unsupported.
- *Why:* Bar labels, DAW-ready start and Set bar 1 depend on downbeats ([DOWNBEAT_ESTIMATION.md](../04_algorithms/DOWNBEAT_ESTIMATION.md)).
- *Accept:* A7 downbeat F ≥ 0.9 on Tier M 4/4 items (PROVISIONAL); 7/8 item shows the unsupported flag rather than a wrong bar grid.
- *Deps:* REQ-GRID-005.

**REQ-BEAT-003 · Owned decoder with repair** · MVP-Must · Test
- *Shall:* The system shall decode activations with its own deterministic DP decoder constrained to a ±40 % tempo corridor around the chosen metrical level, inferring missing beats and removing extra beats.
- *Why:* ADR-003; ghost notes must not become anchors ([OUTLIER_REJECTION.md](../04_algorithms/OUTLIER_REJECTION.md)).
- *Accept:* DSP-ANA-3: FP ≤ 2 %, FN ≤ 2 % for σ ≤ 5 ms and ghosts at −18 dB (PROVISIONAL); S06 4-bar dropout produces no ratio spike beyond the 8 % warning (§2.4).
- *Deps:* REQ-TEMPO-004.

**REQ-BEAT-004 · Sub-frame refinement** · MVP-Must · Test
- *Shall:* The system shall refine each beat by parabolic interpolation of the activation peak, optionally snap within ±25 ms to a high-resolution onset in one fixed band per song, and apply one global median offset correction to all anchors.
- *Why:* Beat This! frames are 20 ms (σ ≈ 5.8 ms quantisation); per-beat band switching would inject jitter (WARP_FUNCTION §5).
- *Accept:* frame-quantised input yields no snapping of BPM to 107.1/111.1 (BPM_ESTIMATION tests); A10 bias \|mean e\| ≤ 1 ms on S01(c) (PROVISIONAL).
- *Deps:* [ONSET_DETECTION.md](../04_algorithms/ONSET_DETECTION.md).

**REQ-BEAT-005 · Per-beat confidence** · MVP-Must · Test
- *Shall:* The system shall attach a confidence ∈ [0, 1] and an `inferred` flag to every beat.
- *Why:* Confidence weights the warp smoother and drives dotted markers.
- *Accept:* range check; monotonic: more injected jitter ⇒ lower or equal confidence (S05 σ 2→20 ms).
- *Deps:* REQ-WARP-005.

**REQ-BEAT-006 · Global confidence with reason** · MVP-Must · Test, Analysis
- *Shall:* The system shall report global confidence High / Medium / Low / None with a one-line reason, computed from activation strength, residual scale, phase coherence R, candidate margin and inferred-beat fraction; thresholds live in one config file and are logged with each analysis.
- *Why:* Confidence decides prompts (REQ-UI-003); Beat This! is overconfident by design.
- *Accept:* calibrated so "High" ⇒ < 2 % octave-error rate and median \|ΔBPM\| < 0.1 on the stable subset; ECE (A13) reported. Thresholds PROVISIONAL, fitted on synthetic, MIDI-rendered and CC material only (Suno-fitted calibration is for legal review, DECISION_SUMMARY §6).
- *Deps:* REQ-TEST-007.

**REQ-BEAT-007 · No steady beat** · MVP-Must · Test
- *Shall:* The system shall report "None" when no steady beat exists, build no warp map, disable export with "Nothing to correct: no beats were detected.", and still allow a typed BPM for grid display and click overlay.
- *Why:* Never force a confident wrong warp (SMC "blind spot" pattern; UX_BEHAVIOR §4.4).
- *Accept:* S14 silence, −90 dBFS noise and C ambient items → None (AT-22).
- *Deps:* REQ-UI-003.

**REQ-BEAT-008 · Confidence in damaged spans** · MVP-Should · Test
- *Shall:* The system shall cap beat confidence at Low inside damaged spans.
- *Why:* Inserted silence is not musical evidence.
- *Accept:* beats in REQ-IMP-010 spans carry Low confidence.
- *Deps:* REQ-IMP-010.

## 6. REQ-TEMPO: Tempo estimation and stability

**REQ-TEMPO-001 · Global tempo** · MVP-Must · Test
- *Shall:* The system shall estimate the global BPM in float64 with a Huber IRLS linear fit of beat time on beat index, cross-checked by Theil–Sen (flag if they differ by > 0.05 BPM).
- *Why:* Mean IOI telescopes to the endpoints; median IOI snaps to the frame lattice ([BPM_ESTIMATION.md](../04_algorithms/BPM_ESTIMATION.md) §3). Brief example REQ-BEAT-001 "estimate musical tempo" is covered here.
- *Accept:* synthetic 110.000 / 109.96 / 127.5 BPM, σ ≤ 15 ms, ≥ 200 beats → \|error\| < 0.01 BPM; +100 ms endpoint outlier moves the estimate < 0.005 BPM; A11 ≤ 0.01 BPM (S) / ≤ 0.05 BPM (real stable items, PROVISIONAL).
- *Deps:* REQ-BEAT-004.

**REQ-TEMPO-002 · Tempo uncertainty and range** · MVP-Must · Test
- *Shall:* The system shall report a 95 % CI (block bootstrap, block 8 beats, B = 500, fixed seed) and `bpmRange` = [p5, p95] of the smoothed local tempo.
- *Why:* For drifting songs the range is the honest answer (BPM_ESTIMATION §3).
- *Accept:* 108→113 ramp: bpmRange covers [108.3, 112.7] within ±0.3 and bpmGlobal ≈ 110.5; repeated runs identical.
- *Deps:* REQ-TEMPO-003.

**REQ-TEMPO-003 · Local tempo curve** · MVP-Must · Test
- *Shall:* The system shall compute the local tempo curve from the shared Whittaker/IRLS smoother at a fixed display cutoff N_c = 8, independent of the user's Natural↔Strict setting.
- *Why:* One implementation for displayed and applied tempo; the graph must not move with the slider.
- *Accept:* changing the detent leaves `localBpm` unchanged bit-for-bit.
- *Deps:* REQ-WARP-004.

**REQ-TEMPO-004 · Metrical-level candidates** · MVP-Must · Test
- *Shall:* The system shall generate candidates B̂ × {1/3, 1/2, 2/3, 1, 3/2, 2, 3} within 40–250 BPM, score them, and classify the margin Green / Amber / Red.
- *Why:* A 55 BPM floor forces double-tempo answers on ~21 % of slow SMC tracks (DECISION_SUMMARY §6).
- *Accept:* closure contains the true level for clicks at 60/90/120/174 BPM; A14 correct level in top-2 on 100 % of S08 half/double items; corridor path never jumps an octave on a click track with 25 % dropouts.
- *Deps:* [HALF_DOUBLE_TIME_RESOLUTION.md](../04_algorithms/HALF_DOUBLE_TIME_RESOLUTION.md).

**REQ-TEMPO-005 · Ambiguity surfaced, never silent** · MVP-Must · Test
- *Shall:* The system shall expose every candidate within the ambiguity margin and raise an explicit ambiguity flag on ties instead of picking silently; Amber/Red asks the user (REQ-UI-003).
- *Why:* Octave errors are catastrophic (every other beat on the grid).
- *Accept:* candidate ranking stable under ±2 % jitter; constructed tie → flag set.
- *Deps:* REQ-UI-004.

**REQ-TEMPO-006 · Stability classification** · MVP-Must · Test
- *Shall:* The system shall detect change points and splices first, then classify the song and each region as Stable, Drifting, Stepped, Spliced, Level switch or Unreliable, with numeric evidence (max drift ms vs constant fit, IBI CV).
- *Why:* Each class needs a different warp consequence ([TEMPO_STABILITY_ANALYSIS.md](../04_algorithms/TEMPO_STABILITY_ANALYSIS.md)).
- *Accept:* S03 → Drifting; S04 step located ±1 beat; injected phase jumps 20–200 ms detected and located ±1 beat; detector noise S05 σ ≤ 10 ms classified Stable. Class thresholds PROVISIONAL. Level-switch detection MVP-Should.
- *Deps:* REQ-ANA-011.

**REQ-TEMPO-007 · Intentional tempo change: flag and ask** · MVP-Must · Test, Demonstration
- *Shall:* The system shall flag a detected intentional tempo change and offer "flatten to target" or "keep that section unwarped (r = 1)", inserting a smoother break (`g_i = 0`) at the change point; it shall never auto-smear a step into a ramp.
- *Why:* Protects intentional music (DECISION_SUMMARY §6).
- *Accept:* S04 120→126: default leaves the choice pending with an Issues entry; "keep" yields r = 1 ± 1e-9 inside the section; "flatten" yields no ratio spike beyond the 8 % warning outside the step beat (AT-04).
- *Deps:* REQ-WARP-005.

**REQ-TEMPO-008 · Splice handling** · MVP-Must · Test
- *Shall:* The system shall absorb a splice (phase jump) within one inter-onset interval when the resulting ratio stays inside the 8 % warning band, otherwise flag it to the user with its position and size; it shall never spread a splice over N_c beats.
- *Why:* A smoothed splice creates an audible tempo surge (TEMPO_ESTIMATION_RESEARCH §5).
- *Accept:* AT-05: 30 ms jump absorbed in one beat (all other r_loc within 0.5 %); 150 ms jump flagged "possible splice at m:ss; +150 ms".
- *Deps:* REQ-TEMPO-006.

**REQ-TEMPO-009 · Target suggestion** · MVP-Must · Test
- *Shall:* The system shall suggest `round(bpmGlobal)` as the default target, also offering the x.5 value when the song is stable and \|bpmGlobal − round\| > 0.25.
- *Why:* Fixed-BPM workflows use integer tempi; AI generators are believed to be conditioned on integers (UNVERIFIED).
- *Accept:* 109.96 → 110; stable 127.4 → 127 with 127.5 offered.
- *Deps:* REQ-UI-005.

## 7. REQ-GRID: Target beat grid

**REQ-GRID-001 · Fixed grid** · MVP-Must · Test
- *Shall:* The system shall generate target beat times `T_k = φ + k·60/B` in float64 for any target `B` in 40–250 BPM entered with 0.01 BPM resolution.
- *Why:* A 0.1 BPM error is 0.27 s of drift over 5 min at 110 BPM ([TARGET_BEAT_GRID.md](../04_algorithms/TARGET_BEAT_GRID.md) §6).
- *Accept:* over 10 000 beats `|T_k − (φ + kP)| < 1e-9 s`. Inspection finds no float32 in grid code.
- *Deps:* REQ-AUDIO-006.

**REQ-GRID-002 · Beat-count preservation** · MVP-Must · Test
- *Shall:* The system shall map source anchor `i` to grid slot `i − i_0`, in order, and shall never add or delete musical beats.
- *Why:* A dropped or duplicated beat is a catastrophic, audible time jump.
- *Accept:* for every S-tier corpus item, `#anchors(out) == #anchors(in)`. The re-detected output beat count is within ±1 of the input (edge beats).
- *Deps:* REQ-BEAT-004.

**REQ-GRID-003 · Phase policy "Preserve start" (default)** · MVP-Must · Test
- *Shall:* By default the system shall place φ so that the first confident downbeat keeps its source time.
- *Why:* Least surprising. The pre-roll is left untouched.
- *Accept:* `|T(d_0) − S(d_0)| ≤ max(1 sample, |W(0)|)` on S-tier fixtures (output-origin rule, TARGET_BEAT_GRID §5.1).
- *Deps:* REQ-GRID-005.

**REQ-GRID-004 · Phase policy "DAW-ready start"** · MVP-Should · Test
- *Shall:* On request, the system shall prepend digital silence so that the first downbeat lands exactly on a bar line counted from t = 0.
- *Why:* The file then drops onto bar 1/2 of a DAW at tempo B (differentiator D10, [COMPETITIVE_ANALYSIS.md](../01_product/COMPETITIVE_ANALYSIS.md)).
- *Accept:* first downbeat at `m·beatsPerBar·60/B` ± 1 sample. Prepended samples are exact zeros. The warp map is unchanged.
- *Deps:* REQ-GRID-005.

**REQ-GRID-005 · Bars from downbeats** · MVP-Must (4/4) / MVP-Should (3/4) · Test
- *Shall:* The system shall derive bar lines from the downbeat phase and the meter (4/4 by default, 3/4 when detected or chosen), and shall let the user shift bar 1 by ±1 beat.
- *Why:* The Bars.Beats display and DAW-ready start depend on it ([DOWNBEAT_ESTIMATION.md](../04_algorithms/DOWNBEAT_ESTIMATION.md)).
- *Accept:* a downbeat shift is one undo step and moves every bar line by exactly one beat.
- *Deps:* REQ-BEAT-002.

## 8. REQ-WARP: Warp map

**REQ-WARP-001 · Monotonic mapping** · MVP-Must · Test
- *Shall:* The system shall construct a strictly increasing, piecewise-linear mapping `W` from source anchors to the target grid, with a closed-form inverse `W⁻¹`.
- *Why:* A non-monotone W reverses time. The pull-based renderer needs `W⁻¹` ([WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md) §3).
- *Accept:* property test over 10⁴ random jittered anchor sets: every `x_{i+1} − x_i ≥ Δ_min`, and `|W⁻¹(W(t)) − t| < 1e-9 s`.
- *Deps:* REQ-GRID-001.

**REQ-WARP-002 · Ratio bounds and validation** · MVP-Must · Test
- *Shall:* Before every render, the system shall validate the warp map against the four ratio thresholds of §2.4. It shall refuse to render outside [0.5, 2.0] and offer the half/double fix.
- *Why:* An out-of-range ratio means a wrong beat level, not a correction.
- *Accept:* a 70 BPM song with target 140 and no relabel → render refused and the candidate picker opened (AT-06).
- *Deps:* REQ-TEMPO-005.

**REQ-WARP-003 · Drift-free sample accounting** · MVP-Must · Test
- *Shall:* The renderer shall compute input sample spans from absolute `W⁻¹` positions. It shall never accumulate rounded counts.
- *Why:* Incremental rounding drifts by tens of ms per song (WARP_FUNCTION §7).
- *Accept:* on a 20-min render, output length = `round(W(end)·fs)` ± 1. Impulse positions are within 0.5 ms of `W(s_k)` (Path K, [METRICS.md](../07_quality/METRICS.md)).
- *Deps:* REQ-STR-005.

**REQ-WARP-004 · Shared smoother** · MVP-Must · Test
- *Shall:* The system shall compute smoothed anchors with a single Whittaker/Huber-IRLS implementation, used both for warping (user N_c) and for the local tempo display (fixed N_c = 8).
- *Why:* One implementation prevents the displayed tempo and the applied tempo from diverging.
- *Accept:* λ = 0 → x = S exactly. λ = 10⁹ → linear fit within 1e-6 s. Sinusoidal tempo modulation is attenuated per `H(ω)` ± 10 %.
- *Deps:* REQ-TEMPO-003.

**REQ-WARP-005 · Confidence weights and change-point breaks** · MVP-Must · Test
- *Shall:* The smoother shall weight anchors by per-beat confidence. It shall not apply the smoothness penalty across detected steps or splices (`g_i = 0`).
- *Why:* Drumless intros follow the smooth tempo, and intentional steps are not smeared ([TEMPO_STABILITY_ANALYSIS.md](../04_algorithms/TEMPO_STABILITY_ANALYSIS.md)).
- *Accept:* synthetic step 100→110 BPM: output step width ≤ 1 beat with the break, ≥ N_c/2 beats without it. Inferred beats (w ≈ 0.1) move the fit of confident neighbours by < 5 ms.
- *Deps:* REQ-BEAT-005, REQ-TEMPO-007.

**REQ-WARP-006 · Natural ↔ Strict control** · MVP-Must · Test, Listening
- *Shall:* The system shall expose six detents N_c ∈ {Global ∞, 32, 16, 8 (default), 4, Strict}, mapped to `λ = 1/(16 sin⁴(π/N_c))` (Strict: λ = 0).
- *Why:* Strict copies detector noise into the output. Natural keeps groove (WARP_FUNCTION §4).
- *Accept:* λ values match the table to 4 significant digits. The default N_c = 8 is confirmed or replaced by listening test S-3 before release.
- *Deps:* REQ-UI-006.

**REQ-WARP-007 · Anchor refinement** · MVP-Must · Test
- *Shall:* The system shall refine frame-quantised beats with parabolic interpolation, a fixed-band onset snap (±25 ms), and one global median offset.
- *Why:* 50 fps frames give 5.8 ms RMS error, which is up to 3.7 % local ratio error at 110 BPM.
- *Accept:* anchor RMS error ≤ 5 ms on S/M-tier items (PROVISIONAL).
- *Deps:* REQ-BEAT-003.

**REQ-WARP-008 · Deterministic derived warp map** · MVP-Must · Test
- *Shall:* The warp map shall be derived from persisted inputs only and serialised per [WARP_MAP.md](../04_algorithms/WARP_MAP.md) with `schemaVersion`, `algoVersion` and `inputsHash`. It shall be recomputed, with a visible notice, when `algoVersion` changes.
- *Why:* One source of truth and reproducible renders.
- *Accept:* the same inputs give byte-identical JSON. Round trips are stable. Unknown fields are preserved.
- *Deps:* REQ-PROJ-001.

## 9. REQ-STR: Time-stretch

**REQ-STR-001 · Smooth mode (default)** · MVP-Must · Test, Listening
- *Shall:* The system shall render the corrected audio with Signalsmith Stretch, driven by `W⁻¹` on a fixed output hop ([TIME_STRETCH_ENGINE.md](../04_algorithms/TIME_STRETCH_ENGINE.md)).
- *Why:* MIT licence, native variable ratio, linked stereo (DECISION_SUMMARY §1).
- *Accept:* non-inferior to the Rubber Band R3 reference by ≤ 5 MUSHRA points on drums, vocals and dense mix ([SUBJECTIVE_LISTENING_TESTS.md](../07_quality/SUBJECTIVE_LISTENING_TESTS.md)).
- *Deps:* REQ-WARP-003.

**REQ-STR-002 · Pitch preservation** · MVP-Must · Test
- *Shall:* In Smooth mode, the system shall not change pitch.
- *Why:* This is the core promise.
- *Accept:* on sustained-sine and harmonic fixtures, f0 deviation ≤ 5 cents median and ≤ 10 cents P95 over ratios 0.92–1.08 (PROVISIONAL).
- *Deps:* REQ-STR-001.

**REQ-STR-003 · Linked stereo** · MVP-Must · Test
- *Shall:* Both channels shall be stretched with one shared time map and linked phase.
- *Why:* Independent L/R processing smears the image ([STEREO_PHASE_PRESERVATION.md](../04_algorithms/STEREO_PHASE_PRESERVATION.md)).
- *Accept:* inter-channel correlation and M/S energy ratio stay within the PROVISIONAL deltas of STEREO_PHASE_PRESERVATION. A known ITD is preserved ± 1 sample.
- *Deps:* REQ-STR-001.

**REQ-STR-004 · Resample mode** · MVP-Should · Test
- *Shall:* The system shall offer varispeed "Resample" only when the correction is a single constant ratio (Global detent), and shall display the resulting pitch shift in cents.
- *Why:* It has zero TSM artifacts for users who accept a pitch change. Per-beat varispeed would produce vibrato.
- *Accept:* 108→110 shows "+31.8 cents". The option is disabled for every other detent.
- *Deps:* REQ-EXP-005.

**REQ-STR-005 · Timing accuracy** · MVP-Must · Test
- *Shall:* Rendered events shall land at `W(s)` within 0.5 ms (PROVISIONAL) on impulse-train fixtures, across ratios 0.8–1.25 and with per-beat ratio changes.
- *Why:* Timing is the whole product.
- *Accept:* Path K metric (METRICS.md) P95 ≤ 0.5 ms.
- *Deps:* REQ-WARP-003.

**REQ-STR-006 · Replaceable engine** · MVP-Must · Inspection, Test
- *Shall:* Stretch engines shall implement only `ITimeStretcher`. The Rubber Band R3 reference adapter shall build only under a dev flag and shall never ship.
- *Why:* GPL licence, and engines can be swapped without a rewrite ([SYSTEM_ARCHITECTURE.md](../05_architecture/SYSTEM_ARCHITECTURE.md) §3).
- *Accept:* the release-build SBOM contains no Rubber Band. Every adapter passes the same contract tests.
- *Deps:* REQ-LIC-008.

**REQ-STR-007 · Transient mode** · Post-MVP · Listening
- *Shall:* The system may offer an HPSS hybrid "Transient" mode ([TRANSIENT_PRESERVATION.md](../04_algorithms/TRANSIENT_PRESERVATION.md)) after legal review of US 9,230,558 B2.

**REQ-STR-008 · Region Auto mode** · Post-MVP · Listening
- *Shall:* Per-region strategy selection per [HYBRID_AUTO_ENGINE.md](../04_algorithms/HYBRID_AUTO_ENGINE.md), only after listening tests show a benefit.

## 10. REQ-RND: Render and cache

**REQ-RND-001 · Render from source + state only** · MVP-Must · Test, Inspection
- *Shall:* Every render shall read the cached source PCM and the current warp map. No render shall read a previous render or an export.
- *Why:* Brief §13: no generation loss.
- *Accept:* code review shows the renderer's inputs are `PcmCache` + `WarpMap` only. AT-15 (target change) output equals a fresh single-pass render within D2 tolerance.
- *Deps:* REQ-AUDIO-002.

**REQ-RND-002 · Deterministic chunked parallel render** · MVP-Must · Test
- *Shall:* The system shall render in chunks cut at quiet beat boundaries (8–16 beats), with pre-roll and crossfade, and shall give identical output for any thread count.
- *Why:* Speed without non-determinism ([RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md)).
- *Accept:* D1 bit-exact for 1, 2 and 8 threads. Chunked vs single-pass null residual ≤ −90 dBFS peak (D2, PROVISIONAL).
- *Deps:* REQ-RND-006.

**REQ-RND-003 · Progressive preview** · MVP-Must · Demonstration
- *Shall:* Chunks nearest the playhead shall render first. Corrected playback starts as soon as the chunk under the playhead is ready.
- *Accept:* REQ-PERF-006 is met.
- *Deps:* REQ-PERF-006.

**REQ-RND-004 · Content-addressed cache** · MVP-Must · Test
- *Shall:* The system shall cache source PCM and rendered chunks under keys that hash all inputs: PCM fingerprint, warp `inputsHash`, engine id/version/params, and chunk range.
- *Why:* Undo, A/B and parameter toggles become cache hits.
- *Accept:* undo after a target change re-renders 0 chunks. Changing N_c re-renders only the affected chunks.
- *Deps:* REQ-AUDIO-004.

**REQ-RND-005 · Invalidation** · MVP-Must · Test
- *Shall:* Any change to target BPM, N_c, mode, beat interpretation or downbeat shall invalidate exactly the chunks whose keys change.
- *Accept:* edit-matrix test: stale chunk count is 0 after each edit type.
- *Deps:* REQ-RND-004.

**REQ-RND-006 · Single-pass reference** · MVP-Must · Test
- *Shall:* A single-pass render path shall exist, used for tests (null reference) and as a fallback if chunked rendering fails.
- *Deps:* REQ-TEST-003.

**REQ-RND-007 · No stale corrected audio** · MVP-Must · Test
- *Shall:* Audio rendered for an older edit state shall never be played or exported as "Corrected".
- *Why:* The A/B comparison must be trustworthy ([PLAYBACK_AND_AB.md](../06_ui/PLAYBACK_AND_AB.md)).
- *Accept:* an edit during playback makes B pause with a buffering indicator at the next unrendered chunk until it is ready (PLAYBACK_AND_AB §4). Old-state audio and uncorrected audio are never played as B.
- *Deps:* REQ-PLAY-002.

## 11. REQ-PLAY: Playback and A/B

**REQ-PLAY-001 · Transport** · MVP-Must · Demonstration
- *Shall:* The system shall play, pause, stop, seek and loop original or corrected audio via WASAPI shared mode.
- *Deps:* REQ-PLAY-005.

**REQ-PLAY-002 · Seamless A/B** · MVP-Must · Test
- *Shall:* Switching A↔B shall map the position through `W`/`W⁻¹` (same musical position) and crossfade equal-power over 20 ms.
- *Accept:* when switching at beat k, audio continues at beat k ± 1 ms. The discontinuity detector (METRICS.md) finds no click.
- *Deps:* REQ-WARP-001.

**REQ-PLAY-003 · Click overlay** · MVP-Should · Demonstration
- *Shall:* An optional metronome click on the target grid (corrected) or on the detected beats (original) shall be mixable into playback.
- *Why:* It is the fastest way for a person to verify the beat interpretation.

**REQ-PLAY-004 · Loop selection** · MVP-Should · Demonstration
- *Shall:* A time selection can be looped in either lane, and the loop maps through `W` on A/B switch.

**REQ-PLAY-005 · Realtime safety** · MVP-Must · Test, Inspection
- *Shall:* The audio callback shall not allocate, lock, log or do I/O ([THREADING_MODEL.md](../05_architecture/THREADING_MODEL.md) §2).
- *Accept:* the allocation/lock hook in debug builds reports 0 violations over the AT suite. 0 dropouts at 256-sample buffers on REF-MID during analysis and render.

## 12. REQ-VIS: Visualization

**REQ-VIS-001 · Dual synchronized lanes** · MVP-Must · Demonstration
- *Shall:* The system shall show Original (top) and Corrected (bottom) lanes with shared zoom, scroll and playhead linked through `W`, and connector lines from each detected beat to its landed position ([WAVEFORM_VIEW.md](../06_ui/WAVEFORM_VIEW.md)).

**REQ-VIS-002 · Markers** · MVP-Must · Demonstration
- *Shall:* Beats, downbeats, the target grid, low-confidence beats and inferred beats shall be drawn distinctly, by shape and style and not by colour alone.

**REQ-VIS-003 · Correction classes** · MVP-Must · Test
- *Shall:* Each segment shall be classified by `p = 100·(e^{|ln(r/g)|} − 1)` into classes 0–3 (< 0.5, 0.5–2, 2–5, ≥ 5 %; PROVISIONAL and user-configurable) and drawn with Okabe-Ito hues plus a signed strip.
- *Accept:* a unit test checks the class boundaries. A greyscale screenshot still distinguishes the classes (hatching/strip).
- *Deps:* REQ-ACC-003.

**REQ-VIS-004 · Instant estimate** · MVP-Must · Test
- *Shall:* Before render, the corrected lane shall show the source peak pyramid mapped through `W`, labelled "Estimate", and replaced chunk by chunk with real peaks.
- *Accept:* the estimate is visible ≤ 100 ms after a warp change.

**REQ-VIS-005 · Peak pyramid** · MVP-Must · Test
- *Shall:* Waveforms shall be drawn from a min/max/RMS pyramid (base 64 samples per pixel, ×4 per level), and from raw PCM below 64 spp.
- *Accept:* REQ-PERF-009 is met.

**REQ-VIS-006 · Tempo graph** · MVP-Should · Demonstration
- *Shall:* An Advanced tempo graph shall plot the detected per-beat BPM, the smoothed local tempo, the target and the change points.

**REQ-VIS-007 · Inspection** · MVP-Should · Demonstration
- *Shall:* Hover cards and a region list shall show per-beat time, local BPM, ratio, class and confidence. The region list is the screen-reader equivalent of the canvas.

## 13. REQ-UI: Application UI

**REQ-UI-001 · Main window** · MVP-Must · Demonstration
- *Shall:* Layout and controls per [MAIN_WINDOW.md](../06_ui/MAIN_WINDOW.md) §1–2.

**REQ-UI-002 · Document model** · MVP-Must · Demonstration
- *Shall:* One song per window and per project. Open via dialog, drag-drop or Ctrl+O.

**REQ-UI-003 · Confidence presentation** · MVP-Must · Demonstration
- *Shall:* Global confidence (High/Medium/Low/None) shall be shown as a word plus a reason and shall drive prompts per MAIN_WINDOW §4.4. Low requires confirmation before the first export.

**REQ-UI-004 · Candidate picker** · MVP-Must · Demonstration
- *Shall:* Ranked tempo-level candidates with a Green/Amber/Red margin, auto-expanded on Amber/Red, with "Play with click" to compare.

**REQ-UI-005 · Target BPM field** · MVP-Must · Test
- *Shall:* A numeric target with 0.01 resolution. The suggestion is shown with one-click revert, and the target follows the chosen beat level unless the user typed a value.

**REQ-UI-006 · Natural ↔ Strict slider** · MVP-Must · Demonstration
- *Shall:* A six-detent slider with a live readout "grid err ≤ X ms · max ±Y %".

**REQ-UI-007 · Undo/redo** · MVP-Must · Test
- *Shall:* Every edit (target, detent, mode, interpretation, downbeat, change-point decision) is one undo step over edit-state snapshots.

**REQ-UI-008 · Reset vs re-analyse** · MVP-Must · Test
- *Shall:* "Reset corrections" restores the analysis defaults without re-analysis. "Re-analyse" is explicit and keeps the previous analysis in undo history.

**REQ-UI-009 · Keyboard shortcuts** · MVP-Must · Inspection
- *Shall:* Per [KEYBOARD_SHORTCUTS.md](../06_ui/KEYBOARD_SHORTCUTS.md).

**REQ-UI-010 · Quality status and issues** · MVP-Must · Demonstration
- *Shall:* A quality chip and an Issues list shall surface every warning class of MAIN_WINDOW §5, each with "Go to".

## 14. REQ-EXP: Export

**REQ-EXP-001 · Formats** · MVP-Must · Test
- *Shall:* Export WAV 16/24/32-float, FLAC 16/24, and MP3 CBR 320 / V0 / V2 (LAME, with the LAME/Xing gapless tag).
- *Accept:* every format round-trips with a 0-sample offset (click-track test).
- *Deps:* REQ-LIC-003.

**REQ-EXP-002 · Defaults** · MVP-Must · Inspection
- *Shall:* Defaults are WAV 24-bit with TPDF dither, FLAC 24 and MP3 CBR 320, at the source sample rate ([EXPORT_UI.md](../06_ui/EXPORT_UI.md)).

**REQ-EXP-003 · Peak gate** · MVP-Must · Test
- *Shall:* Before encoding, if the sample peak is > 0 dBFS (integer PCM) or the true peak is > −1 dBTP (lossy), the user shall choose one of: lower gain (default), opt-in true-peak limiter, or export anyway (clipped-sample count reported). Float WAV gets a warning only. Nothing happens silently.
- *Why:* TSM was measured producing +1.25 dBFS overs on a −0.1 dBFS master ([AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md)).
- *Accept:* on the AT-12 dense master the gate appears. The "lower gain" output is ≤ −1.0 dBTP (lossy) / ≤ 0 dBFS (PCM).
- *Deps:* REQ-AUDIO-008.

**REQ-EXP-004 · Dither rules** · MVP-Must · Test
- *Shall:* TPDF dither is applied only when quantising to 16/24-bit integer. It is never applied to float output or to encoder input. The seed is fixed per project.
- *Accept:* a re-export is byte-identical. Float WAV is bit-exact to the render buffer ([DITHERING.md](../04_algorithms/DITHERING.md)).

**REQ-EXP-005 · At most one SRC** · MVP-Must · Test
- *Shall:* Sample-rate conversion happens only at export, only on request or when the codec forces it, through the r8brain contract wrapper.
- *Accept:* the [RESAMPLING.md](../04_algorithms/RESAMPLING.md) contract tests pass (length, zero delay, ≥ 120 dB stopband).

**REQ-EXP-006 · Encode once** · MVP-Must · Inspection
- *Shall:* An export encodes the float render exactly once.

**REQ-EXP-007 · Output validation** · MVP-Must · Test
- *Shall:* After writing, the system shall re-decode the file and verify length, 0-sample offset (cross-correlation), NaN/Inf, peak, unexpected silence, loudness delta and FLAC MD5 ([QUALITY_VALIDATION.md](../04_algorithms/QUALITY_VALIDATION.md)).
- *Accept:* fault injection of a corrupted write is detected, the user sees the failure, and the temp file is removed.

**REQ-EXP-008 · Atomic, source-safe write** · MVP-Must · Test
- *Shall:* Write to a temp file in the destination folder, then rename atomically. Refuse any destination equal to, or hard-linked to, the source.
- *Accept:* killing the process during export leaves no partial target file. The source path is refused (file-ID check).
- *Deps:* REQ-SEC-006.

**REQ-EXP-009 · Metadata** · MVP-Should · Test
- *Shall:* Carry over title/artist/album/comment/artwork and provenance fields. Strip stale timing tags (ReplayGain, iTunSMPB, beatgrid blobs, old BPM). Write the new BPM.

**REQ-EXP-010 · Render marker tag** · MVP-Should · Test
- *Shall:* Exports shall carry a `BEATFIX_RENDER` tag (app version, project id), which REQ-IMP-013 uses.

**REQ-EXP-011 · Progress and cancel** · MVP-Must · Test
- *Shall:* Export shows progress and cancels within REQ-PERF-008 without leaving files behind.

**REQ-EXP-012 · Filenames** · MVP-Must · Test
- *Shall:* The default name is `<source stem> (<B> BPM).<ext>`. Reserved Windows names and characters are sanitised. Existing files are never overwritten without confirmation.

---

# NON-FUNCTIONAL REQUIREMENTS

## 15. REQ-AUDIO: Audio fidelity

**REQ-AUDIO-001 · Immutable source** · MVP-Must · Test, Inspection
- *Shall:* The system shall open source files read-only and never modify, move or overwrite them.
- *Accept:* the source file hash is unchanged after the full AT suite. No code path opens the source for write.

**REQ-AUDIO-002 · No repeated lossy encoding** · MVP-Must · Inspection, Test
- *Shall:* The system shall never require repeated lossy re-encoding during iterative editing. Every preview and export is rendered from the decoded source plus the current project state.
- *Accept:* code review finds no lossy encoder outside the export worker. 20 consecutive target changes followed by export equal a direct export (D2).

**REQ-AUDIO-003 · Gapless decode** · MVP-Must · Test
- *Shall:* MP3 decode shall honour the LAME/Xing delay and padding (and Info frame skip), producing a 0-sample offset.
- *Why:* An untrimmed decode shifts every beat by 1105 samples (≈ 25 ms), as measured ([AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md) F1).
- *Accept:* click-track MP3 fixtures (CBR 320, V0, V2 at 44.1/48 kHz) have offset 0 samples and length equal to the original ± 0. This is the Phase 0 gate for minimp3. Fallback: libmpg123.

**REQ-AUDIO-004 · One decoded buffer** · MVP-Must · Test
- *Shall:* Analysis and render shall derive from the same gapless-decoded float32 PCM buffer, cached and keyed by PCM hash.
- *Accept:* beat times found on the analysis branch, mapped to the render buffer, agree within 0.05 ms.

**REQ-AUDIO-005 · Native sample rate** · MVP-Must · Test
- *Shall:* Render shall run at the source's native sample rate. Only the analysis branch (REQ-ANA-003) is resampled internally. Export SRC happens at most once (REQ-EXP-005).

**REQ-AUDIO-006 · Time precision** · MVP-Must · Inspection, Test
- *Shall:* All time positions, anchors, warp values and sample indices shall be float64 or int64. Sample buffers are float32 and accumulators float64.
- *Why:* float32 cannot hold exact sample positions past 349.5 s at 48 kHz.

**REQ-AUDIO-007 · Unchanged regions** · MVP-Should · Test
- *Shall:* Where `r = 1` over a whole chunk, the corrected output shall null against the source to ≤ −90 dBFS peak (PROVISIONAL). A bit-exact pass-through is an open question (§24).

**REQ-AUDIO-008 · No silent gain or limiting** · MVP-Must · Test
- *Shall:* The system shall never apply gain change, normalisation or limiting without an explicit user choice (REQ-EXP-003). Loudness is preserved by default and the delta is reported.

**REQ-AUDIO-009 · Metering** · MVP-Must · Test
- *Shall:* Integrated LUFS and 4× oversampled true peak per ITU-R BS.1770 shall be measured via libebur128 for the source and for the render.
- *Accept:* EBU R128 compliance test vectors are within ±0.1 LU / ±0.2 dB.

**REQ-AUDIO-010 · Stereo integrity** · MVP-Must · Test
- *Shall:* Post-render checks shall compare inter-channel correlation, M/S ratio and mono fold-down loss with the source (REQ-STR-003).

**REQ-AUDIO-011 · Validation before preview is trusted** · MVP-Must · Test
- *Shall:* Every rendered chunk is checked for NaN/Inf, unexpected silence and discontinuities before it is played ([QUALITY_VALIDATION.md](../04_algorithms/QUALITY_VALIDATION.md)).

**REQ-AUDIO-012 · Determinism** · MVP-Must · Test
- *Shall:* Rendering is deterministic per the D1/D2/D3 levels of [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) (REQ-TEST-005).

**REQ-AUDIO-013 · Honest quality claims** · MVP-Must · Inspection
- *Shall:* UI text, docs and marketing shall never claim that upsampling, lossless export or processing restores information absent from the source. The source-quality report shows lossy origin and estimated bandwidth.
- *Accept:* a copy review against the vocabulary table in AUDIO_QUALITY_RESEARCH.

**REQ-AUDIO-014 · Channel count preserved** · MVP-Must · Test
- *Shall:* Output channel count equals source channel count (mono or stereo).

## 16. REQ-PROJ: Project and session

**REQ-PROJ-001 · Project stores inputs only** · MVP-Must · Inspection
- *Shall:* `.beatfix` JSON stores the source reference, fingerprints, analysis result, user edits, settings and UI state. Warp maps and audio are derived ([PROJECT_SESSION_MODEL.md](../05_architecture/PROJECT_SESSION_MODEL.md)).

**REQ-PROJ-002 · Persisted analysis** · MVP-Must · Test
- *Shall:* Activations (or a hash-addressed sidecar), the model hash and the analysis parameters are persisted, so reopening never silently re-analyses.
- *Accept:* reopen on another machine shows identical beats and BPM.

**REQ-PROJ-003 · Versioned schema** · MVP-Must · Test
- *Shall:* `schemaVersion` with pure migration functions. Unknown fields are preserved. A newer file opens read-only with a warning.

**REQ-PROJ-004 · Atomic save** · MVP-Must · Test
- *Shall:* Save writes a temp file and then calls `ReplaceFileW`. A crash at any point leaves either the old or the new project intact.

**REQ-PROJ-005 · Autosave and recovery** · MVP-Must · Test
- *Shall:* Autosave every 60 s (PROVISIONAL) when dirty, with a lock file. After a crash, offer recovery on next start.
- *Accept:* kill -9 during editing → recovery dialog restores the last autosave.

**REQ-PROJ-006 · Fingerprints and relink** · MVP-Must · Test
- *Shall:* Store file size, mtime, file hash and PCM hash. If the source is missing, offer relink by search (same name, then PCM hash match) or manual browse. A hash mismatch is a warning with the choice "use anyway (re-analyse)".
- *Accept:* moved source relinks. A different file with the same name triggers a mismatch warning (AT-17).

**REQ-PROJ-007 · Cache outside project** · MVP-Must · Inspection
- *Shall:* PCM and render caches live in `%LOCALAPPDATA%\BeatFix\cache` with an LRU size cap (default 5 GB, setting) and a "clear cache" command. Deleting the cache never loses user work.

## 17. REQ-PERF: Performance

All values are PROVISIONAL on REF-MID ([PERFORMANCE_BENCHMARKS.md](../07_quality/PERFORMANCE_BENCHMARKS.md)), calibrated in Phase 0.

| ID | Requirement | Target |
|---|---|---|
| REQ-PERF-001 | Full analysis (decode excluded) | ≤ 0.15 × RT |
| REQ-PERF-002 | Probe | ≤ 200 ms |
| REQ-PERF-003 | Decode to PCM cache | ≤ 0.05 × RT |
| REQ-PERF-004 | Beat-model inference (incl. in 001) | ≤ 0.10 × RT; otherwise switch to the small model |
| REQ-PERF-005 | Full Smooth render | ≤ 0.25 × RT |
| REQ-PERF-006 | Corrected preview start after an edit (chunk under playhead) | ≤ 500 ms |
| REQ-PERF-007 | Peak RSS, 10 min @ 48 kHz stereo | ≤ 1.0 GB |
| REQ-PERF-008 | Cancel latency (analysis, render, export) | ≤ 250 ms |
| REQ-PERF-009 | Waveform redraw per lane at 2560 px | ≤ 4 ms |
| REQ-PERF-010 | UI stalls during background work | no frame > 50 ms; P95 ≤ 16.7 ms |
| REQ-PERF-011 | A/B switch latency | ≤ audio buffer + 20 ms crossfade |
| REQ-PERF-012 | Export (WAV) after render complete | ≤ 0.05 × RT + validation |
| REQ-PERF-013 | Cold start to empty window | ≤ 2 s |
| REQ-PERF-014 | Duration tiers | targets guaranteed ≤ 20 min. 20–60 min supported via memory-mapped mode. > 60 min refused (owner decision, §24) |

Verification: Test (PERF-* benchmarks in CI smoke and the release benchmark run).

## 18. REQ-REL: Reliability

**REQ-REL-001 · Cancellation everywhere** · MVP-Must · Test
- *Shall:* Every long job (decode, analysis incl. ONNX `SetTerminate`, render, export) is cancellable via `std::stop_token`. Cancellation is not an error and leaves the previous valid state.

**REQ-REL-002 · Crash safety** · MVP-Must · Test
- *Shall:* No crash loses more than one autosave interval of edits (REQ-PROJ-005). No crash corrupts the source, the project or an existing export.

**REQ-REL-003 · Failure to safe state** · MVP-Must · Test
- *Shall:* Every failure mode in [ERROR_HANDLING.md](../05_architecture/ERROR_HANDLING.md) ends in a defined state with a user message and recovery action. No modal dead ends.

**REQ-REL-004 · No partial outputs** · MVP-Must · Test
- *Shall:* Failed or cancelled exports leave no file at the target path (REQ-EXP-008).

**REQ-REL-005 · Memory guard** · MVP-Must · Test
- *Shall:* Before decode, estimate the memory need. If it is over budget, use memory-mapped mode or refuse with a message. Never OOM-crash.

## 19. REQ-SEC: Security and privacy

Details in [SECURITY_AND_PRIVACY.md](SECURITY_AND_PRIVACY.md).

| ID | Requirement | Verification |
|---|---|---|
| REQ-SEC-001 | The application makes **no network connections** (no telemetry, update checks or cloud) | Test (firewall-monitored AT run) |
| REQ-SEC-002 | ONNX Runtime telemetry disabled at startup (`DisableTelemetryEvents`) | Inspection, Test |
| REQ-SEC-003 | Logs never contain audio data, full paths or user names | Inspection, Test (log grep) |
| REQ-SEC-004 | Crash dumps are local-only minidumps without audio memory, never uploaded, can be disabled; sharing is the user's explicit action | Inspection |
| REQ-SEC-005 | Temp files go in per-user dirs, are cleaned on exit and swept on next start | Test |
| REQ-SEC-006 | Never write to the source path or a hard link of it (file-ID check) | Test |
| REQ-SEC-007 | Decoders are fuzzed (malformed WAV/FLAC/MP3/Ogg) in CI; parse failures are errors, never crashes | Test |
| REQ-SEC-008 | The model file SHA-256 is verified before load; mismatch → DSP fallback + message | Test |

## 20. REQ-LIC: Licence compliance

| ID | Requirement | Verification |
|---|---|---|
| REQ-LIC-001 | A CI licence allow/deny check over all dependencies ([TECH_STACK.md](../08_engineering/TECH_STACK.md)) | Test |
| REQ-LIC-002 | No GPL, AGPL or non-commercial component is shipped (Rubber Band GPL, Essentia, aubio, madmom models, StaffPad, fdk-aac excluded) | Inspection (SBOM) |
| REQ-LIC-003 | LGPL components (LAME; libmpg123 if used) are dynamically linked DLLs with relinking possible and source offer/notice | Inspection |
| REQ-LIC-004 | A NOTICE / third-party licence bundle is shipped and shown in About | Inspection |
| REQ-LIC-005 | No AAC/Opus import or export until legal review clears it | Inspection |
| REQ-LIC-006 | JUCE is used under a commercial tier (never AGPL) | Inspection (owner) |
| REQ-LIC-007 | Beat This! MIT attribution is shipped; the training-data caveat is reviewed by legal before release | Inspection |
| REQ-LIC-008 | Rubber Band R3 is used only in dev/test builds and never distributed | Inspection (SBOM) |

## 21. REQ-ACC: Accessibility (WCAG 2.2 AA where applicable)

| ID | Requirement | Verification |
|---|---|---|
| REQ-ACC-001 | Every function is operable by keyboard. Every drag has a single-pointer alternative (SC 2.5.7) | Demonstration |
| REQ-ACC-002 | Controls expose accessible names, roles and states to UIA screen readers. The region list is the canvas equivalent | Demonstration (NVDA/Narrator script) |
| REQ-ACC-003 | Text contrast ≥ 4.5:1, UI graphics ≥ 3:1. Correction classes are never encoded by colour alone | Inspection, Test |
| REQ-ACC-004 | The Windows "Animation effects" off setting disables non-essential motion | Demonstration |
| REQ-ACC-005 | Pointer targets ≥ 24 × 24 px. Windows contrast themes are supported | Inspection |

## 22. REQ-TEST: Verification infrastructure

| ID | Requirement |
|---|---|
| REQ-TEST-001 | Unit tests for all `bf_dsp` math (grid, warp, smoother, BPM stats) with the cases listed in each algorithm doc |
| REQ-TEST-002 | Synthetic DSP tests (click trains, ramps, steps, jitter, chirps, sines, stereo ITD) run in CI ([DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md)) |
| REQ-TEST-003 | Golden audio tests are tolerance-based against the single-pass reference |
| REQ-TEST-004 | Benchmark corpus gate: analysis metrics (CMLt, spurious/missing-anchor rate, anchor ms, fine BPM error) on tiers S/M/C in CI and P/U locally ([BENCHMARK_CORPUS.md](../07_quality/BENCHMARK_CORPUS.md)) |
| REQ-TEST-005 | Determinism levels D1/D2/D3 are enforced as defined in DSP_TEST_STRATEGY |
| REQ-TEST-006 | A listening-test gate precedes every engine change and release ([SUBJECTIVE_LISTENING_TESTS.md](../07_quality/SUBJECTIVE_LISTENING_TESTS.md)) |
| REQ-TEST-007 | Confidence thresholds are calibrated on synthetic, MIDI-rendered and CC material only. Fitting on Suno outputs is pending legal review |
| REQ-TEST-008 | A perf smoke runs in CI and the full PERF suite runs on REF-MID before release |

## 23. Traceability

| REQ group | Design docs | Verification docs |
|---|---|---|
| IMP, AUDIO-003/004 | [AUDIO_PIPELINE.md](../05_architecture/AUDIO_PIPELINE.md), [AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md) | DSP_TEST_STRATEGY (DSP-DEC), [COMPATIBILITY_MATRIX.md](COMPATIBILITY_MATRIX.md), AT-13/18/19 |
| ANA, BEAT | [BEAT_TRACKING.md](../04_algorithms/BEAT_TRACKING.md), [ONSET_DETECTION.md](../04_algorithms/ONSET_DETECTION.md), [DOWNBEAT_ESTIMATION.md](../04_algorithms/DOWNBEAT_ESTIMATION.md), [OUTLIER_REJECTION.md](../04_algorithms/OUTLIER_REJECTION.md) | METRICS (A*), BENCHMARK_CORPUS, AT-01..09 |
| TEMPO | [BPM_ESTIMATION.md](../04_algorithms/BPM_ESTIMATION.md), [HALF_DOUBLE_TIME_RESOLUTION.md](../04_algorithms/HALF_DOUBLE_TIME_RESOLUTION.md), [TEMPO_STABILITY_ANALYSIS.md](../04_algorithms/TEMPO_STABILITY_ANALYSIS.md) | METRICS, AT-02..06 |
| GRID, WARP | [TARGET_BEAT_GRID.md](../04_algorithms/TARGET_BEAT_GRID.md), [WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md), [WARP_MAP.md](../04_algorithms/WARP_MAP.md) | unit tests in each doc, Path K/R |
| STR, RND | [TIME_STRETCH_ENGINE.md](../04_algorithms/TIME_STRETCH_ENGINE.md), [RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md), [STEREO_PHASE_PRESERVATION.md](../04_algorithms/STEREO_PHASE_PRESERVATION.md) | DSP_TEST_STRATEGY, SUBJECTIVE_LISTENING_TESTS, AT-08..11, 15 |
| PLAY, VIS, UI, ACC | [06_ui](../06_ui/) | AT-* demonstrations, accessibility script |
| EXP, AUDIO | [AUDIO_PIPELINE.md](../05_architecture/AUDIO_PIPELINE.md) §7, [QUALITY_VALIDATION.md](../04_algorithms/QUALITY_VALIDATION.md), [DITHERING.md](../04_algorithms/DITHERING.md), [RESAMPLING.md](../04_algorithms/RESAMPLING.md) | AT-12, 13, 20 |
| PROJ, REL | [PROJECT_SESSION_MODEL.md](../05_architecture/PROJECT_SESSION_MODEL.md), [ERROR_HANDLING.md](../05_architecture/ERROR_HANDLING.md) | AT-16..21 |
| PERF | [THREADING_MODEL.md](../05_architecture/THREADING_MODEL.md), RENDER_PIPELINE | [PERFORMANCE_BENCHMARKS.md](../07_quality/PERFORMANCE_BENCHMARKS.md) |
| SEC, LIC | [SECURITY_AND_PRIVACY.md](SECURITY_AND_PRIVACY.md), [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md), [TECH_STACK.md](../08_engineering/TECH_STACK.md) | CI licence check, SBOM inspection |

## 24. Owner decisions and open conflicts
1. Maximum supported duration (REQ-IMP-015 / REQ-PERF-014): proposed tiers ≤ 20 / ≤ 60 min.
2. Bit-exact pass-through for `r = 1` chunks (REQ-AUDIO-007): measured in Phase 0. If Signalsmith cannot pass through exactly, the requirement stays at −90 dBFS null.
3. Whether to use Suno outputs to fit the calibration models (REQ-TEST-007) is pending legal review.
4. Licence items REQ-LIC-006/007 need owner and legal sign-off before release.

No unresolved contradiction with DECISION_SUMMARY is known at v1.0. All open items are tracked in [OPEN_QUESTIONS.md](../10_implementation/OPEN_QUESTIONS.md).
