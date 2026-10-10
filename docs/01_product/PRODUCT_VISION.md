# Product Vision

Status: 2026-10-08, v1.0 (synthesis). Authority: [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) wins on any conflict. Evidence: [COMPETITOR_RESEARCH.md](../03_research/COMPETITOR_RESEARCH.md), [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md), [AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md), [RESEARCH_SUMMARY.md](../03_research/RESEARCH_SUMMARY.md). Personas and journeys: [USER_PERSONAS.md](USER_PERSONAS.md).

BeatFix is a Windows desktop tool that does one job: take one finished song whose timing wanders, put it on a fixed-BPM grid with pitch preserved, and show and let the user hear exactly what changed. It is not a DAW.

---

## 1. Problem

### 1.1 What goes wrong in AI-generated (and other non-gridded) songs

| Symptom | Effect in a fixed-BPM workflow | Evidence status |
|---|---|---|
| Slow tempo drift (ramps of a few %) | Song falls off a DAW/DJ grid by bar 16–32 | Existence **verified** (Suno help says tempo "is not consistent over time"). Magnitude **anecdotal** (e.g. 206→214 BPM over 3 min, vendor example). See BEAT_TRACKING_RESEARCH F11–F12 |
| Non-integer global tempo (e.g. 107.55 BPM) | Project BPM can't be set to match; loops slip | **Anecdotal** single case (COMPETITOR_RESEARCH F10) |
| Tempo steps / phase jumps at continuation seams | A smooth correction can't hide them; local lurch | **Hypothesis** H3 (BEAT_TRACKING_RESEARCH §9). Unmeasured |
| Half-/double-time and 3:2 ambiguity | Wrong displayed BPM, wrong default target | **Verified** as universal for trackers: SOTA octave-correct accuracy ≈ 83–87 % (TEMPO_ESTIMATION_RESEARCH T1–T2) |
| Weak/missing kicks, soft transients (codec/vocoder smear) | Noisy beat anchors → jitter if corrected strictly | **Hypothesis** H4. Mechanism for jitter is verified (WARP_FUNCTION.md §4.1) |
| Beatless intros, breakdowns, fills | Detector has nothing to lock to | **Verified** failure class for all trackers (BEAT_TRACKING_RESEARCH §6) |
| MP3 delivery (encoder delay 1105 samples = 25 ms @ 44.1 kHz) | Corrected file sits off the DAW grid by a constant offset if mishandled | **Measured** (AUDIO_QUALITY_RESEARCH F1, §4.12) |

### 1.2 Why existing workflows don't solve it for the target users
- **Suno Studio** can lock to a typed BPM and (since Studio 1.2) quantize warp markers, but only on the Premier plan (~$30/mo), cloud-only, with no confidence or change display; stems exported from a tempo-locked project are *anecdotally* reported to still drift (COMPETITOR_RESEARCH F1, F2).
- **DAWs** (Ableton, Logic, Cubase, Studio Pro, Pro Tools, REAPER) can all warp audio, but the standard Suno workaround is manual: set bar 1, then a warp marker every few bars, 20–60 min per song (USER_PERSONAS P2). Logic is Mac-only. Octave fixes are hidden buttons the user must know (F4).
- **FL Studio NewTime** models one BPM per sample, with manual markers plus a global quantize. It's a loop/vocal timing editor, not a whole-song drift fixer (F6, Med confidence: inferred from the manual).
- **Web "Suno tempo fixers"** are one-click but cloud-only, opaque, and by their own admission unpredictable on intentional tempo changes, rubato and beatless sections (F8).
- **DJ apps** describe drift with dynamic grids but don't render corrected audio (§9 of COMPETITOR_RESEARCH).

The capability exists. What's missing is **automatic + transparent + local + fidelity-safe** in one tool, for users who don't own or don't want a DAW workflow.

## 2. Core objective

> Understand the rhythmic structure, estimate the musical tempo, recognise timing inconsistencies, build a mathematically consistent target beat grid, warp the source onto it while preserving pitch and transients, minimise artifacts, and let the user clearly see and hear what changed. (Brief §51, restated.)

MVP in one sentence: DECISION_SUMMARY §5.

Measurable intent (hypotheses from USER_PERSONAS, to validate in interviews and tests):
- P1 (Suno creator): drop → exported fix in < 2 min, with zero settings touched on ≥ 70 % of songs.
- P2 (producer): import → on-grid in FL in < 5 min, no audible groove flattening at the default setting.
- Every render is reproducible from source + project file. No output silently clips, shifts or re-encodes.

## 3. Principles

Each principle lists **why** and the **binding consequence** for engineering. These override convenience.

### 3.1 Fidelity first
Why: the render is the only thing users keep. Detection errors can be fixed in the UI. Render damage can't (TIME_STRETCH_RESEARCH "Why It Matters"). Inputs are usually already lossy, and every extra lossy generation measurably degrades the signal (AUDIO_QUALITY_RESEARCH F13).
Consequences:
- The source file is immutable. It is decoded once (gapless-correct) and every render starts from that decoded PCM, never from a previous render or export (DECISION_SUMMARY §2 "Codec approach").
- Process at the native sample rate. At most one SRC, only at export (ADR-002). Time and positions are float64/int64. f32 positions break after 2²⁴ samples, which is 349.5 s at 48 kHz (AUDIO_QUALITY_RESEARCH §4.5).
- One lossy encode at most, at the very end. Every export is re-decoded and validated before the atomic rename.
- **Nothing silent**: peak overs trigger a gate with an explicit user choice. TSM measurably raises sample peaks by up to +1.25 dB on limited masters (DECISION_SUMMARY §6 "Peak handling").
- Honest vocabulary: no "restored", "HD", or "Hi-Res" claims for upsampling or lossless containers of lossy sources (AUDIO_QUALITY_RESEARCH §4.1).

### 3.2 Analysis is separate from correction
Why: detection is wrong often enough (octave errors ≈ 1 in 6–8 songs on generic data) that the user must be able to fix *what was heard* without touching *what will be done*. Pro tools already split these: Melodyne Assign vs Edit, Pro Tools Analysis vs Warp (COMPETITOR_RESEARCH F5).
Consequences:
- Three layers: **beat map** (interpretation: beats, level, bar 1, meter, confidence) → **correction** (target BPM, Natural↔Strict, section decisions) → **render** (derived, cached). Editing the first two never touches audio until render (WARP_MAP.md "inputs vs derived").
- Model activations are persisted, so interpretation edits (×2, ÷2, bar shift) re-run only decoding and post-processing, not inference (BEAT_TRACKING_RESEARCH §10).
- The project stores inputs. The warp map and renders are derived (ADR-005, ADR-010).

### 3.3 Confidence-aware automation
Why: one-click is the main value for P1, but a black box fails silently exactly where web fixers fail. Activation height alone is not a reliable confidence ("confident-but-wrong", BEAT_TRACKING_RESEARCH F7).
Consequences:
- Confidence comes from several independent signals: activation, salience, DSP-tracker agreement, residuals, IOI consistency (BEAT_TRACKING_RESEARCH §7). It is exposed per beat and globally (High/Medium/Low/None).
- High → defaults apply with no prompt. Medium → the alternative is shown as a chip. Low → the candidate picker opens and the first export asks for confirmation (UX_BEHAVIOR.md §4).
- Low-confidence anchors are **down-weighted, not forced**. The warp follows the smooth tempo through intros and breakdowns (WARP_FUNCTION.md §4.2).
- Intentional tempo changes are **flagged and asked about**, never auto-smeared (DECISION_SUMMARY §6). Splices are flagged, never spread over many beats.
- Default correction strength is **Natural (N_c = 8 beats)**, not Strict, because strict alignment of noisy anchors copies detector noise into the output (WARP_FUNCTION.md §4.1). This default is an **assumption** until listening tests confirm it (DECISION_SUMMARY §3.3).

### 3.4 Offline and local
Why: privacy is a positioning advantage over Suno Studio and the web fixers (D11). It's also a reliability requirement: the tool must work without accounts or connectivity.
Consequences:
- The app makes **no network calls** (DECISION_SUMMARY §1 "Packaging"). ONNX Runtime telemetry is disabled at startup (ADR-003). No telemetry, no cloud, no accounts (DECISION_SUMMARY §4).
- The ML model ships inside the installer and runs on CPU. A pure-DSP tracker works if the model is missing (ADR-003).
- Crash dumps and logs stay local (ARCHITECTURE_RESEARCH §9).

### 3.5 Show the change
Why: no competitor shows how much was stretched where (F3). Showing it is both the differentiator and the user's quality check.
Consequences: original and corrected lanes linked through W, a correction heat class on `|log2(r/g)|`, numeric values on hover, and an Issues list (WAVEFORM_VIEW.md §5, DECISION_SUMMARY §6).

### 3.6 Narrow scope
Why: every DAW already does general editing. BeatFix wins only by doing one job better (COMPETITOR_RESEARCH "Alternatives Considered" 1).
Consequence: strict MVP (DECISION_SUMMARY §4–5, [PRODUCT_SCOPE.md](PRODUCT_SCOPE.md)). Architecture leaves room for stems, batch and CLI (brief §40) without building them.

## 4. Positioning

"Automatic first, transparent always, manual when needed. Local and lossless-safe." (COMPETITOR_RESEARCH, chosen alternative 3.)

| Versus | They do | BeatFix differs by | BeatFix must not claim |
|---|---|---|---|
| **Suno Studio** (Premier) | Manual BPM lock, warp markers, quantize. In-app, cloud | Any source, offline, no subscription, confidence + heatmap, groove-preserving default, validated export with correct MP3 gapless handling | That Suno output is "broken". That BeatFix is affiliated with or endorsed by Suno (nominative use only, LICENSING_RESEARCH §8) |
| **FL Studio NewTime** | Per-sample single BPM, manual warp/groove/downbeat markers, quantize time | Whole-song variable-tempo analysis, automatic map, ranked hypotheses, Natural↔Strict, before/after lanes | NewTime feature names or UI likeness. "NewTime alternative" style marketing needs legal review (LICENSING_RESEARCH §8–9) |
| **DAWs** (Ableton, Logic, Cubase, Studio Pro, Pro Tools, REAPER) | Full warp toolkits, multitrack, many modes | Zero-skill default path, hypothesis ranking instead of hidden ×2/÷2 buttons, explicit confidence, Windows standalone (Logic is Mac-only) | Better stretch quality than élastique/Rubber Band R3 (unproven: only listening tests can establish it) |
| **Web fixers** (Neural Analog, Wondera) | One-click cloud constant-BPM | Local, inspectable, reviewable low-confidence spans, flagged intentional changes, lossless export | "Works on every song" |
| **DJ apps** (rekordbox, Serato, DJ.Studio) | Dynamic/flexible grids that *describe* drift | Removes the drift so the output grids as Fixed anywhere | — |

Strategic risk: Suno may make tempo-lock free or improve generation timing. Mitigation: stay source-agnostic (live recordings, old records, samples) and win on fidelity and transparency (COMPETITOR_RESEARCH "Risks").

## 5. Non-goals

Not built, and not in the MVP architecture beyond keeping the seams open:
- General audio editing, multitrack mixing, effects, plugin hosting (not a DAW).
- Music generation, or regeneration of bad sections. The UI may *suggest* regenerating in the source tool when correction is extreme (D12).
- Any enhancement that changes the beat-sync result. Enhancement, if ever built, is a separate opt-in stage after sync with bit-identical bypass (brief §14, AUDIO_QUALITY_RESEARCH §4.13).
- "AI restoration" claims, upsampling marketed as quality.
- Cloud processing, accounts, telemetry, online model downloads.
- Copying competitor internals, names, layouts or presets (LICENSING_RESEARCH §9).
- Reading or writing proprietary project formats (FLP, NewTime) without legal sign-off.
- Training any model on user audio or Suno outputs (Suno ToS item 10, QA_RESEARCH F19).
- MVP exclusions specifically: DECISION_SUMMARY §4.

## 6. Assumptions and open questions

| # | Item | Type | Owner / resolution |
|---|---|---|---|
| A1 | Suno is the dominant AI source (Udio downloads disabled since 2025-10) | Assumption (Med) | Market watch |
| A2 | Typical corrections are within ±3–5 % local ratio | Assumption | Measure on the annotated Suno corpus (BENCHMARK_CORPUS.md) |
| A3 | Natural (N_c = 8) is the right default | Assumption | SUBJECTIVE_LISTENING_TESTS.md |
| A4 | Users accept a desktop install over a web tool for privacy/quality | Assumption | 5–8 user interviews (USER_PERSONAS.md) |
| Q1 | Business model (free / one-time / subscription). Sets the JUCE tier and whether a Rubber Band licence pays off | Open | Owner (DECISION_SUMMARY §8) |
| Q2 | Product name "BeatFix" is **not cleared** | Open, **for legal review** | Trademark search class 9/42 |
| Q3 | Beat This! training-data caveat for commercial inference | Open, **for legal review** | LICENSING_RESEARCH §10 |
| Q4 | Does Suno's Manual BPM change the downloaded audio or only Studio playback? | Open | Hands-on test (COMPETITOR_RESEARCH OQ1) |
