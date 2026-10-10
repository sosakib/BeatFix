# BeatFix: Planning Package (master map)

**BeatFix** (working name, trademark not cleared) is a Windows desktop tool that fixes the tempo of a finished song, especially AI-generated music (Suno). It analyses the whole song, finds the beats, measures where the tempo drifts, and lets the user pick a fixed BPM. It then time-warps the audio onto that grid **without changing pitch**, shows original vs corrected waveforms with a correction heatmap, offers A/B preview, and exports WAV/FLAC/MP3, always rendered once from the untouched source.

Status: planning complete, v1.0, 2026-10-10. **No code yet.** Start at Phase 0.

---

## 1. How to use this package (implementation agents read this first)
1. Read **[10_implementation/DECISION_SUMMARY.md](10_implementation/DECISION_SUMMARY.md)**. It is authoritative, and wins every conflict.
2. Read **[10_implementation/IMPLEMENTATION_ROADMAP.md](10_implementation/IMPLEMENTATION_ROADMAP.md)** and work one phase at a time. Each phase lists objective, modules, tasks, tests and exit gates.
3. For each task, open the requirement in **[02_requirements/SOFTWARE_SYSTEM_REQUIREMENTS.md](02_requirements/SOFTWARE_SYSTEM_REQUIREMENTS.md)** (REQ-IDs, acceptance criteria, verification method) and the design doc it traces to (SRS §23).
4. Algorithms: equations, pseudocode and test cases are in `04_algorithms/`. Implement exactly that, and test exactly that.
5. A task is done only per **[DEFINITION_OF_DONE.md](10_implementation/DEFINITION_OF_DONE.md)**.
6. Hit an unknown? Check **[OPEN_QUESTIONS.md](10_implementation/OPEN_QUESTIONS.md)**: each item has a proposed default. If it is missing, add it there rather than guessing silently.
7. `03_research/` is evidence, not instructions. Some research docs predate the final decisions.

## 2. Architecture overview
```
┌──────────── app (JUCE 9 GUI) ────────────┐   future: cli (batch)
│ main window · lanes · transport · export │
└───────────────┬──────────────────────────┘
                │ plain C++ API (no JUCE types below this line)
┌───────────────▼──────────────────────────────────────────────────┐
│ bf_engine  project/session · edit state · render scheduler +     │
│            chunk cache · playback engine (lock-free RT boundary) │
├──────────────┬──────────────────────┬────────────────────────────┤
│ bf_analysis  │ bf_dsp               │ bf_io                      │
│ Beat This!   │ grid · warp · tempo  │ decode (JUCE, minimp3)     │
│ (ONNX RT)    │ stats · smoother ·   │ encode (JUCE, LAME DLL)    │
│ DP decoder · │ ITimeStretcher       │ r8brain SRC · libebur128   │
│ repair ·     │ (Signalsmith; R3 dev)│ PCM cache · tags           │
│ IBeatAnalyzer│                      │                            │
└──────────────┴──────────────────────┴────────────────────────────┘
```
Details: [05_architecture/SYSTEM_ARCHITECTURE.md](05_architecture/SYSTEM_ARCHITECTURE.md), [THREADING_MODEL.md](05_architecture/THREADING_MODEL.md), [AUDIO_PIPELINE.md](05_architecture/AUDIO_PIPELINE.md), [RENDER_PIPELINE.md](05_architecture/RENDER_PIPELINE.md).

## 3. Key decisions (one line each)

| Area | Decision |
|---|---|
| Language / GUI | C++20 · JUCE 9 (commercial tier, never AGPL) · framework-free core |
| Beat analysis | Beat This! ONNX activations (MIT) on ONNX Runtime CPU + owned DP decoder; DSP fallback |
| Tempo | Huber-IRLS line fit; Whittaker local curve; ranked octave candidates ×{1/3…3}, 40–250 BPM |
| Warp | monotone piecewise-linear W from smoothed anchors; **Natural ↔ Strict** = cutoff N_c beats (default 8) |
| Stretch | Signalsmith Stretch "Smooth" (MIT) driven by W⁻¹; "Resample" for constant ratios; R3 as a dev-only reference |
| Render | deterministic chunk-parallel, content-addressed cache, playhead-first preview |
| Audio fidelity | immutable source; decode once (gapless); float32 audio / float64 time; native SR; ≤ 1 SRC; encode once; peak gate; validate |
| Codecs | JUCE readers + minimp3_ex; WAV/FLAC via JUCE; MP3 via LAME DLL; r8brain; libebur128 |
| Project | `.beatfix` JSON of inputs; atomic save; autosave; relink by hash |
| Distribution | CMake FetchContent; GitHub Actions; Inno Setup + portable zip; no network calls |

Full table with fallbacks: [DECISION_SUMMARY.md](10_implementation/DECISION_SUMMARY.md) · ADRs: [09_decisions/](09_decisions/README.md) · libraries and licences: [08_engineering/TECH_STACK.md](08_engineering/TECH_STACK.md).

## 4. Algorithm pipeline
decode (gapless) → analysis branch (22.05 kHz mono) → onset features → Beat This activations → tempo candidates + margin → DP beat decode within a tempo corridor → missing/extra repair → downbeats → sub-frame refinement → confidence → tempo + stability (drift vs noise, steps, splices) → **target grid** → **warp map** (Natural↔Strict smoother, validation) → Smooth stretch via W⁻¹ (chunked) → validation (Path K / Path R, peaks, stereo) → export (peak gate → SRC → dither → encode → validate). Full diagram, pseudocode and answers to the 18 algorithm questions: **[04_algorithms/ALGORITHM_OVERVIEW.md](04_algorithms/ALGORITHM_OVERVIEW.md)**.

## 5. MVP boundary
**In:** Windows 10/11 x64 · one mono/stereo file · import WAV/AIFF/FLAC/MP3/Ogg · full analysis · BPM + confidence + stability · candidate picker · target BPM · Natural↔Strict · Smooth (+ Resample) · original/corrected lanes with heatmap · A/B + click · undo · project save/autosave · export WAV/FLAC/MP3 with peak gate and validation.
**Out:** stems, per-section target BPM, manual warp-marker dragging, Transient/Slice/Texture/Auto modes, enhancement, AAC/Opus, batch/CLI, MIDI tempo export, macOS/Linux, telemetry. See [MVP_DEFINITION.md](10_implementation/MVP_DEFINITION.md), [PRODUCT_SCOPE.md](01_product/PRODUCT_SCOPE.md).

## 6. Roadmap (1 experienced C++ developer)
| Phase | Content | Weeks |
|---|---|---|
| 0 | De-risking spikes (Signalsmith alignment, gapless MP3, ONNX timing, r8brain, R3 references, Suno file probe) + skeleton | 3–4 |
| 1 | Import, decode, source playback, waveform | 3–4 |
| 2 | Grid, warp and tempo maths (pure, fully unit-tested) | 2 |
| 3 | Beat and tempo analysis | 6–8 |
| 4 | Render engine (Smooth + Resample) | 4–5 |
| 5 | Export and quality protection | 2–3 |
| 6 | Corrected view and A/B | 3–4 |
| 7 | Workflow UI, project/session, export dialog | 5–6 |
| 8 | Calibration, listening, hardening, release | 5–7 |
| | **MVP total** | **33–43 (40–52 with contingency)** |
| 1.5 | Transient mode (post-MVP, after legal review) | 4–6 |

## 7. Critical risks (top 6; full list in [RISK_REGISTER.md](10_implementation/RISK_REGISTER.md))
1. **Beat-tracking continuity** errors cause 2×/½ local stretches (R-01).
2. **Signalsmith quality** on dense AI masters may trail Rubber Band R3 (R-02). Fallbacks: Bungee, or a Rubber Band commercial licence.
3. **Suno Studio** adds native tempo locking and warp markers (R-20). Differentiate on offline processing, confidence, groove and fidelity.
4. **Legal:** JUCE tier, Beat This! training-data caveat, Fraunhofer transient patent, Suno ToS on test data (R-30…R-37).
5. **Calibration data** for AI-music drift is anecdotal until a ≥ 50-song annotated corpus exists (R-22).
6. **Schedule:** calibration and listening dominate (R-40). Single developer (R-41).

## 8. Top unresolved questions
Owner: business model (JUCE tier, Rubber Band licence), legal entity for code signing, maximum song duration. Legal: L-1…L-12. Measurements answered by Phase 0: M-1…M-7. See [OPEN_QUESTIONS.md](10_implementation/OPEN_QUESTIONS.md).

## 9. Document index

| Folder | Documents |
|---|---|
| **01_product** | [PRODUCT_VISION](01_product/PRODUCT_VISION.md) · [PRODUCT_SCOPE](01_product/PRODUCT_SCOPE.md) · [USER_PERSONAS](01_product/USER_PERSONAS.md) (incl. journeys) · [FEATURE_SPECIFICATION](01_product/FEATURE_SPECIFICATION.md) · [COMPETITIVE_ANALYSIS](01_product/COMPETITIVE_ANALYSIS.md) |
| **02_requirements** | [SOFTWARE_SYSTEM_REQUIREMENTS](02_requirements/SOFTWARE_SYSTEM_REQUIREMENTS.md) (functional, non-functional, performance, audio fidelity) · [SECURITY_AND_PRIVACY](02_requirements/SECURITY_AND_PRIVACY.md) · [COMPATIBILITY_MATRIX](02_requirements/COMPATIBILITY_MATRIX.md) |
| **03_research** | [RESEARCH_SUMMARY](03_research/RESEARCH_SUMMARY.md) · [TIME_STRETCH](03_research/TIME_STRETCH_RESEARCH.md) · [DSP](03_research/DSP_RESEARCH.md) · [BEAT_TRACKING](03_research/BEAT_TRACKING_RESEARCH.md) · [TEMPO_ESTIMATION](03_research/TEMPO_ESTIMATION_RESEARCH.md) · [AUDIO_QUALITY](03_research/AUDIO_QUALITY_RESEARCH.md) (+ `_evidence/` measurements) · [LIBRARY_EVALUATION](03_research/LIBRARY_EVALUATION.md) · [LICENSING](03_research/LICENSING_RESEARCH.md) · [COMPETITOR](03_research/COMPETITOR_RESEARCH.md) · [UI_UX](03_research/UI_UX_RESEARCH.md) · [QA](03_research/QA_RESEARCH.md) · [ARCHITECTURE](03_research/ARCHITECTURE_RESEARCH.md) · [REFERENCES](03_research/REFERENCES.md) · [baseline log](03_research/_baseline_research_log.md) |
| **04_algorithms** | [ALGORITHM_OVERVIEW](04_algorithms/ALGORITHM_OVERVIEW.md) · [ONSET_DETECTION](04_algorithms/ONSET_DETECTION.md) · [BEAT_TRACKING](04_algorithms/BEAT_TRACKING.md) · [DOWNBEAT_ESTIMATION](04_algorithms/DOWNBEAT_ESTIMATION.md) · [BPM_ESTIMATION](04_algorithms/BPM_ESTIMATION.md) · [TEMPO_STABILITY_ANALYSIS](04_algorithms/TEMPO_STABILITY_ANALYSIS.md) · [OUTLIER_REJECTION](04_algorithms/OUTLIER_REJECTION.md) · [HALF_DOUBLE_TIME_RESOLUTION](04_algorithms/HALF_DOUBLE_TIME_RESOLUTION.md) · [TARGET_BEAT_GRID](04_algorithms/TARGET_BEAT_GRID.md) · [WARP_FUNCTION](04_algorithms/WARP_FUNCTION.md) · [WARP_MAP](04_algorithms/WARP_MAP.md) · [TIME_STRETCH_ENGINE](04_algorithms/TIME_STRETCH_ENGINE.md) · [TRANSIENT_PRESERVATION](04_algorithms/TRANSIENT_PRESERVATION.md) · [HYBRID_AUTO_ENGINE](04_algorithms/HYBRID_AUTO_ENGINE.md) · [STEREO_PHASE_PRESERVATION](04_algorithms/STEREO_PHASE_PRESERVATION.md) · [RESAMPLING](04_algorithms/RESAMPLING.md) · [DITHERING](04_algorithms/DITHERING.md) · [QUALITY_VALIDATION](04_algorithms/QUALITY_VALIDATION.md) |
| **05_architecture** | [SYSTEM_ARCHITECTURE](05_architecture/SYSTEM_ARCHITECTURE.md) (modules, interfaces, data flow) · [THREADING_MODEL](05_architecture/THREADING_MODEL.md) · [AUDIO_PIPELINE](05_architecture/AUDIO_PIPELINE.md) (incl. file integrity) · [RENDER_PIPELINE](05_architecture/RENDER_PIPELINE.md) (incl. cache) · [PROJECT_SESSION_MODEL](05_architecture/PROJECT_SESSION_MODEL.md) · [ERROR_HANDLING](05_architecture/ERROR_HANDLING.md) · [LOGGING_AND_DIAGNOSTICS](05_architecture/LOGGING_AND_DIAGNOSTICS.md) (incl. observability) |
| **06_ui** | [UI_UX_PRINCIPLES](06_ui/UI_UX_PRINCIPLES.md) · [MAIN_WINDOW](06_ui/MAIN_WINDOW.md) (incl. analysis view, stretch-mode UI) · [WAVEFORM_VIEW](06_ui/WAVEFORM_VIEW.md) (incl. tempo view, warp-map view, difference model) · [PLAYBACK_AND_AB](06_ui/PLAYBACK_AND_AB.md) · [EXPORT_UI](06_ui/EXPORT_UI.md) · [KEYBOARD_SHORTCUTS](06_ui/KEYBOARD_SHORTCUTS.md) · [UX_BEHAVIOR](06_ui/UX_BEHAVIOR.md) |
| **07_quality** | [QA_STRATEGY](07_quality/QA_STRATEGY.md) (incl. unit tests) · [DSP_TEST_STRATEGY](07_quality/DSP_TEST_STRATEGY.md) (incl. golden tests) · [METRICS](07_quality/METRICS.md) · [BENCHMARK_CORPUS](07_quality/BENCHMARK_CORPUS.md) · [PERFORMANCE_BENCHMARKS](07_quality/PERFORMANCE_BENCHMARKS.md) · [SUBJECTIVE_LISTENING_TESTS](07_quality/SUBJECTIVE_LISTENING_TESTS.md) · [ACCEPTANCE_CRITERIA](07_quality/ACCEPTANCE_CRITERIA.md) |
| **08_engineering** | [TECH_STACK](08_engineering/TECH_STACK.md) · [REPOSITORY_STRUCTURE](08_engineering/REPOSITORY_STRUCTURE.md) · [BUILD_AND_DEPENDENCIES](08_engineering/BUILD_AND_DEPENDENCIES.md) · [CODING_STANDARDS](08_engineering/CODING_STANDARDS.md) · [CI_CD](08_engineering/CI_CD.md) · [RELEASE_STRATEGY](08_engineering/RELEASE_STRATEGY.md) |
| **09_decisions** | [ADR index](09_decisions/README.md): ADR-001 … ADR-013 |
| **10_implementation** | [DECISION_SUMMARY](10_implementation/DECISION_SUMMARY.md) · [IMPLEMENTATION_ROADMAP](10_implementation/IMPLEMENTATION_ROADMAP.md) (incl. phase plan, task breakdown) · [MVP_DEFINITION](10_implementation/MVP_DEFINITION.md) · [DEFINITION_OF_DONE](10_implementation/DEFINITION_OF_DONE.md) · [RISK_REGISTER](10_implementation/RISK_REGISTER.md) · [OPEN_QUESTIONS](10_implementation/OPEN_QUESTIONS.md) · [SELF_CRITIQUE](10_implementation/SELF_CRITIQUE.md) |

Brief §30 was consolidated where separate files would have been thin. The merges are listed in DECISION_SUMMARY §7.
