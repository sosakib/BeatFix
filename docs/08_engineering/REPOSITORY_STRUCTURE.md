# Repository Structure

Status: 2026-10-08, v1.0. Derived from [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §10, [QA_STRATEGY.md](../07_quality/QA_STRATEGY.md) §Implementation Implications and [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) §Implementation Implications. Module rules come from [ADR-001](../09_decisions/ADR-001-desktop-framework-and-language.md) and [TECH_STACK.md](TECH_STACK.md) §1.1.

Principle: folders are created when the first file needs them, not before. The tree below is the target shape, not a scaffolding checklist.

## 1. Tree

```
BeatFix/
├── CMakeLists.txt              top level: options, add_subdirectory(src, tests)
├── CMakePresets.json           dev / release / asan presets (BUILD_AND_DEPENDENCIES.md §2)
├── vcpkg.json                  manifest: mp3lame only, builtin-baseline pinned
├── .clang-format  .clang-tidy  .gitattributes  .gitignore
├── cmake/
│   ├── Deps.cmake              every FetchContent pin: URL + SHA256 (single place to bump versions)
│   └── Warnings.cmake          compiler flags per target type
├── src/
│   ├── bf_dsp/                 pure C++20 + std. No JUCE, no OS headers, no I/O
│   │     warp.{h,cpp}            WarpMap, W, W⁻¹, validate (WARP_FUNCTION.md, WARP_MAP.md)
│   │     smooth.{h,cpp}          Whittaker/Huber IRLS banded solver (shared by warp + tempo curve)
│   │     stretch.h               ITimeStretcher (interface #1 of 2)
│   │     stretch_signalsmith.cpp stretch_resample.cpp  [stretch_rubberband.cpp, stretch_bungee.cpp: flag-gated]
│   │     resample.{h,cpp}        r8brain wrapper with exact-length/zero-delay contract (one function, no interface)
│   │     render_plan.{h,cpp}     chunk cut points, pre-roll, crossfade (RENDER_PIPELINE.md)
│   │     peaks.{h,cpp}           min/max/RMS pyramid (ADR-008)
│   │     dither.{h,cpp} loudness.{h,cpp} xfade.h  result.h  units.h
│   ├── bf_analysis/            features + model + owned post-processing. No JUCE
│   │     analyzer.h              IBeatAnalyzer (interface #2 of 2)
│   │     beatthis_onnx.cpp       mel front-end + ORT session (telemetry off)
│   │     dsp_tracker.cpp         HPSS + SuperFlux fallback/cross-check
│   │     decode_beats.cpp repair.cpp refine.cpp tempo.cpp octave.cpp stability.cpp confidence.cpp
│   ├── bf_io/                  decode → PCM cache, export writers, atomic file ops
│   │     decode.{h,cpp}          JUCE readers (private) + minimp3_ex with gapless trim
│   │     export.{h,cpp}          WAV/FLAC via JUCE writers, peak gate, validation re-decode
│   │     lame_writer.cpp         AudioFormatWriter wrapping the delay-loaded LAME DLL
│   │     atomic_file.{h,cpp}     temp + FlushFileBuffers + ReplaceFileW
│   ├── bf_engine/              session, jobs, cache, playback mixer, project I/O, C API
│   │     session.{h,cpp} project_json.cpp jobs.cpp render_scheduler.cpp cache.cpp
│   │     playback.{h,cpp}        RT mixer + SPSC command queue ([RT] code lives here and in bf_dsp only)
│   │     beatfix_c.h             thin C-compatible API (the seam a Tauri shell would use)
│   ├── app/                    JUCE GUI executable (only target that links juce_gui_*)
│   │     Main.cpp MainWindow.cpp WaveformLane.cpp Transport.cpp Inspector.cpp ExportDialog.cpp
│   └── cli/                    console target `bftool` (see §3: dev-only in MVP, user CLI post-MVP)
├── tests/
│   ├── unit/                   Catch2, L1: math, warp, smoother, BPM, project migrations, cache keys
│   ├── dsp/                    Catch2, L2: synthetic signals through decoder/SRC/stretch/renderer/exporter
│   ├── golden/                 L3: goldens/<id>/{metrics.json, warp.json, hashes.json[, audio.flac]}
│   └── fixtures/               small committed inputs: project files per schemaVersion, warpmap/vN/,
│                               click-track MP3/FLAC (< 1 MB total), corpus manifests (no Tier P/U audio)
├── bench/                      Python, dev-only, never packaged
│   ├── requirements.txt        pinned + hashed
│   ├── gen/                    synthetic signal generators S01–S16 (DSP_TEST_STRATEGY.md §3)
│   ├── eval/                   mir_eval + METRICS.md implementations, bench.py (enforces allowed_use)
│   └── corpus/                 manifest/<id>.json, ATTRIBUTION.md (generated); audio lives outside git
├── models/
│   ├── manifest.json           id, file name, SHA-256, size, checkpoint URL + commit, opset, licence, notes
│   ├── export_beat_this.py     official checkpoint → ONNX; verifies vs PyTorch (BUILD_AND_DEPENDENCIES.md §5)
│   └── LICENSE.beat_this       MIT text + training-data caveat note
├── third_party/
│   ├── manifest.json           every dependency: name, version, SPDX id, linking, source URL, SHA-256
│   ├── licenses/<name>.txt     full licence text per dependency (feeds THIRD_PARTY_NOTICES.txt)
│   └── policy.toml             ALLOW / DENY lists (TECH_STACK.md §2)
├── installer/
│   └── BeatFix.iss             Inno Setup script (per-user)
├── tools/
│   ├── licence_check.py        policy gate: manifest + dumpbin /dependents vs allow-list
│   ├── make_notices.py         THIRD_PARTY_NOTICES.txt + SPDX SBOM from third_party/ and JUCE.spdx.json
│   └── null_test.py            chunked vs single-pass render residual (wraps bftool)
├── docs/                       this documentation tree
└── .github/workflows/
    ├── ci.yml                  PR + push jobs
    ├── nightly.yml             ASan, full DSP, golden, perf smoke
    └── release.yml             tag → signed artifacts (CI_CD.md §5)
```

## 2. What lives where (rules)

| Question | Answer |
|---|---|
| Can `bf_dsp` read a file, log, or take a lock? | No. It is pure computation over `std::span`. Logging happens in the caller. |
| Where do JUCE types appear? | `app/` freely. `bf_io`/`bf_engine` `.cpp` files privately. Never in any `bf_*` public header. |
| Where does realtime code live? | `bf_engine/playback.cpp` and `bf_dsp` mixers/crossfades only. Functions are marked `// [RT]` ([CODING_STANDARDS.md](CODING_STANDARDS.md) §3). |
| Where do heuristic constants live? | One `constexpr` header per module (`bf_analysis/params.h`, `bf_dsp/params.h`), each value commented with the doc/test that justifies it. |
| Where are interfaces? | Exactly two: `ITimeStretcher` (`bf_dsp/stretch.h`) and `IBeatAnalyzer` (`bf_analysis/analyzer.h`). Everything else is a concrete type or a free function. |
| Where do test signals come from? | Generated by `bench/gen` (Python) for L3/L4, or by C++ generator helpers in `tests/dsp/` for L2. Generated audio is never committed. |
| Where is golden audio? | Metrics/warp/hashes in `tests/golden/`. The ≤ 20 FLAC clips (≤ 15 s each) are committed only if they are Tier S/M renders; Tier C audio is fetched by hash (open question §4). |
| Where is the model? | Not in git. `models/manifest.json` pins it by SHA-256; the build fetches it ([BUILD_AND_DEPENDENCIES.md](BUILD_AND_DEPENDENCIES.md) §5). |
| Where are third-party sources? | Downloaded by FetchContent into the build tree. `third_party/` holds only records (manifest, licence texts, policy). No vendored copies unless a library has no stable download (then `third_party/<name>/src/`, unmodified). |

Dependencies point downward only: `app → bf_engine → {bf_io, bf_analysis} → bf_dsp`. `cli` depends on `bf_engine`, never on `app`.

## 3. The `cli` target in the MVP

DECISION_SUMMARY §4 excludes a user-facing CLI from the MVP. The QA plan, though, drives golden and benchmark runs through a console executable (`analyze`, `render --warp-json`, `export`, `dump-warp`, `--threads N`, `--seed`, `--profile`; [QA_STRATEGY.md](../07_quality/QA_STRATEGY.md), [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) §Implementation Implications). Resolution: `src/cli` builds `bftool.exe` from Phase 1 as a **dev-only** binary. It is not installed, not in the zip, and its flags are not a stable interface. Shipping it to users is **Post-MVP** (same target, promoted). One target, no second "dev tool".

## 4. Naming

| Item | Convention | Example |
|---|---|---|
| Directories, source files | `snake_case` | `render_plan.cpp` |
| CMake targets | `bf_<module>`, executables `beatfix`, `bftool`, tests `bf_<module>_tests` | `bf_dsp_tests` |
| Namespaces | `bf::dsp`, `bf::analysis`, `bf::io`, `bf::engine` | |
| Test files | `<unit>_test.cpp`, Catch2 tags `[unit]`, `[dsp]`, `[golden]`, `[slow]` | `warp_test.cpp` |
| Golden ids | `GOLD-NN` as in DSP_TEST_STRATEGY §5 | `tests/golden/GOLD-02/` |
| Corpus ids | `<tier>-<category>-<nnn>` | `C-EDM-007` |
| C API symbols | `bf_` prefix, plain C types | `bf_session_open` |
| Branches | `main` (protected), `feat/…`, `fix/…`, `release/x.y` | |

Code-level naming (types, functions, unit suffixes) is in [CODING_STANDARDS.md](CODING_STANDARDS.md) §6.

## 5. Mapping to QA document paths
QA_STRATEGY/BENCHMARK_CORPUS name `tools/gen/`, `tools/eval/`, `corpus/manifest/`, `goldens/`. This layout moves them to `bench/gen/`, `bench/eval/`, `bench/corpus/manifest/`, `tests/golden/` so that all Python lives in one dev-only tree and all test data under `tests/`. The content is unchanged.

## 6. Open questions
1. Storage for Tier C audio (≥ 30 CC BY/CC0 tracks): Git LFS quota vs a private release-asset bucket fetched by SHA-256. Recommendation: release assets (no LFS dependency in clones); owner to decide.
2. Monorepo vs separate private corpus repo for annotations (annotations are small; keep in `bench/corpus/` unless licence attribution volume grows).
