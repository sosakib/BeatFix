# MVP Definition

Status: 2026-10-08, v1.0. Source of truth: [DECISION_SUMMARY.md §4–5](DECISION_SUMMARY.md). This file expands it into a checkable in/out list and release criteria. If the two disagree, DECISION_SUMMARY wins.

**One sentence (DECISION_SUMMARY §5).** Windows desktop: import one stereo/mono WAV/AIFF/FLAC/MP3/Ogg file, analyse it, show BPM + confidence + stability, pick a target BPM (one-click suggestion), render a pitch-preserving corrected version (Natural↔Strict), show original vs corrected waveforms with correction heat, A/B preview, export WAV/FLAC/MP3 with a peak gate and validation.

**Why this cut.** The core job is "remove drift without new audible damage, and show what changed". Everything in the MVP serves one of three things: correctness of the beat map, fidelity of the render, or the user's ability to verify both. Everything left out either multiplies the test surface (stems, multiple modes, per-section tempi), carries unresolved legal risk (AAC, transient-copy modes), or serves a minority persona (P4 in [USER_PERSONAS.md](../01_product/USER_PERSONAS.md)).

## 1. IN the MVP

| Area | Included | Spec |
|---|---|---|
| Platform | Windows 10/11 x64 desktop, per-user install + portable zip, code-signed, no network calls | DECISION_SUMMARY §1 |
| Input | One file, mono or stereo (> 2 channels rejected with a message). WAV/RF64, AIFF, FLAC, MP3 (gapless-trimmed), Ogg Vorbis. Native sample rate kept | DECISION_SUMMARY §1, §6 |
| Source safety | Source never written; decoded once to a float32 PCM cache; file + PCM hashes | [AUDIO_QUALITY_RESEARCH.md §4.10](../03_research/AUDIO_QUALITY_RESEARCH.md) |
| Analysis | Beat This! ONNX (full model; small if the Phase 3 perf gate fails) + own DP decoder, repair, sub-frame refinement; DSP fallback and cross-check; downbeats; persisted activations | [BEAT_TRACKING_RESEARCH.md §12](../03_research/BEAT_TRACKING_RESEARCH.md) |
| Tempo | Global BPM (fractional), local tempo curve, candidates ×{1/3…3} in 40–250 BPM, Green/Amber/Red octave margin with user prompt on Amber/Red, stability class, intentional-change detection | [BPM_ESTIMATION.md](../04_algorithms/BPM_ESTIMATION.md), [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) |
| Beat-level edits | ×2 / ÷2, choose ranked alternate, downbeat shift, accept/reject change points, "flatten" vs "keep section unwarped (r = 1)" for intentional changes | DECISION_SUMMARY §3 item 6, §4 |
| Target | Single target BPM per song, default `round(detected)`, one-click suggestion | DECISION_SUMMARY §3 item 6 |
| Warp | Piecewise-linear monotone W; Natural↔Strict with 6 detents (Global, 32, 16, **8 default**, 4, Strict); ratio warn/hard bounds; unreliable-analysis gate | [WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md), [WARP_MAP.md](../04_algorithms/WARP_MAP.md) |
| Stretch modes | **Smooth** (Signalsmith, default) and **Resample** (varispeed, constant-ratio only, cents shown). Nothing else | DECISION_SUMMARY §1 |
| Render | Chunked parallel render, deterministic, content-addressed chunk cache; single-pass reference/fallback | DECISION_SUMMARY §1 |
| View | Original and corrected lanes on a shared musical timeline, correction heat `|log2(r/g)|`, beat/downbeat/grid markers, hover card, overview, collapsible tempo view, region list | [WAVEFORM_VIEW.md](../06_ui/WAVEFORM_VIEW.md) |
| Playback | A/B at the same musical position, 20 ms equal-power crossfade, loop/selection, metronome overlay, progressive preview | [PLAYBACK_AND_AB.md](../06_ui/PLAYBACK_AND_AB.md) |
| Export | WAV 16/24/32f (24 default, TPDF on int), FLAC 16/24, MP3 via LAME DLL (CBR 320 default, LAME tag). SRC only on request or when forced. Peak gate (lower gain default / opt-in TP limiter / export anyway with count). Re-decode validation. Metadata carry-over + BPM tag | DECISION_SUMMARY §6, [AUDIO_QUALITY_RESEARCH.md §4.7, §4.11](../03_research/AUDIO_QUALITY_RESEARCH.md) |
| Project | `.beatfix` JSON (inputs only), atomic save, autosave + crash recovery, source relocation by hash, undo/redo | DECISION_SUMMARY §1, ARCHITECTURE_RESEARCH §8 |
| Accessibility | Keyboard-only operation, UIA labels, light/dark/high-contrast | [UI_UX_PRINCIPLES.md §3](../06_ui/UI_UX_PRINCIPLES.md), [KEYBOARD_SHORTCUTS.md](../06_ui/KEYBOARD_SHORTCUTS.md) |
| Privacy | Local logs without paths/audio, local crash dumps, ORT telemetry disabled, no update check | ARCHITECTURE_RESEARCH §9 |

## 2. NOT in the MVP

Classification uses brief §34: **Phase 1.5** (next release, planned), **Release 2** (planned post-MVP), **Experimental** (needs research or a listening result first), **Future** (no plan).

| Item | Class | Why excluded |
|---|---|---|
| Transient mode (HPSS hybrid) | Phase 1.5 | Needs a listening win on drums and legal review vs the Fraunhofer family (TIME_STRETCH_RESEARCH rec. 4, F10) |
| Microsoft Store / MSIX | Release 2 | Not needed for first users; changes cache paths (ARCHITECTURE_RESEARCH §14) |
| Manual warp-marker dragging | Release 2 | Beat-level edits cover the documented failure modes; marker editing multiplies UI and undo surface |
| Per-section target BPM | Release 2 | Single target + "keep section r = 1" covers intentional changes (DECISION_SUMMARY §3 item 6) |
| AAC/M4A/Opus import; AIFF/Ogg/AAC/Opus export | Release 2 | AAC patent pool and MF licence unresolved (L-9); Suno delivers MP3/WAV |
| Match-source-loudness default on, loudness-matched/blind A/B | Release 2 | Needs user testing (AUDIO_QUALITY_RESEARCH OQ6, PLAYBACK_AND_AB §3) |
| Passthrough (bit-exact r ≈ 1 regions) | Experimental | Seam audibility unmeasured (TIME_STRETCH OQ5, M-4) |
| Rubber Band R3 as shipped premium engine | Experimental | Only if MUSHRA shows a gap and the commercial licence is bought (LICENSING_RESEARCH rec. 5) |
| Region-switching Auto, per-bin fuzzy hybrid | Experimental | No evidence it is needed; seams risk (TIME_STRETCH rec. 7) |
| Unusual meters beyond 3/4, 4/4 | Experimental | Decoder supports {3, 4}; others flagged unsupported (O-10) |
| Multi-track / stems, linked group stretch | Future | Different product scope; inter-mic phase constraints |
| Slice / Texture modes | Future | Patent flag on transient copy until 2029; not needed for beat correction |
| Groove templates / swing | Future | Conflicts with "keep groove" default |
| Enhancement (denoise, de-clip, bandwidth extension) | Future | Separate opt-in stage; model licences unresolved (AUDIO_QUALITY_RESEARCH §4.13) |
| Batch / CLI as a product, MIDI/tempo-map export, plugin hosting | Future | The dev CLI exists for tests only and is not installed |
| macOS / Linux | Future | Engine core is portable; shell and packaging are not planned |
| Cloud, accounts, telemetry, online model download | Future (never by default) | Product principle "offline and local" ([PRODUCT_VISION.md §3.4](../01_product/PRODUCT_VISION.md)) |
| Training any model on user or Suno audio | Never | Suno ToS item 10; privacy |

**UI-doc conflicts resolved here** (details in [OPEN_QUESTIONS.md §5](OPEN_QUESTIONS.md)): the mode menu in MAIN_WINDOW / UX_BEHAVIOR / UI_UX_PRINCIPLES lists "Auto, Smooth, Transient" with default Auto. For the MVP the menu is **Smooth (default) and Resample**. The MP3 default in EXPORT_UI (VBR V0) is replaced by **CBR 320** (DECISION_SUMMARY §6).

## 3. MVP release criteria

All of these must hold on the release candidate. Metric IDs are defined in [METRICS.md](../07_quality/METRICS.md). Thresholds marked PROVISIONAL there must have been frozen in Phase 8 into `07_quality/ACCEPTANCE_CRITERIA.md`; the numbers below are the current values and the freeze may only tighten them or replace them with measured values under the [DSP_TEST_STRATEGY.md §7](../07_quality/DSP_TEST_STRATEGY.md) procedure (thresholds tighter than the reference engine achieves are escalated, not loosened silently).

### 3.1 Analysis (detector)
| Gate | Criterion |
|---|---|
| A10 anchor precision | synthetic clicks/kicks: median ≤ 2 ms, P95 ≤ 5 ms (DSP-ANA-1) |
| A11 fine tempo | ≤ 0.01 BPM synthetic 240 s; ≤ 0.05 BPM real stable items |
| A9 spurious/missing | FP ≤ 2 %, FN ≤ 2 % at σ ≤ 5 ms (DSP-ANA-3) |
| A5 CMLt | corpus-level not worse than the frozen baseline beyond the bootstrap CI ([QA_STRATEGY.md §6](../07_quality/QA_STRATEGY.md)) |
| A14 metric level | correct level in top-2 candidates on 100 % of synthetic half/double items |
| A13 calibration | confidence not overconfident: ECE reported; "High" means < 2 % octave-error rate and median |ΔBPM| < 0.1 on the stable subset ([BPM_ESTIMATION.md §7](../04_algorithms/BPM_ESTIMATION.md)) |
| Unreliable material | ambient/rubato items produce Low confidence and no auto-render (BENCHMARK_CORPUS cat. 16) |

### 3.2 Render and export (engine)
| Gate | Criterion |
|---|---|
| O1 time-map fidelity | median ≤ 1 ms, P95 ≤ 3 ms, max ≤ 5 ms at ratios 0.90–1.10 (or the Phase 0 frozen values) |
| O2 end-to-end alignment | synthetic median ≤ 3 ms / P95 ≤ 10 ms; real (source CMLt ≥ 0.9) median ≤ 10 ms / P95 ≤ 25 ms, F@70 ≥ 0.97 |
| O3 cumulative drift | ≤ 5 ms end-of-song, |slope| ≤ 0.05 ms/beat; also at 60 min (PERF-LONG-3) |
| O13 a, b | mono and polarity invariance exact (≤ 1e-7) |
| O14 peaks | no silent clipping; every over reported; gate behaviour per DECISION_SUMMARY §6 |
| O15 clicks | zero new flags on synthetic signals and at every chunk boundary |
| O16, O17 | no NaN/Inf/unexpected silence; length within 1 sample of expected |
| O18 identity | identity map residual ≤ −60 dBFS RMS and ABX-transparent |
| O19 export integrity | re-decode: lossless exact, MP3 offset 0 ± 1 sample |
| Determinism | D1 bit-exact across thread counts on one machine; D2 tolerances across machines |
| Source immutability | DSP-INT-1 passes |

### 3.3 Listening ([SUBJECTIVE_LISTENING_TESTS.md §6](../07_quality/SUBJECTIVE_LISTENING_TESTS.md))
- Smooth vs Rubber Band R3 MUSHRA: lower 95 % CI bound of (Smooth − R3) ≥ −5 points on drums, vocals, dense mix at product-typical ratios. Failure → escalation path, and release only with the owner's recorded acceptance.
- Stable-song LIS-ABX: no listener significant, pooled not significant.
- Natural (N_c = 8) default confirmed or replaced by LIS-PAIR result.

### 3.4 Performance (REF-MID, measured targets from Phase 0; current PROVISIONAL values from [PERFORMANCE_BENCHMARKS.md §4](../07_quality/PERFORMANCE_BENCHMARKS.md))
Analysis ≤ 0.15× RT (full) or ≤ 0.05× RT (small) · Smooth render ≤ 0.25× RT · preview ≤ 500 ms · peak working set ≤ 1.0 GB at 10 min / 48 kHz stereo · UI stall ≤ 100 ms hard · cancel ≤ 250 ms · 0 xruns during background work. REF-LOW ≤ 2× time targets.

### 3.5 Robustness, licence, privacy
- Nightly decoder/project fuzzing clean for the last 14 days; PERF-LONG-1…4 and PERF-CANCEL-1 pass.
- SBOM complete and transitive; no DENY licence in shipped binaries; LGPL obligations for LAME met; third-party notices in the installer and About box.
- No outbound network connection during a full session (verified by firewall log/ETW).
- Legal sign-offs recorded for L-1 (JUCE tier), L-2 (Beat This data caveat), L-4 (LGPL/EULA), L-8 (name).

### 3.6 User-visible acceptance (brief §44)
All scenarios of the acceptance matrix pass, at minimum: stable song (BPM ≈ source, minimal warp, mostly grey, transparent), slight drift 108 → 113 (drift detected, gradual correction, no seams), drum-heavy (no obvious smearing; judged by listening), vocal-heavy (pitch preserved; O7), target change rebuilds from source/project state and never from an encoded intermediate, MP3 input/output alignment, half/double ambiguity prompt, drumless intro flagged or extrapolated.

## 4. Scope-creep rule

1. **Default answer is no.** A feature not in §1 is out of the MVP until this file is changed.
2. A change to §1 needs a written entry (in OPEN_QUESTIONS or an ADR) that states: the user problem with evidence (not anecdote alone), which persona, which existing MVP item it replaces or delays, added test surface (which METRICS/QA levels), licence impact, and effort in weeks.
3. Accept only if **at least one** holds: (a) it fixes a failure that blocks a §3 release criterion; (b) it removes more work or risk than it adds; (c) the owner explicitly trades it for a §1 item of equal or larger effort.
4. Never accepted into the MVP regardless of the above: anything on a DENY-licence dependency, anything needing network access, any automatic action that changes audio without a visible flag.
5. "Small" additions count. A new mode, a new format or a new setting each adds goldens, UI states and documentation. Settings that exist only to avoid a decision are rejected (UI_UX_PRINCIPLES P1).
6. Requests that pass go to Release 2 by default, not into the current phase, unless rule 3(a) applies.
