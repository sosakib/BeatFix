# Decision Summary

**The single most useful document for an implementation agent.** Every decision below is final for the MVP unless marked *Open*. Rationale and rejected alternatives are in the linked ADR and research doc. If another doc contradicts this one, **this one wins**. Flag the conflict in OPEN_QUESTIONS.md.

Status: 2026-10-08, v1.0 (post-synthesis, post-critique).

---

## 1. Stack at a glance

| Area | Decision | Confidence | ADR | Fallback |
|---|---|---|---|---|
| Language | **C++20** (no C++23 features; `std::expected` is not used. Errors are `Result<T>` = small struct `{T value; Error err;}`) | High | ADR-001 | — |
| GUI + audio device + basic formats | **JUCE 9.0.x** (pinned exact tag), commercial licence (Starter tier, which is free below US$20k revenue, then Indie). **Never use the AGPL option.** | Med-High | ADR-001 | Tauri 2 shell over the same C API (engine has no JUCE types) |
| Engine core (`bf_dsp`, `bf_analysis`) | Plain C++20 + std only, **no JUCE types**, with a thin C-compatible API | High | ADR-001 | — |
| Build | CMake ≥ 3.28 + FetchContent (pinned tags + SHA256). vcpkg only for `mp3lame` | High | ADR-013 | vcpkg manifest for all |
| Beat/downbeat activations | **Beat This!** (MIT code + weights), ONNX export, on **ONNX Runtime 1.30 CPU EP** (telemetry disabled at startup). Full model by default. Switch to the small model if the Phase 3 perf gate fails | Med-High | ADR-003 | Built-in DSP tracker (HPSS + SuperFlux → same decoder) |
| Beat decoding / post-processing | **Owned code**: DP decoder with a ±40 % tempo corridor around the chosen metrical level, missing/extra-beat repair, sub-frame refinement + global offset, confidence | High | ADR-003 | — |
| Tempo estimation | Robust (Huber IRLS) line fit + Theil–Sen cross-check. Whittaker IRLS local tempo curve | High | ADR-004 | — |
| Octave (half/double) | Ranked candidates ×{1/3,1/2,2/3,1,3/2,2,3} in 40–250 BPM. Green/Amber/Red margin. The user is asked when Amber/Red | Med | ADR-004 | — |
| Warp model | Piecewise-linear monotone W from Whittaker/IRLS-smoothed anchors. **Natural ↔ Strict** = cutoff N_c in beats (default **Natural, N_c = 8**) | High (math) / Med (default value) | ADR-005 | PCHIP if ratio steps are audible |
| Warp-map format | JSON, derived from persisted inputs, `schemaVersion` + `algoVersion` + `inputsHash` | High | ADR-005 | — |
| Time-stretch (default "Smooth") | **Signalsmith Stretch 1.4.0** (MIT), driven by W⁻¹ on a fixed output hop, linked-stereo | Med-High (quality unconfirmed by listening) | ADR-006 | Bungee (MPL-2.0, unmodified) |
| Time-stretch "Resample" | Varispeed via r8brain. Allowed only on constant-ratio material. Pitch shift shown in cents | High | ADR-006 | — |
| Transient mode | **Phase 1.5**: HPSS split, harmonic → Smooth, percussive → short OLA, same W⁻¹ | Med | ADR-006 | — |
| Slice / Texture / region-switching Auto | **Not in MVP** (patent flag on transient copy, US 9,230,558 B2 until 2029) | — | ADR-006 | — |
| Quality reference engine | Rubber Band R3 (`setKeyFrameMap`), **dev/test builds only, never distributed** (GPL) | High | ADR-006 | Commercial licence later if listening tests justify it |
| Resampler | **r8brain-free-src** (MIT), wrapped with an exact-length/zero-delay contract | High | ADR-009 | Own polyphase SRC |
| Decode | JUCE readers for WAV/AIFF/FLAC/Ogg Vorbis. **minimp3 (`minimp3_ex`, CC0)** for MP3 with LAME/Xing gapless trim. Phase 1 gate: click-track test, 0-sample offset | Med | ADR-009 | libmpg123 (LGPL, DLL) with gapless on |
| Encode | WAV 16/24/32f (JUCE). FLAC 16/24 (JUCE/libFLAC). **MP3 via LAME (LGPL) as a separate DLL**, with LAME tag written | High | ADR-009 | — |
| Loudness / true peak | **libebur128** (MIT): integrated LUFS, true peak 4× | High | ADR-009 | — |
| Internal precision | Sample buffers **float32**. Accumulators and **all time/position values float64 or int64** | High | ADR-002 | — |
| Sample rate | Process at the **native source rate**. At most one SRC, only at export, only when asked or forced by the codec | High | ADR-002 | — |
| Project format | `.beatfix` JSON, `schemaVersion`, atomic save (temp + `ReplaceFileW`), autosave + lock file | High | ADR-010 | — |
| Threading | UI · RT audio · import · analysis · peaks · render pool · export. SPSC queues + atomics to RT. `std::stop_token` cancellation | High | ADR-011 | — |
| Rendering | Chunked parallel render cut at quiet beat boundaries (8–16 beats) with pre-roll + crossfade. Deterministic regardless of core count. Content-addressed chunk cache | Med | ADR-007 | Single-pass render (also the null-test reference) |
| Waveform | Min/max/RMS peak pyramid. Corrected view first drawn by **mapping the source pyramid through W** (instant), then replaced by real peaks of the render | High | ADR-008 | — |
| Tests | Catch2 (C++) unit/DSP tests, synthetic signals, golden files with tolerances. Python + `mir_eval` benchmark harness (dev-only, not shipped) | High | ADR-012 | — |
| Packaging | Inno Setup per-user installer + portable zip, both code-signed. The app makes **no network calls** | Med | ADR-013 | MSIX (Phase 2) |

## 2. Plain-language answers (brief §45)

- **Language:** C++20.
- **GUI framework:** JUCE 9 (custom-drawn components). The engine core is framework-free, so the shell could be replaced.
- **Audio framework:** JUCE `AudioDeviceManager` (WASAPI) for playback, plus our own lock-free transport.
- **Analysis libraries:** Beat This! ONNX model on ONNX Runtime CPU for activations. Everything after that (decoding, repair, tempo, stability, confidence) is our own deterministic C++. The pure-DSP tracker is the fallback and the cross-check.
- **Time-stretch approach:** one phase-vocoder engine (Signalsmith Stretch) driven sample-accurately by the inverse warp map. Constant-ratio "Resample" is an option. A transient-aware hybrid comes in Phase 1.5. No per-region engine switching in the MVP.
- **Resampler:** r8brain-free-src, export-only.
- **Codec approach:** decode once (gapless-correct), cache float PCM, render from that every time, encode once at export, then re-decode and validate.
- **Project format:** human-readable JSON storing *inputs* (analysis, edits, settings). The warp map and renders are derived and cached.
- **Threading model:** the realtime thread never allocates, locks, logs or does I/O. All heavy work runs on cancellable workers.
- **Waveform strategy:** a multi-resolution peak pyramid, built once per source. The corrected waveform is previewed instantly through W.
- **Testing approach:** synthetic ground truth first (exact BPM, ramps, jitter, impulse trains), then corpus benchmarks with mir_eval, then blind listening tests as the quality gate.

## 3. Known compromises

1. **JUCE licence cost** above $20k revenue, accepted for the lowest-risk audio path. The UI takes longer to build than a web UI.
2. **One stretch engine in the MVP.** Drums in dense mixes may smear slightly more than with a transient-aware hybrid (Phase 1.5).
3. **Natural (N_c = 8) default is an assumption** until listening tests on Suno material confirm it.
4. **ONNX inference is not bit-exact across machines**, so activations are persisted in the project. Re-analysis is an explicit user action.
5. **Confidence thresholds** start as placeholders and are calibrated on the corpus before release.
6. **Single target BPM per song.** Intentional tempo changes are detected and flagged, and the user chooses "flatten" or "keep section untouched (r = 1)". Per-section target tempi are post-MVP.
7. **No AAC/M4A/Opus** import or export in the MVP (patent/licensing review, and Suno delivers MP3/WAV).

## 4. NOT in the MVP

Multi-track/stems, per-section target BPM, manual warp-marker dragging (beat-level edits only: ×2/÷2, downbeat shift, accept/reject change points), groove templates/swing, Slice/Texture modes, region Auto mode, enhancement (denoise/de-clip/bandwidth extension), AAC/Opus/AIFF/Ogg export, batch/CLI, MIDI/tempo-map export, plugin hosting, macOS/Linux, cloud anything, telemetry.

## 5. MVP scope (one sentence)
Windows desktop: import one stereo/mono WAV/AIFF/FLAC/MP3/Ogg file, analyse it, show BPM + confidence + stability, pick a target BPM (one-click suggestion), render a pitch-preserving corrected version (Natural↔Strict), show original vs corrected waveforms with correction heat, A/B preview, export WAV/FLAC/MP3 with a peak gate and validation.

## 6. Cross-document conflict resolutions (synthesis)

| Conflict | Sources | Resolution | Why |
|---|---|---|---|
| A/B crossfade length 10 ms vs 20 ms | ARCHITECTURE_RESEARCH vs PLAYBACK_AND_AB | **20 ms equal-power** | The UI spec owns behaviour. 20 ms avoids clicks on bass-heavy material |
| Own polyphase SRC vs library | RESAMPLING (orig.) vs LIBRARY_EVALUATION | **r8brain** + contract tests | Reuse beats writing. Own SRC stays as the fallback |
| Decode via dr_libs vs JUCE readers | LIBRARY_EVALUATION vs ARCHITECTURE_RESEARCH | **JUCE readers** (WAV/AIFF/FLAC/Ogg) + **minimp3_ex** (MP3) | JUCE is already a dependency. MP3 needs explicit gapless handling |
| Correction colour from `|log2 r|` vs `|log2 r/g|` | WARP_MAP vs WAVEFORM_VIEW | **`|log2(r/g)|`** | A deliberate global tempo change must not paint the whole song |
| Natural↔Strict detents | WARP_FUNCTION vs MAIN_WINDOW | **6 detents**: Global, 32, 16, **8 (default)**, 4, Strict | — |
| Candidate tempo range 50–220 vs 40–250 | BPM_ESTIMATION vs BEAT_TRACKING_RESEARCH | **40–250**, ×{1/3…3} | Avoids forced octave errors on slow songs |
| Import formats in MVP | AUDIO_QUALITY_RESEARCH (incl. AAC/Opus) vs brief MVP | **WAV/RF64, AIFF, FLAC, MP3, Ogg Vorbis**. AAC/M4A/Opus post-MVP | AAC licensing review. Suno delivers MP3/WAV |
| Export defaults | AUDIO_QUALITY_RESEARCH | WAV **24-bit** (TPDF dither on), FLAC 24, MP3 **CBR 320** with LAME tag. Float WAV is never dithered | — |
| Peak handling | AUDIO_QUALITY_RESEARCH | Post-render **peak gate**: 0 dBFS (integer PCM) / −1 dBTP (lossy). The user chooses: lower gain (default) / opt-in TP limiter / export anyway (clip count reported). **Nothing silent** | TSM measured +1.25 dBFS overs on a −0.1 dBFS master |
| Performance targets | PERFORMANCE_BENCHMARKS | PROVISIONAL: analysis ≤ 0.15× RT, Smooth render ≤ 0.25× RT, preview ≤ 500 ms, ≤ 1.0 GB for 10 min @ 48 kHz, cancel ≤ 250 ms, on a 4C/8T AVX2 16 GB laptop | Calibrated in the Phase 0 spike |
| Analysis determinism | BEAT_TRACKING_RESEARCH, DSP_TEST_STRATEGY | Persist activations + model hash. Determinism levels D1/D2/D3 per DSP_TEST_STRATEGY | ORT CPU is not bit-exact across machines |
| Three ratio thresholds (8 % / 12 % / 15 %) | WARP_FUNCTION vs WAVEFORM_VIEW vs UX | They measure **different things**. Per-segment local deviation `|r/g − 1|` ≥ 8 % → quality warning. ≥ 12 % → "extreme": likely a missed or extra beat. Global change `|g − 1|` ≥ 15 % → large-overall-change warning. Hard bound r ∉ [0.5, 2] → render refused, half/double fix offered | — |
| Calibration models fitted on Suno tracks | RESEARCH_SUMMARY | Fit confidence/octave calibration on **synthetic, MIDI-rendered and CC material only**. Using Suno outputs for fitting is **for legal review** (Suno ToS item 10) | Licence risk |
| Intentional tempo change default | TEMPO_STABILITY_ANALYSIS | MVP: **flag + ask**. Options: "flatten to target" or "keep that section unwarped (r = 1)". Never auto-smear | Protects intentional music |

## 7. Final documentation map (consolidated)
Brief §30 was consolidated where files would have been thin. Merges:
- `02_requirements/SOFTWARE_SYSTEM_REQUIREMENTS.md` contains functional, non-functional, performance and audio-fidelity requirements (FUNCTIONAL/NON_FUNCTIONAL/PERFORMANCE/AUDIO_FIDELITY_REQUIREMENTS are sections of it, not separate files).
- `01_product/USER_PERSONAS.md` also contains the user journeys.
- `04_algorithms/HYBRID_AUTO_ENGINE.md` documents the *future* Auto design. MVP has no region switching.
- `05_architecture/SYSTEM_ARCHITECTURE.md` includes modules, interfaces and data flow. `RENDER_PIPELINE.md` includes the cache strategy. `AUDIO_PIPELINE.md` includes file/data integrity (brief §38).
- `06_ui/MAIN_WINDOW.md` includes the analysis view and stretch-mode UI. `WAVEFORM_VIEW.md` includes the tempo view, warp-map view and visual difference model.
- `07_quality/QA_STRATEGY.md` includes the unit test strategy. `DSP_TEST_STRATEGY.md` includes the golden audio tests.
- `08_engineering/BUILD_AND_DEPENDENCIES.md` = build system + dependency management. Observability lives in `05_architecture/LOGGING_AND_DIAGNOSTICS.md`.
- `10_implementation/IMPLEMENTATION_ROADMAP.md` = roadmap + phase plan + task breakdown.

## 8. Pending decisions for the owner (do not block Phase 0–2)
See OPEN_QUESTIONS.md. Top items: the business model, which sets the JUCE tier and whether a paid Rubber Band licence makes sense; legal review of the Beat This! training-data caveat; the product name/trademark ("BeatFix" is not cleared); the code-signing entity/country.
