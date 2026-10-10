# Implementation Roadmap (roadmap + phase plan + task breakdown)

Status: 2026-10-08, v1.0. Subordinate to [DECISION_SUMMARY.md](DECISION_SUMMARY.md): where this file and the decision summary disagree, the decision summary wins. Scope boundary: [MVP_DEFINITION.md](MVP_DEFINITION.md). Completion rules: [DEFINITION_OF_DONE.md](DEFINITION_OF_DONE.md). Risks and open items referenced as `R-…` / `O-…`, `L-…`, `M-…`: [RISK_REGISTER.md](RISK_REGISTER.md), [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md).

## 0. Conventions

**Phase naming.** Two numbering systems were in use across the docs. This roadmap fixes them:

| Label | Meaning | Where used |
|---|---|---|
| **Phase 0 … Phase 8** | Build phases of the MVP (this file) | DECISION_SUMMARY "Phase 1 gate" (decode click test) = Phase 1 here; "Phase 3 perf gate" (Beat This full vs small) = Phase 3 here; "calibrated in the Phase 0 spike" = Phase 0 here |
| **Phase 1.5** | First post-MVP release: Transient mode | DECISION_SUMMARY §1, TIME_STRETCH_RESEARCH rec. 4 |
| **Release 2 / Post-MVP** | Everything in DECISION_SUMMARY §4 plus MSIX/Store ("MSIX (Phase 2)") | DECISION_SUMMARY §1 Packaging row |

**Module names.** `08_engineering/REPOSITORY_STRUCTURE.md` did not exist when this was written, so files follow the layout in [ARCHITECTURE_RESEARCH.md §10](../03_research/ARCHITECTURE_RESEARCH.md) plus the test/tool folders required by [QA_STRATEGY.md](../07_quality/QA_STRATEGY.md) Implementation Implications and the licence folders from [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) Implementation Implications:

```
src/bf_dsp/       pure C++20 + std: warp math, tempo fits, stretch adapters, resample wrapper, dither, peaks, xfade
src/bf_analysis/  mel front-end, Beat This ONNX, DSP onset fallback, tempo hypotheses, decoder, confidence
src/bf_io/        decode -> PCM cache, export writers, LAME writer, atomic file helper, metadata
src/bf_engine/    session, jobs, render scheduler, cache, playback, project JSON
apps/beatfix/     JUCE GUI
apps/beatfix-cli/ internal dev/test harness (NOT shipped in MVP: DECISION_SUMMARY §4 excludes batch/CLI as a product feature)
tests/{unit,integration,golden}   tools/{gen,eval,listening,sbom,spike}   corpus/manifest/   goldens/   models/   third_party/   licenses/   installer/
```

**Requirement prefixes.** `02_requirements/SOFTWARE_SYSTEM_REQUIREMENTS.md` did not exist when this was written. Phases list the requirement *families* they satisfy, using the brief §32 scheme. Exact IDs are assigned in the SRS and must be reconciled there:
`REQ-IO` import/decode/export · `REQ-AUDIO` fidelity, precision, peak/dither/SRC · `REQ-BEAT` beats/downbeats/confidence · `REQ-TEMPO` BPM, octave, stability · `REQ-WARP` grid, warp map, Natural↔Strict · `REQ-STRETCH` time-stretch modes · `REQ-PLAY` playback/A/B · `REQ-UI` views, workflow, accessibility · `REQ-PROJ` project/session/integrity · `REQ-PERF` time/memory targets · `REQ-RT` threading/realtime safety · `REQ-PRIV` offline/privacy · `REQ-LIC` licence compliance · `REQ-QA` test/validation infrastructure.

**Why these phases and not the brief §33 example.** Derived from the research, not copied:
1. A **Phase 0 spike** comes first because six MVP decisions are only Med confidence (DECISION_SUMMARY §1: Signalsmith quality, minimp3 gapless, Beat This CPU time, chunked render, perf targets, thresholds). Each has a named fallback; finding out in Phase 4 would cost the most.
2. **Tempo/warp math is its own small phase** (Phase 2) because it is pure, testable with synthetic beat lists, and needed by both analysis (shared Whittaker/IRLS code, [WARP_FUNCTION.md §4.2](../04_algorithms/WARP_FUNCTION.md)) and rendering. It runs in parallel with Phase 1.
3. **"Analysis engine" and "BPM/beat detection" are one phase** (Phase 3): tempo hypotheses drive the DP decoder's local period path ([BEAT_TRACKING_RESEARCH.md §12](../03_research/BEAT_TRACKING_RESEARCH.md)), so they cannot be built or gated separately.
4. **Export is pulled ahead of the UI** (Phase 5) so the full CLI chain import → analyse → render → export exists before any heavy UI work. That chain is what the L3 golden tests need ([QA_STRATEGY.md](../07_quality/QA_STRATEGY.md) Recommendation; [DSP_TEST_STRATEGY.md §5](../07_quality/DSP_TEST_STRATEGY.md)).
5. **"Smart hybrid processing" is not an MVP phase.** Region-switching Auto is excluded and Transient mode is Phase 1.5 (DECISION_SUMMARY §1, TIME_STRETCH_RESEARCH rec. 4, 7).
6. **Benchmarking is not deferred to the end.** Tests and the `tools/gen` + `tools/eval` harness start in Phase 0. Phase 8 is calibration (freezing thresholds on the corpus), listening and release hardening.

## 1. Phase overview and dependencies

| Phase | Name | Depends on | Effort (weeks, 1 dev) |
|---|---|---|---|
| 0 | De-risking spike + skeleton | — | 3–4 |
| 1 | Import, decode, source playback, source waveform | 0 | 3–4 |
| 2 | Tempo, grid and warp math (pure) | 0 | 2 |
| 3 | Beat and tempo analysis | 1, 2 | 6–8 |
| 4 | Render engine (Smooth + Resample) | 1, 2 (not 3) | 4–5 |
| 5 | Export and quality protection | 1, 4 | 2–3 |
| 6 | Corrected view and A/B preview | 1, 2, 4 | 3–4 |
| 7 | Workflow UI, project/session, export dialog | 3, 5, 6 | 5–6 |
| 8 | Calibration, listening, hardening, release | 0–7 | 5–7 |
| **MVP total** | | | **33–43 weeks** (+20 % contingency → **40–52 weeks**) |
| 1.5 | Transient mode (**Post-MVP**) | MVP released; legal review L-3 | 4–6 |

```
                         ┌──────────────────────────────┐
                         │ P0  Spike + skeleton          │  go/no-go per decision,
                         │     (CI, licence gate, gen/eval)│  measured perf + thresholds
                         └──────────┬───────────┬────────┘
                                    │           │
               ┌────────────────────▼──┐   ┌────▼──────────────────┐
               │ P1 Import/decode/      │   │ P2 Tempo/grid/warp     │
               │    playback/peaks      │   │    math (pure bf_dsp)  │
               └───┬─────────┬──────┬───┘   └──┬──────────┬─────────┘
                   │         │      │          │          │
                   │   ┌─────▼──────▼──────────▼──┐       │
                   │   │ P4 Render engine          │◄──────┘   (P4 is driven by --warp-json;
                   │   │    Smooth/Resample, chunks│           it does NOT need P3)
                   │   └──┬──────────────┬─────────┘
   ┌───────────────▼──┐   │              │
   │ P3 Beat + tempo   │◄──┼── (P1 PCM, P2 fits)
   │    analysis       │   │              │
   └────────┬──────────┘ ┌▼──────────┐ ┌─▼─────────────────┐
            │            │ P5 Export  │ │ P6 Corrected view  │
            │            │  + gates   │ │    + A/B preview   │
            │            └─────┬──────┘ └─────────┬─────────┘
            └──────────────────┼──────────────────┘
                         ┌─────▼──────────────────────────┐
                         │ P7 Workflow UI + project +     │
                         │    export dialog               │
                         └─────┬──────────────────────────┘
                         ┌─────▼──────────────────────────┐
                         │ P8 Calibration, listening,     │──► MVP release
                         │    hardening, signed installer │
                         └─────┬──────────────────────────┘
                               ▼
                         Phase 1.5 Transient mode (Post-MVP)

Parallel tracks (start in P1, finish in P8): corpus curation + annotation (BENCHMARK_CORPUS),
legal review queue (OPEN_QUESTIONS §2), user interviews (USER_PERSONAS OQ1–4).
```

Single-developer order: 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8. With two developers: DSP track (2 → 4 → 5) runs beside the analysis track (3); UI work (6, 7) starts once 4 lands.

### Effort assumptions
- One experienced C++20 developer who has shipped JUCE (ARCHITECTURE_RESEARCH Recommendation 1), full-time, AI-assisted coding, no new algorithm research needed (the research is done).
- Reference laptop (REF-MID, [PERFORMANCE_BENCHMARKS.md §1](../07_quality/PERFORMANCE_BENCHMARKS.md)) available from Phase 0.
- Legal review runs in parallel and does not block engineering until Phase 8. If L-2 (Beat This data caveat) or L-5 (Suno ToS for test data) resolve negatively, add 2–4 weeks (DSP-only analysis quality work, or a weaker proxy corpus).
- Corpus annotation (≥ 30 Tier C + ≥ 50 Tier U songs at ~1.5 h each ≈ 120 h, [BENCHMARK_CORPUS.md §1](../07_quality/BENCHMARK_CORPUS.md)) is spread across Phases 1–7 at ~4 h/week and is included in the totals.
- A listening panel of ≥ 8 is reachable in Phase 8. A formal 15–20-listener MUSHRA is not in the estimate (O-5).
- Ranges are wide on purpose. Phase 3 and Phase 7 carry most of the uncertainty (R-40, R-13).

---

## Phase 0: De-risking spike + skeleton

**Objective.** Retire the Med-confidence decisions before anything depends on them, and replace PROVISIONAL performance and fidelity numbers with measurements on REF-MID. Leave behind a repo that builds, tests and enforces the licence policy from the first commit.

**Dependencies.** None. Needs REF-MID hardware (O-4) and, for spike S0-6, owner-supplied Suno files under the L-5 caveat.

**Modules/files touched.** `CMakeLists.txt`, `CMakePresets.json`, `cmake/Deps.cmake` (FetchContent pins + SHA256), `vcpkg.json` (mp3lame only), `.github/workflows/ci.yml`, `licenses/policy.toml`, `tools/sbom/`, `third_party/*/{LICENSE,VERSION,SOURCE_URL}`, `src/bf_dsp/` (one test target, `Result<T>`), `apps/beatfix/` (empty window), `apps/beatfix-cli/` (stub), `tools/gen/` (S01, S03, S08, S10, S12, S16), `tools/eval/` (O1, O6, O13, O17, O19, A10), `tools/spike/` (throwaway drivers, kept for reproducibility), `models/` (ONNX + SHA256 + LICENSE via LFS), `installer/BeatFix.iss`.

**Implementation tasks.**
1. *Skeleton.* CMake ≥ 3.28, MSVC v143 x64, `/W4 /permissive- /utf-8 /fp:precise`; FetchContent pins: JUCE 9.0.x exact tag, Signalsmith Stretch 1.4.0, Catch2 v3, ORT 1.30 CPU prebuilt zip, r8brain-free-src, minimp3, libebur128; vcpkg `mp3lame`. CI on `windows-latest`: build, unit tests, ASan job. One Catch2 test in `bf_dsp`. Empty JUCE window. Unsigned Inno installer. Constants header for heuristics (ARCHITECTURE_RESEARCH §13).
2. *Licence gate.* SPDX scan of `third_party/` + DLL manifest against the ALLOW/DENY policy ([LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) Technical Details). CI fails on DENY/unknown. `BF_WITH_RUBBERBAND=OFF` by default and refused in release presets.
3. *Spike S0-1, Signalsmith variable-ratio alignment.* Drive `process()` per synthesis interval from W⁻¹ with absolute-position rounding ([TIME_STRETCH_RESEARCH.md TD-4](../03_research/TIME_STRETCH_RESEARCH.md), [WARP_FUNCTION.md §7](../04_algorithms/WARP_FUNCTION.md)). Inputs: impulse/burst/kick trains S01(a–c) at constant ratios {0.90, 0.95, 0.98, 1.00, 1.02, 1.05, 1.10} and an S03-derived piecewise W. Verify `outputSeek` pre-roll at a non-unity initial rate and the latency compensation. Measure O1 (Path K), O13(a,b), O17, render RTF for `presetDefault` / `presetCheaper`.
4. *Spike S0-2, minimp3 gapless.* LAME-encode S01(b) at CBR320/V0/V2, 44.1 and 48 kHz → `minimp3_ex` decode with Xing/LAME trim → first-click offset and length. Also a file without a LAME tag (expected: offset reported, not silently wrong).
5. *Spike S0-3, Beat This ONNX on CPU.* Own export script (pinned opset) for `final0` and `small0`; activation parity vs the PyTorch reference on ≥ 10 clips. Includes the 22.05 kHz downsample through r8brain vs the reference soxr path (M-6, R-06). ORT 1.30 CPU on REF-MID: RTF, peak RSS, intra-op threads, run-to-run determinism with fixed threads (D1), `RunOptions::SetTerminate` cancel latency, `DisableTelemetryEvents()` verified with an ETW listener.
6. *Spike S0-4, r8brain contract.* Exact output length, zero net delay after compensation, DSP-RES-1 passband/stopband at 44.1↔48↔96 kHz ([RESAMPLING.md](../04_algorithms/RESAMPLING.md) Tests).
7. *Spike S0-5, Rubber Band R3 reference renders* (dev build only, never distributed; L-7). `setKeyFrameMap` at anchors, `OptionChannelsTogether`. Produce reference distributions of O1, O6–O10, O13 and CPU time. Run the threshold procedure of [DSP_TEST_STRATEGY.md §7](../07_quality/DSP_TEST_STRATEGY.md) steps 1–3 for engine-fidelity metrics.
8. *Spike S0-6, real Suno probe (10–20 files).* Codec, bitrate, SR, bit depth, LAME tag presence, spectral cutoff (lossy-origin WAV?), sample/true peak, LUFS, rough drift magnitude (quick Python Beat This run), peak growth after a Signalsmith render. Answers M-1…M-7.
9. *Spike report.* One page per spike in `tools/spike/REPORT.md`: measured numbers, pass/fallback decision, updated PROVISIONAL targets (measured × 1.2 headroom, PERFORMANCE_BENCHMARKS Recommendation).

**Tests.** CI green (build + 1 unit test + licence gate). Spike checks: O1 median ≤ 1 ms / P95 ≤ 3 ms / max ≤ 5 ms and |bias| ≤ 0.5 ms (DSP-STR-1); O13(a,b) exact (DSP-STR-4); DSP-DEC-2 for MP3 offset 0 ± 1 sample; DSP-RES-1; activation parity tolerance (set in the spike); ORT D1.

**Acceptance criteria.**
- Each of S0-1 … S0-6 has a recorded decision: **keep** (decision stands), **fix** (driver/config change, decision stands), or **fallback** (Bungee for S0-1, libmpg123 DLL for S0-2, small model or DSP-only for S0-3, own polyphase SRC for S0-4).
- PERFORMANCE_BENCHMARKS §4 targets carry measured values or an explicit "not measurable yet" note.

**Exit criteria (gates).**
- G0.1 Signalsmith alignment passes O1 at all spike ratios **or** the Bungee fallback is adopted and passes. *No stretch-engine work starts without this.*
- G0.2 MP3 decode offset = 0 ± 1 sample for LAME-tagged files.
- G0.3 Beat This full model analysis ≤ 0.15× RT on REF-MID **or** the small-model decision is taken now (DECISION_SUMMARY §1 allows the switch).
- G0.4 Licence gate active in CI. No GPL/AGPL/NC code in any non-dev target.

**REQ families.** REQ-QA, REQ-LIC, REQ-PERF (calibration), REQ-STRETCH, REQ-IO, REQ-BEAT (feasibility only).

---

## Phase 1: Import, decode, source playback, source waveform

**Objective.** Open one file, decode it once gapless-correct into an immutable float32 PCM cache, show its waveform and play it through a realtime-safe engine. This is the spine everything else reads from.

**Dependencies.** Phase 0 (skeleton, S0-2 decoder decision).

**Modules/files touched.** `bf_io/decode.cpp` (JUCE readers for WAV/RF64/AIFF/FLAC/Ogg Vorbis; `minimp3_ex` for MP3), `bf_io/pcm_cache.cpp`, `bf_io/atomic_file.cpp`, `bf_engine/session.cpp`, `bf_engine/jobs.cpp`, `bf_engine/cache.cpp` (PCM cache key), `bf_engine/playback.cpp`, `bf_dsp/peaks.h`, `bf_dsp/xfade.h`, `apps/beatfix/` (MainWindow shell, WaveformLane source lane, Transport), `apps/beatfix-cli` (`probe`, `decode`), `tools/gen` (full S catalogue), `tests/unit`, `tests/integration`. Docs: [UX_BEHAVIOR.md §2, §14, §15](../06_ui/UX_BEHAVIOR.md), [AUDIO_QUALITY_RESEARCH.md §4.10](../03_research/AUDIO_QUALITY_RESEARCH.md), [ARCHITECTURE_RESEARCH.md §3.3, §4, §6, §9](../03_research/ARCHITECTURE_RESEARCH.md).

**Implementation tasks.**
1. *Import.* Probe container/codec/SR/channels/duration in ≤ 200 ms; reject > 2 channels and AAC/M4A/Opus with the unsupported-format message (MVP). Streaming XXH3-128 file hash; PCM hash over the trimmed PCM; decoder id + version recorded.
2. *Decode.* Gapless trim (LAME/Xing); damaged MP3 frames replaced by silence of the exact skipped length and logged as damaged spans. Float32 PCM cache file (header + CRC), memory-mapped; temp → flush → rename. Source opened share-read only; never written.
3. *Time base.* `int64` sample positions and `double` seconds everywhere; one rounding policy in one header ([QA_STRATEGY.md §2](../07_quality/QA_STRATEGY.md) "Sample/time conversion").
4. *Playback.* Own `AudioIODeviceCallback` (not `AudioTransportSource`, ARCHITECTURE_RESEARCH Implementation Implications). SPSC POD command queue in, atomics out. `[RT]` annotations and the MUST-NOT list enforced by review. Background copy at device SR when device SR ≠ source SR (preview only; never feeds export, ADR-002 Consequences). Pre-touch mapped pages ahead of the playhead.
5. *Peaks.* Min/max (+RMS) pyramid at 256/1024/4096/16384 samples per bin, built on the peaks worker, progressive during decode.
6. *Diagnostics.* Rotating local log (no full paths, no audio), local minidumps, xrun/max-callback counters, orphan temp sweep at startup.

**Tests.** DSP-DEC-1 (bit-exact WAV/FLAC/AIFF), DSP-DEC-2 (MP3/Ogg offset 0 ± 1 sample), DSP-INT-1 (source SHA-256 unchanged; read-only handle), S14 edge cases (silence, 0.5 s file, 1-sample file, 3-channel rejection), L1 sample/time conversion (10-min last-sample error < 0.5 sample), L6 decoder fuzz job started (30 min nightly, ASan), P7 decode RTF, P10 xruns while a dummy CPU load runs.

**Acceptance criteria.** All MVP import formats open, play and show a waveform. Corrupt and unsupported files give the UX_BEHAVIOR §14/§15 messages, never a crash. Re-opening the same file hits the PCM cache.

**Exit criteria (gates).**
- G1.1 **DECISION_SUMMARY "Phase 1 gate":** click-track test, 0-sample offset for MP3 (LAME CBR/VBR at 44.1/48 kHz) and Ogg, exact for lossless.
- G1.2 0 xruns in 10 min of playback with analysis-like background load (P10).
- G1.3 Decoder fuzzing runs nightly with no open crash.

**REQ families.** REQ-IO, REQ-AUDIO (precision, source immutability), REQ-PLAY (transport), REQ-RT, REQ-UI (source lane), REQ-PRIV (local logs only).

---

## Phase 2: Tempo, grid and warp math (pure `bf_dsp`)

**Objective.** Implement every deterministic formula between "a list of beat times" and "a validated warp map W / W⁻¹", tested against exact synthetic beat lists. No audio, no ML.

**Dependencies.** Phase 0 skeleton only. Runs in parallel with Phase 1.

**Modules/files touched.** `bf_dsp/tempo_fit.{h,cpp}` (Huber IRLS line fit, Theil–Sen cross-check, slope uncertainty), `bf_dsp/smooth.{h,cpp}` (Whittaker–Eilers banded solver with IRLS weights, shared by analysis and warp), `bf_dsp/grid.{h,cpp}` (T_k = φ + k·P, slot assignment, phase estimation), `bf_dsp/warp.{h,cpp}` (anchor smoothing with N_c detents and g_i breaks, piecewise-linear W/W⁻¹ with edge rule, `WarpMap::validate`, correction class), `bf_engine/warp_json.cpp` (schema v1). Specs: [BPM_ESTIMATION.md §3–8](../04_algorithms/BPM_ESTIMATION.md), [TARGET_BEAT_GRID.md §1–5](../04_algorithms/TARGET_BEAT_GRID.md), [WARP_FUNCTION.md §1–9](../04_algorithms/WARP_FUNCTION.md), [WARP_MAP.md](../04_algorithms/WARP_MAP.md), [WAVEFORM_VIEW.md §5](../06_ui/WAVEFORM_VIEW.md).

**Implementation tasks.**
1. Robust global tempo (Huber IRLS + Theil–Sen), fractional BPM, local tempo curve.
2. Target grid: phase φ, beat-count-preserving slot assignment, pre/post-roll edges.
3. Anchor smoother: λ from N_c (`λ = 1/(16 sin⁴(π/N_c))`), six detents {Global, 32, 16, 8, 4, Strict}, `g_i = 0` across change points, ratio invariants (hard 0.5–2.0, warn 0.92–1.08, Δ_min).
4. W / W⁻¹ evaluation (binary search; f64), absolute-position block sample counts.
5. WarpMap JSON with `schemaVersion`, `algoVersion`, `inputsHash`; canonical serialisation (it is part of the render cache key).
6. Correction intensity `|log2(r/g)|` and class assignment (DECISION_SUMMARY §6).

**Tests.** All rows of [QA_STRATEGY.md §2](../07_quality/QA_STRATEGY.md) for BPM, intervals, grid, warp ratio, monotonic mapping, outlier rejection; DSP-MAP-1 on S03-derived beats; property tests (fixed seed in CI, random seed nightly); O5; warp rebuild timing ≤ 100 ms for 10⁴ anchors.

**Acceptance criteria.** Exact BPM recovery on noiseless grids (|err| < 1e-9 BPM); ≤ 20 % outliers keep BPM within 0.05; |T_10000 − exact| < 1 µs; W⁻¹(W(t)) = t within 1 µs; W(anchor) = target exactly in Strict; JSON round-trip identity.

**Exit criteria (gates).**
- G2.1 Property suite passes 10 000 random anchor sets per nightly run.
- G2.2 `WarpMap::validate` rejects every invariant violation in the negative-test set.
- G2.3 Unreliable-analysis gate implemented: robust residual scale > 0.15·P → "beat analysis unreliable", no auto-render (WARP_FUNCTION Risks).

**REQ families.** REQ-TEMPO (BPM math), REQ-WARP, REQ-AUDIO (f64 time), REQ-QA.

---

## Phase 3: Beat and tempo analysis

**Objective.** From decoded PCM, produce persisted activations, beats, downbeats, tempo candidates with octave ranking, a stability classification and calibrated-ready confidence, with a DSP fallback that works when the model is missing.

**Dependencies.** Phase 1 (PCM cache, jobs, cancellation), Phase 2 (tempo fits, Whittaker smoother). Spike S0-3 decision (full vs small model).

**Modules/files touched.** `bf_analysis/analyzer.h` (`IBeatAnalyzer`), `bf_analysis/frontend.cpp` (mono downmix, 22.05 kHz via r8brain, log-mel matching Beat This exactly), `bf_analysis/beatthis_onnx.cpp` (ORT session, 1500-frame chunks / border 6, `SetTerminate`), `bf_analysis/dsp_onset_fallback.cpp` (HPSS percussive + SuperFlux, low/mid band ODFs, hop-128 refinement ODF), `bf_analysis/tempo_hypotheses.cpp` (ACF×Fourier tempogram, candidates ×{1/3, 1/2, 2/3, 1, 3/2, 2, 3} in 40–250 BPM, level ranking with Green/Amber/Red margin, ±40 % corridor Viterbi), `bf_analysis/decode_dp.cpp` (time-varying Ellis DP, deterministic ties), `bf_analysis/repair_refine.cpp` (missing/extra repair, parabolic sub-frame, fixed-band snap, global offset), `bf_analysis/downbeat.cpp` (meter {3,4} phase Viterbi), `bf_analysis/stability.cpp` (change points, phase jumps, Stable/Drifting/Stepped/Spliced/Level switch/Unreliable), `bf_analysis/confidence.cpp`, `bf_engine/cache.cpp` (analysis cache keyed by PCM key + analyzer id), `models/`. Specs: [BEAT_TRACKING_RESEARCH.md §5–13](../03_research/BEAT_TRACKING_RESEARCH.md), [TEMPO_ESTIMATION_RESEARCH.md §2–8](../03_research/TEMPO_ESTIMATION_RESEARCH.md), [BPM_ESTIMATION.md §6–7](../04_algorithms/BPM_ESTIMATION.md), [WARP_FUNCTION.md §5](../04_algorithms/WARP_FUNCTION.md).

**Implementation tasks.**
1. *Front-end + model.* Mel parity with the reference within the S0-3 tolerance; model hash and export-script version recorded; telemetry off; fixed ORT session options; persisted float16 activations.
2. *DSP path.* Always computed (cheap): fallback, cross-check, refinement, band evidence for octave/downbeat.
3. *Tempo hypotheses and local period path* feeding the decoder.
4. *Decoding, repair, refinement, downbeats* as in BEAT_TRACKING_RESEARCH §12 pseudocode. Analysis-rate conversions in f64 seconds (DSP_RESEARCH Risks).
5. *Stability + anomalies.* Change points set `g_i = 0` for Phase 2's smoother; splices never smoothed; intentional tempo change → flag + ask (DECISION_SUMMARY §6).
6. *Confidence.* Per-beat logistic features; global = DSP–ML agreement, level margin, fraction confident. Thresholds as placeholders in one config file, logged with each analysis (calibrated in Phase 8).
7. *Persistence and cache.* Activations + beats + versions persisted; re-analysis only on explicit user action (D3 rule).
8. `beatfix-cli analyze` emitting JSON for `tools/eval`.

**Tests.** DSP-ANA-1 (A10 median ≤ 2 ms / P95 ≤ 5 ms on clicks and synthetic kicks; A11 ≤ 0.01 BPM on 240 s), DSP-ANA-2 (ramp → drift; step located ±1 beat), DSP-ANA-3 (FP/FN ≤ 2 % at σ ≤ 5 ms, ghosts at −18 dB), DSP-ANA-4 (correct level in candidate list; ambiguity flag); confidence monotonic under jitter (S05); model-missing path produces "DSP mode" capped at Amber; determinism D1 with fixed threads, D2 tolerances across thread counts; P1, P6; corpus Tier S/M baseline (A5, A9, A10, A11, A14) recorded as the first trend point.

**Acceptance criteria.** Every BENCHMARK_CORPUS category 1–13, 18, 20 synthetic item gives the expected behaviour column. Analysis never blocks the UI and cancels within 250 ms.

**Exit criteria (gates).**
- G3.1 **DECISION_SUMMARY "Phase 3 perf gate":** full model P1 ≤ 0.15× RT on REF-MID (measured target from Phase 0). Fail → ship the small model as default (≤ 0.05× RT) and log the decision.
- G3.2 DSP-ANA-1…4 pass.
- G3.3 Fallback path passes DSP-ANA-1 on clicks/kicks (accuracy on octave/weak-onset material may be lower, as documented).
- G3.4 Activations persisted; re-render never triggers re-analysis (integration test).

**REQ families.** REQ-BEAT, REQ-TEMPO, REQ-PERF (analysis), REQ-PROJ (persisted analysis), REQ-PRIV (no telemetry), REQ-QA.

---

## Phase 4: Render engine (Smooth + Resample)

**Objective.** Render a pitch-preserving corrected version from source PCM and a warp map: sample-accurate, linked-stereo, deterministic regardless of core count, cached per chunk. Also the constant-ratio "Resample" mode.

**Dependencies.** Phase 1 (PCM cache, jobs), Phase 2 (W, W⁻¹, WarpMap). Not Phase 3: tests use synthetic W via `--warp-json`. Spike S0-1 decision.

**Modules/files touched.** `bf_dsp/stretch.h` (`ITimeStretcher`: prepare / latencies / reset / renderChunk / id), `bf_dsp/stretch_signalsmith.cpp` (W⁻¹ driver, presets), `bf_dsp/stretch_rubberband.cpp` (dev/test only behind `BF_WITH_RUBBERBAND`), `bf_dsp/resample.h` (r8brain wrapper with exact-length/zero-delay contract; Resample mode at constant ratio only, pitch shift in cents), `bf_dsp/xfade.h`, `bf_engine/render_scheduler.cpp` (chunk plan at quiet beat boundaries every 8–16 beats, pre-roll, overlap crossfade, ordered merge), `bf_engine/cache.cpp` (content-addressed chunk cache, LRU budget), single-pass reference renderer, `apps/beatfix-cli render --warp-json`. Specs: [TIME_STRETCH_RESEARCH.md TD-4, TD-5, Recommendation](../03_research/TIME_STRETCH_RESEARCH.md), [ARCHITECTURE_RESEARCH.md §5](../03_research/ARCHITECTURE_RESEARCH.md), [WARP_FUNCTION.md §6–7](../04_algorithms/WARP_FUNCTION.md).

**Implementation tasks.**
1. Signalsmith adapter (stereo linked, `presetDefault`; `presetCheaper` only if preview latency demands it, M-3). Pre-roll via `outputSeek`; end via `inputLatency()` extra samples then `flush()`.
2. Resample mode: refuse when W is not constant-ratio (DECISION_SUMMARY §1); report cents shift.
3. Single-pass reference render (also the fallback export path).
4. Chunk-parallel render: deterministic chunk plan as a pure function of (PCM, W, params); per-worker stretcher instances; boundary check (O15) per chunk with fallback to single-pass for that region.
5. Render generations with preallocated output and per-chunk ready flags (consumed in Phase 6).
6. Tail policy per OPEN_QUESTIONS §A "tail policy": keep the tail; output length per O17.
7. Rubber Band R3 dev engine for regression baselines and listening references (never in release presets).

**Tests.** DSP-STR-1…6, DSP-REN-1 (1/2/4/8 threads, boundaries forced onto transients; null vs single-pass ≤ −90 dBFS peak), DSP-REN-2 (100 random cancels), DSP-REN-3 only if Passthrough is adopted (M-4), GOLD-01…07, GOLD-12, PERF-EXT-1/2, PERF-SCALE-1, P2/P2rs.

**Acceptance criteria.** Identity map (GOLD-01) meets O18. Ramp correction (GOLD-02) has no seams (O15 zero flags). Stereo set (GOLD-06) meets O13 a/b exactly.

**Exit criteria (gates).**
- G4.1 O1 within the thresholds frozen in Phase 0 on all DSP-STR-1 ratios and on time-varying W.
- G4.2 D1: identical SHA-256 for 1/4/8 threads on the same machine.
- G4.3 P2 Smooth render ≤ measured target (PROVISIONAL 0.25× RT) on REF-MID.
- G4.4 No NaN/Inf, exact length (O16, O17) at extreme ratios 0.5/2.0.

**REQ families.** REQ-STRETCH, REQ-AUDIO, REQ-WARP (consumption), REQ-PERF (render), REQ-RT (cancellation), REQ-QA.

---

## Phase 5: Export and quality protection

**Objective.** Export WAV/FLAC/MP3 from the render with at most one SRC, correct dither, a non-silent peak gate, correct metadata, atomic write and automatic re-decode validation. Close the full CLI chain so L3 goldens run nightly.

**Dependencies.** Phase 4 (render), Phase 1 (decoders for validation).

**Modules/files touched.** `bf_io/export.cpp` (WAV 16/24/32f + RF64, FLAC 16/24 via JUCE/libFLAC), `bf_io/lame_writer.cpp` (LAME as separate unrenamed DLL, LAME tag written; CBR 320 default per DECISION_SUMMARY §6), `bf_io/metadata.cpp` (carry-over, strip stale tags, `TBPM`/`BPM`, `BEATFIX_RENDER` tag), `bf_dsp/dither.{h,cpp}`, `bf_dsp/resample.h` (export SRC), `bf_engine/export_job.cpp` (render → measure → [gain/limiter] → SRC → dither → encode → validate → atomic rename), libebur128 integration, optional true-peak limiter, `apps/beatfix-cli export`. Specs: [AUDIO_QUALITY_RESEARCH.md §4.2–4.11](../03_research/AUDIO_QUALITY_RESEARCH.md), [DITHERING.md](../04_algorithms/DITHERING.md), [RESAMPLING.md](../04_algorithms/RESAMPLING.md), [EXPORT_UI.md §3–5](../06_ui/EXPORT_UI.md) (engine side).

**Implementation tasks.**
1. Writers + LAME DLL loading; LGPL notice + source offer artefacts.
2. Measurement: sample peak, BS.1770 true peak (4×), LUFS-I, LRA for source and output.
3. Peak gate: 0 dBFS (integer), −1 dBTP (lossy), warn-only for float WAV; choices lower gain (default) / opt-in TP limiter / export anyway with clip count. The choice goes in the export report and the project. Nothing silent.
4. TPDF dither on float → int only, deterministic seed from settings; no dither on float/lossy.
5. Export SRC only when requested or forced by the codec.
6. Filename rules, collision handling, refusal to overwrite source (path + volume serial/file index).
7. Re-decode validation per AUDIO_QUALITY_RESEARCH §4.11 before rename; temp removed on cancel/failure.
8. Golden runner wired: GOLD-08…11 full chain nightly.

**Tests.** DSP-EXP-1, O14, O17, O19, DITHERING Tests (acceptance), RESAMPLING Tests (acceptance), limiter latency 0-sample offset, GOLD-08…11, P8 export RTF, filename edge cases (reserved names, emoji/CJK, long paths).

**Acceptance criteria.** Every MVP export format round-trips through the import path with offset 0 and expected length. A −0.1 dBFS master that overs after TSM always produces the gate choice, never a silently clipped file.

**Exit criteria (gates).**
- G5.1 O19 passes for all formats/bit depths at 44.1/48/96 kHz.
- G5.2 Full L3 golden chain (import → analyse → render → export) green nightly.
- G5.3 SBOM + `THIRD_PARTY_NOTICES.txt` generated including LAME (LGPL) obligations.

**REQ families.** REQ-IO, REQ-AUDIO (dither, SRC, peak gate, validation), REQ-LIC, REQ-PROJ (export recorded), REQ-QA.

---

## Phase 6: Corrected view and A/B preview

**Objective.** Show original and corrected waveforms on a shared musical timeline with correction heat, and let the user A/B at the same musical position with progressive preview after every edit.

**Dependencies.** Phase 1 (playback, peaks), Phase 2 (W mapping, correction classes), Phase 4 (generations, chunk cache).

**Modules/files touched.** `bf_engine/playback.cpp` (atomic generation pointer + audio ack retirement, chunk-ready checks, original read at W⁻¹(τ), 20 ms equal-power crossfade, fallback when a chunk is not ready), `bf_engine/render_scheduler.cpp` (priority outward from the playhead, previous generation keeps playing until the playhead chunk is ready), corrected-lane peaks per finished chunk, `apps/beatfix/` (dual WaveformLane, source pyramid mapped through W as the instant preview then replaced by real render peaks, correction strip, connectors, beat/downbeat/grid markers, hover card, overview strip, collapsible tempo view, region list). Specs: [WAVEFORM_VIEW.md](../06_ui/WAVEFORM_VIEW.md), [PLAYBACK_AND_AB.md](../06_ui/PLAYBACK_AND_AB.md), [UX_BEHAVIOR.md §9–10](../06_ui/UX_BEHAVIOR.md), [ARCHITECTURE_RESEARCH.md §4](../03_research/ARCHITECTURE_RESEARCH.md).

**Implementation tasks.**
1. Generation publication and retirement without `shared_ptr` or locks on the RT path.
2. A/B semantics per PLAYBACK_AND_AB §3 (seek to W⁻¹/W, 20 ms equal-power, no auto gain).
3. Progressive preview and "rendering" hatch on invalid chunks.
4. Lanes, markers, heat (`|log2(r/g)|`), predicted waveform at reduced opacity with an "estimate" label.
5. Canvas accessibility: region list as the non-visual equivalent (WAVEFORM_VIEW §9–10).

**Tests.** A/B position mapping unit tests (W⁻¹(W(s)) consistency incl. edges), P3 preview latency ≤ 500 ms, P5 main-thread stall ≤ 50 ms typical / ≤ 100 ms hard, P10 0 xruns during render + playback + A/B toggling storm, generation-retirement stress test (no use-after-free under ASan), switch latency ≤ 50 ms.

**Acceptance criteria.** Stable 110 BPM song shows a mostly grey correction strip. Drift song shows graded heat that matches the warp map. A/B never jumps musical position.

**Exit criteria (gates).**
- G6.1 P3, P5, P10 targets met on REF-MID.
- G6.2 ASan-clean generation swap stress test (10 000 swaps).

**REQ families.** REQ-PLAY, REQ-UI (waveform/heat/tempo view), REQ-RT, REQ-PERF (preview, UI stall).

---

## Phase 7: Workflow UI, project/session, export dialog

**Objective.** Make the one-path workflow (open → analyse → check → listen → export) complete, explainable and recoverable: confidence and candidates, target BPM, Natural↔Strict, issues, undo/redo, projects with autosave and relocation, the export dialog.

**Dependencies.** Phase 3 (analysis results), Phase 5 (export engine), Phase 6 (views, A/B).

**Modules/files touched.** `bf_engine/project_json.cpp` (`.beatfix` schemaVersion + migration chain, unknown fields preserved, atomic save via temp + `ReplaceFileW`, `.bak`, autosave + PID lock recovery, relocation by file/PCM hash), `bf_engine/session.cpp` (undoable commands), `apps/beatfix/` (MainWindow states, confidence presentation, candidate picker ×2/÷2 and alternates, downbeat shift, accept/reject change points, intentional-change flag + ask, target BPM field with suggestion, Natural↔Strict six detents, mode menu **Smooth / Resample** only (OPEN_QUESTIONS §A), quality status + issues list, reset vs re-analyse, export dialog + pre-flight + result card, keyboard map, UIA labels, light/dark/contrast). Specs: [MAIN_WINDOW.md](../06_ui/MAIN_WINDOW.md), [UX_BEHAVIOR.md](../06_ui/UX_BEHAVIOR.md), [EXPORT_UI.md](../06_ui/EXPORT_UI.md), [KEYBOARD_SHORTCUTS.md](../06_ui/KEYBOARD_SHORTCUTS.md), [UI_UX_PRINCIPLES.md](../06_ui/UI_UX_PRINCIPLES.md), `05_architecture/PROJECT_SESSION_MODEL.md`.

**Implementation tasks.**
1. Analysis view states and confidence/candidate UX; Amber/Red asks the user (DECISION_SUMMARY §1 Octave row).
2. Correction controls: target BPM (default round(BPM)), Natural↔Strict, mode, intentional-change options "flatten to target" / "keep section unwarped (r = 1)".
3. Issues list driven by engine flags (warn band, large overall change, unreliable analysis, peak over, damaged spans).
4. Undo/redo of every edit; render invalidation is local through the chunk cache.
5. Project save/open/migrate/relocate; autosave and crash recovery.
6. Export dialog on top of the Phase 5 engine; peak-gate dialog with numeric preview; result card.
7. Keyboard-only operation and screen-reader announcements.

**Tests.** Project round-trip identity; every schema fixture migrates; corrupted project → typed error; relocation by PCM hash; autosave/lock recovery after killed process; every "MUST" statement in UX_BEHAVIOR as a scripted check (CLI-assisted where possible); draft L8 acceptance matrix run (brief §44 scenarios); keyboard-only walkthrough; UIA inspection of all controls.

**Acceptance criteria.** With High confidence, open → export needs zero decisions (UI_UX_PRINCIPLES P1). Every automatic decision has a visible reason and a one-click override. No edit triggers re-analysis.

**Exit criteria (gates).**
- G7.1 All UX_BEHAVIOR MUST checks pass.
- G7.2 Project migration and crash-recovery tests pass.
- G7.3 Draft acceptance matrix: all scenarios pass or have a filed defect with a Phase 8 owner.

**REQ families.** REQ-UI, REQ-PROJ, REQ-WARP (user controls), REQ-TEMPO (candidates), REQ-IO (export dialog), REQ-PLAY.

---

## Phase 8: Calibration, listening, hardening, release

**Objective.** Freeze every PROVISIONAL threshold on the corpus, confirm perceptual quality by listening, harden against long/odd inputs, clear licences and ship a signed MVP.

**Dependencies.** Phases 0–7. Legal answers L-1, L-2, L-4, L-8 before public release.

**Modules/files touched.** Config constants headers (thresholds), `corpus/manifest/`, `tools/eval/bench.py`, `tools/listening/`, `goldens/`, `installer/`, `tools/sbom/`, `07_quality/ACCEPTANCE_CRITERIA.md` (frozen numbers), release notes.

**Implementation tasks.**
1. *Corpus.* Tier S complete, M ≥ 40, C ≥ 30, U ≥ 50 if L-5 clears (else S16 + M with synthetic drift as proxy, R-35). Annotation protocol and agreement ([BENCHMARK_CORPUS.md §3](../07_quality/BENCHMARK_CORPUS.md)); 60/40 dev/test split.
2. *Calibration.* Confidence thresholds (ECE, A13), octave margin bands, warn bands, correction-class bins, analysis thresholds (A5, A9, A10, A11) on the dev split; report on the test split; freeze into ACCEPTANCE_CRITERIA.
3. *Listening.* LIS-MUSHRA engine milestone with Rubber Band R3 reference (non-inferiority, SUBJECTIVE_LISTENING_TESTS §6); LIS-ABX on stable songs (transparency); LIS-PAIR for N_c default (8 vs 4 vs 16) and Strict-step audibility; correction-class perceptual calibration (WAVEFORM_VIEW §5.4). Escalate per the decision rules if Signalsmith fails non-inferiority (tune preset → Rubber Band licence decision → Transient mode priority).
4. *Hardening.* PERF-LONG-1…4, PERF-CANCEL-1, fuzzing 30 min nightly clean for 2 weeks, REF-LOW run, determinism D2 matrix.
5. *Release.* Signed Inno per-user installer + portable zip, timestamped signatures, PDBs archived, SHA256SUMS, third-party notices, LGPL source offer, EULA carve-outs, no network calls verified (firewall/ETW check).
6. *Acceptance.* Full L8 matrix on the release candidate.

**Tests.** L4 all tiers (S/M/C in CI, P/U local), L5 on REF-MID and REF-LOW, L6, L7, L8.

**Acceptance criteria / Exit criteria.** The MVP release criteria in [MVP_DEFINITION.md §3](MVP_DEFINITION.md) and the release DoD in [DEFINITION_OF_DONE.md §3](DEFINITION_OF_DONE.md). Key gates:
- G8.1 No PROVISIONAL threshold left in ACCEPTANCE_CRITERIA; every threshold has its measured basis.
- G8.2 Smooth passes MUSHRA non-inferiority on drums, vocals, dense mix at product-typical ratios, or the owner has accepted the documented gap.
- G8.3 Stable-song ABX shows no significant difference.
- G8.4 Legal sign-off recorded for L-1, L-2, L-4, L-8.

**REQ families.** All; especially REQ-QA, REQ-PERF, REQ-LIC, REQ-PRIV.

---

## Phase 1.5: Transient mode (Post-MVP)

**Objective.** Sharper drums in dense mixes without new seams: HPSS hybrid where harmonic content goes through Smooth and percussive content through short-window OLA, both on the same W⁻¹ (DECISION_SUMMARY §1; TIME_STRETCH_RESEARCH rec. 4, F9).

**Dependencies.** MVP released. Legal review L-3 (Fraunhofer family US 9,230,558 B2 and related; HPSS hybrid "should also be reviewed"). Measurement M-11 (HPSS parameters on AI masters).

**Modules/files touched.** `bf_dsp/hpss.{h,cpp}` (channel-shared soft masks), `bf_dsp/stretch_transient.cpp` (`ITimeStretcher` implementation wrapping Signalsmith + OLA 512/256 at 44.1 kHz), mode menu entry in `apps/beatfix`, `TRANSIENT_PRESERVATION.md`.

**Implementation tasks.** Shared masks (stereo rule, TIME_STRETCH rec. 10); same W⁻¹ for both paths with latency alignment; sum; chunk-boundary behaviour identical to Smooth; mode persisted in project and cache key.

**Tests.** Impulse-train alignment (O1) for both paths and the sum; O8–O10 relative improvement vs Smooth on S08 and GOLD-04; O13 a/b exact; O15 zero new flags; LIS-PAIR on drum-heavy, vocal and pad clips.

**Exit criteria.** Ships only if LIS-PAIR shows a significant preference on drum-heavy clips with no significant loss on vocals/pads (SUBJECTIVE_LISTENING_TESTS §6) **and** L-3 is cleared. Otherwise it stays unreleased.

**REQ families.** REQ-STRETCH, REQ-AUDIO, REQ-LIC.

## 2. What is deliberately not in this roadmap
Slice/Texture modes, region-switching or per-bin Auto, stems, per-section target BPM, manual marker dragging, enhancement, AAC/Opus, batch/CLI as a product, MSIX/Store, macOS/Linux. See [MVP_DEFINITION.md §2](MVP_DEFINITION.md). Adding any of them goes through the scope-creep rule in MVP_DEFINITION §4.
