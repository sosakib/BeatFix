# Build and Dependencies

Status: 2026-10-08, v1.0. Merges brief §30 BUILD_SYSTEM + DEPENDENCY_MANAGEMENT (DECISION_SUMMARY §7). Decision record: [ADR-013](../09_decisions/ADR-013-build-packaging-distribution.md). Versions: [TECH_STACK.md](TECH_STACK.md) §1.

## 1. Toolchain

| Tool | Version | Why |
|---|---|---|
| MSVC | **19.4x** (VS 2022 v143 toolset), x64 only | Primary Windows compiler; JUCE, ORT and LAME are all built/tested with it |
| CMake | **≥ 3.28** | Presets v6+, `FetchContent` with `URL_HASH`, `EXCLUDE_FROM_ALL`/`SYSTEM` on `FetchContent_Declare` |
| Generator | Ninja Multi-Config | Faster CI than the VS generator; one tree for Debug/Release. Needs the MSVC dev environment (`vcvars64`) |
| vcpkg | manifest mode, `builtin-baseline` pinned | Only for `mp3lame` (no good CMake build upstream) |
| Python | 3.11+ (dev only) | `bench/`, `models/export_beat_this.py`, `tools/*.py` |
| clang-format / clang-tidy | LLVM version shipped with the pinned VS 2022 clang component; record `--version` in CI log | Avoid format churn from version drift (exact version **UNVERIFIED** until the runner image is checked) |

CI fails if `CMAKE_CXX_COMPILER_VERSION` is outside 19.40–19.49, so a runner image change cannot silently change the compiler.

## 2. CMake presets

```jsonc
// CMakePresets.json (abridged)
{
  "version": 6,
  "configurePresets": [
    { "name": "base", "hidden": true, "generator": "Ninja Multi-Config",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": { "VCPKG_TARGET_TRIPLET": "x64-windows",
                          "BF_WITH_RUBBERBAND": "OFF", "BF_WITH_BUNGEE": "OFF",
                          "BF_BUILD_TESTS": "ON", "BF_WERROR": "OFF" } },
    { "name": "dev",     "inherits": "base" },
    { "name": "ci",      "inherits": "base", "cacheVariables": { "BF_WERROR": "ON" } },
    { "name": "asan",    "inherits": "ci",   "cacheVariables": { "BF_ASAN": "ON" } },
    { "name": "release", "inherits": "base", "cacheVariables": { "BF_BUILD_TESTS": "OFF", "BF_WERROR": "ON" } },
    { "name": "dev-rb",  "inherits": "dev",  "cacheVariables": { "BF_WITH_RUBBERBAND": "ON" } }
  ],
  "buildPresets": [ { "name": "dev", "configurePreset": "dev", "configuration": "Debug" },
                    { "name": "ci",  "configurePreset": "ci",  "configuration": "RelWithDebInfo" },
                    { "name": "asan","configurePreset": "asan","configuration": "RelWithDebInfo" },
                    { "name": "release", "configurePreset": "release", "configuration": "Release" } ],
  "testPresets":  [ { "name": "ci", "configurePreset": "ci", "configuration": "RelWithDebInfo",
                      "output": { "outputOnFailure": true }, "execution": { "noTestsAction": "error" } } ]
}
```

Options (the only four; add more when a second value is actually needed):

| Option | Default | Effect |
|---|---|---|
| `BF_WITH_RUBBERBAND` | OFF | Builds `stretch_rubberband.cpp` + Rubber Band. **GPL**: the `release` preset hard-errors if ON (`message(FATAL_ERROR)`) |
| `BF_WITH_BUNGEE` | OFF | Builds the Bungee fallback adapter (MPL-2.0, unmodified) |
| `BF_ASAN` | OFF | `/fsanitize=address`, disables `/GL` and incremental linking |
| `BF_WERROR` | OFF | `/WX` on our targets (never on third-party targets) |

`BF_BUILD_TESTS` is the standard test toggle. Debug builds of Signalsmith are ~10× slower [V README]; the `dev` preset compiles `bf_dsp` with `/O2` even in Debug (per-target `COMPILE_OPTIONS`) so DSP tests stay fast. ponytail: per-target optimisation override; split a "dsp-debug" preset only if stepping through DSP code becomes routine.

## 3. Compiler and linker flags

Applied to our targets only (third-party targets keep their own flags, `SYSTEM` includes so their warnings don't fail `/WX`).

| Flag | Where | Why |
|---|---|---|
| `/std:c++20 /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8` | all | Conformance; `__cplusplus` correct for JUCE/nlohmann checks; UTF-8 source and execution charset |
| `/W4` (+ `/WX` when `BF_WERROR`) | all | |
| `/EHsc` | all | C++ exceptions enabled (third-party code throws); our policy on throwing is in CODING_STANDARDS §4 |
| `/fp:precise`, **no** `/fp:fast`, **no** `/fp:contract` | all | Determinism D1/D2 ([DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) §6); fast-math only after null tests prove equivalence |
| no `/arch:AVX2` | all (MVP) | Baseline x64 (SSE2) runs on every Windows 10/11 x64 PC. Revisit only if Phase 0 render RTF misses target; then add a startup CPU check |
| `/O2 /GL` + `/LTCG` | Release | |
| `/Zi` + `/DEBUG:FULL /OPT:REF /OPT:ICF` | Release, RelWithDebInfo | PDBs for every shipped binary (crash-dump symbolication) |
| `/guard:cf`, `/DYNAMICBASE`, `/HIGHENTROPYVA`, `/NXCOMPAT` | exe + our DLLs | Standard hardening; decoders parse untrusted files |
| `/DELAYLOAD:<lame dll>` + `delayimp.lib` | `beatfix.exe` | App starts without the LAME DLL; MP3 export reports "MP3 encoder missing" instead of the process failing to load. Also keeps the LGPL DLL user-replaceable |
| `/Brepro`, `/PDBALTPATH:%_PDB%` | Release | Reproducibility (§8). `/Brepro` is undocumented → **verify** effect on the pinned toolset |

JUCE module config (compile definitions on the JUCE-linking targets): `JUCE_USE_FLAC=1`, `JUCE_USE_OGGVORBIS=1`, `JUCE_USE_MP3AUDIOFORMAT=0` (minimp3 is the one MP3 path), `JUCE_USE_LAME_AUDIO_FORMAT=0` (it shells out to an exe), `JUCE_USE_WINDOWS_MEDIA_FORMAT=0` (no AAC in MVP), `JUCE_WEB_BROWSER=0`. Flag names are from JUCE 8 docs; re-check against 9.0.3 at pin time.

## 4. Dependencies: FetchContent pins

All source dependencies are declared in **one file**, `cmake/Deps.cmake`, as release/tag archives with SHA-256. `GIT_TAG` is not used, because a tag can be moved and git fetches cannot be hash-checked.

```cmake
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)   # never re-download a populated dep

FetchContent_Declare(juce
  URL      https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.3.tar.gz
  URL_HASH SHA256=<pin>                     # computed when pinning, reviewed in the PR
  SYSTEM EXCLUDE_FROM_ALL)
FetchContent_Declare(signalsmith_stretch
  URL      https://github.com/Signalsmith-Audio/signalsmith-stretch/archive/refs/tags/1.4.0.tar.gz
  URL_HASH SHA256=<pin>  SYSTEM)
FetchContent_Declare(onnxruntime
  URL      https://github.com/microsoft/onnxruntime/releases/download/v1.30.0/onnxruntime-win-x64-1.30.0.zip
  URL_HASH SHA256=<pin>)                    # official prebuilt, ~82.6 MB zip [V]; imported as IMPORTED SHARED target
# r8brain-free-src (version-6.5), libebur128 (v1.2.6), minimp3 (commit archive),
# nlohmann_json (v3.12.0), Catch2 (v3.16.0, tests only): same pattern.
FetchContent_MakeAvailable(juce signalsmith_stretch onnxruntime ...)
```

| Dependency | Source | Integration |
|---|---|---|
| JUCE 9.0.3 | tag archive | `add_subdirectory` via FetchContent; `juce_add_gui_app(beatfix …)` |
| Signalsmith Stretch 1.4.0 | tag archive | header-only `INTERFACE` target |
| r8brain-free-src version-6.5 | tag archive | small static lib target we define (`r8bbase.cpp` + headers) |
| libebur128 v1.2.6 | tag archive | small static lib target we define (one `.c`) |
| minimp3 | commit archive (no tags) | header-only `INTERFACE` target |
| nlohmann/json v3.12.0 | release archive | upstream CMake target |
| ONNX Runtime 1.30.0 | official prebuilt zip | `IMPORTED SHARED` target (`include/`, `lib/onnxruntime.lib`, `onnxruntime.dll`) |
| Catch2 v3.16.0 | tag archive | upstream CMake target, `BF_BUILD_TESTS` only |
| Rubber Band v4.0.0 | tag archive | only when `BF_WITH_RUBBERBAND=ON` |
| Bungee v2.4.30 | tag archive + submodule archives (Eigen, PFFFT) | only when `BF_WITH_BUNGEE=ON` |

**ORT from the prebuilt zip, not vcpkg:** the vcpkg port builds ORT from source (long CI times) and lagged at 1.23.2 ([ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) F10).

**Archive stability risk:** GitHub-generated tag archives have changed bytes in the past, which breaks the hash. Mitigation: a hash mismatch fails the build loudly (never silently re-fetches). If it happens, mirror the original archive to our private release-asset bucket and point the URL there.

**Bumping a dependency:** one PR that changes `Deps.cmake` + `third_party/manifest.json` + licence text if changed; CI runs the full nightly set on that PR (label `deps`).

## 5. vcpkg: LAME only

```json
// vcpkg.json
{ "name": "beatfix", "version-semver": "0.1.0",
  "dependencies": [ "mp3lame" ],
  "builtin-baseline": "<vcpkg commit sha>",
  "overrides": [ { "name": "mp3lame", "version": "3.100" } ] }
```

- Triplet `x64-windows` (dynamic CRT, **DLL** linkage) so LAME is a separate DLL as the LGPL requires (dynamic link, user-replaceable). The DLL file name vcpkg produces is recorded in `third_party/manifest.json` and in the DLL allow-list; it is never renamed.
- LGPL source offer: the release pipeline archives the exact LAME source + the vcpkg port patches used ([RELEASE_STRATEGY.md](RELEASE_STRATEGY.md) §6).
- Binary cache: `VCPKG_BINARY_SOURCES=clear;files,<cache dir>,readwrite` in CI (GitHub Actions cache), so LAME builds once per baseline.
- If the minimp3 MP3-decode gate fails in Phase 1, `mpg123` is added here the same way (DLL).

## 6. Model file handling

The Beat This ONNX file (~78 MB for `final0` [V]) is a build input, not source code. **Not in git, not in Git LFS** (no LFS quota, no LFS dependency for clones), unless the owner later decides otherwise.

1. `models/export_beat_this.py` loads the official `final0` checkpoint (Beat This v1.1.0), exports ONNX (fixed opset, recorded in the manifest), and verifies against PyTorch on a fixed clip set: max |logit diff| ≤ 1e-4 and identical decoded beat frames on all clips (third-party exports report ~1e-5 and bit-identical beats [Med], [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) F8).
2. The verified `.onnx` is uploaded once as an asset of a private GitHub release `models-v<N>`.
3. `models/manifest.json` records: id (`beatthis-final0-onnx-op<N>`), file name, **SHA-256**, size, checkpoint URL + commit, export script hash, opset, licence (MIT), training-data caveat (**for legal review**).
4. CMake fetches it with `file(DOWNLOAD … EXPECTED_HASH SHA256=…)` into the build tree; the installer copies it to `{app}\models\`.
5. **At runtime** the app computes the SHA-256 of the model before creating the ORT session (≈ 0.2 s for 78 MB with SHA-NI; acceptable once per launch, done on the analysis worker). Mismatch or missing file → analysis uses the DSP tracker, result marked "DSP mode", user told why ([ADR-003](../09_decisions/ADR-003-beat-analysis-strategy.md)).
6. The model id + hash is persisted with every analysis ([ADR-010](../09_decisions/ADR-010-project-storage.md)).

Only one model ships (full). If the Phase 3 performance gate fails, `small0` replaces it via the same procedure; shipping both is not planned.

## 7. Sanitizers and runtime checks

| Check | Build | Scope | Notes |
|---|---|---|---|
| AddressSanitizer `/fsanitize=address` | `asan` preset (RelWithDebInfo, no `/GL`) | unit + DSP tests, decoder fuzz, project-parser fuzz | Prebuilt `onnxruntime.dll` is not instrumented; that is accepted |
| libFuzzer `/fsanitize=fuzzer` | `asan` preset | MP3 decode, WAV/AIFF/FLAC header parsing, `.beatfix` JSON parser | MSVC libFuzzer support: confirm on the runner; time-boxed 30 min nightly ([QA_STRATEGY.md](../07_quality/QA_STRATEGY.md) L6) |
| RT allocation guard | Debug + RelWithDebInfo | audio callback | `thread_local bool tl_inRtCallback` set on callback entry; replaced global `operator new` asserts when set. ~20 lines, catches the most common RT violation ([CODING_STANDARDS.md](CODING_STANDARDS.md) §3) |
| Clang RealtimeSanitizer | — | — | Windows support **UNVERIFIED**; not in MVP |
| UBSan | — | — | Not available with MSVC; skipped |

## 8. Reproducible builds

Goal: the same tag built twice on the CI image yields byte-identical **unsigned** binaries. Signatures and timestamps legitimately differ, so comparison happens before signing.

- All sources pinned by SHA-256 (§4–6); vcpkg pinned by baseline; compiler version range enforced (§1).
- No `__DATE__`/`__TIME__` in our code; version string injected from the git tag by CMake.
- `/Brepro` + `/PDBALTPATH:%_PDB%` to remove PE timestamps and absolute PDB paths (verify on toolset).
- Absolute source paths in `__FILE__`/PDBs: build in a fixed directory on CI (`D:\a\bf\src`) so paths are constant. ponytail: fixed build path instead of path-mapping flags; add mapping only if local reproduction is needed.
- The release job builds twice and diffs SHA-256 of unsigned outputs. A mismatch is a warning in MVP (not blocking) until the sources of nondeterminism are known.
- Runtime determinism (identical audio output) is a separate, tested property ([DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) §6).

## 9. Runtime files shipped

```
BeatFix.exe                  (JUCE + engine, static)
onnxruntime.dll              (MIT; Microsoft-signed: verify, do not re-sign if already signed)
<lame>.dll                   (LGPL, unrenamed, delay-loaded)
models\beatthis-final0.onnx  (MIT, hash-checked at load)
THIRD_PARTY_NOTICES.txt, LICENSE-LGPL.txt, sbom.spdx.json
```
MSVC runtime: the app uses `/MD` (dynamic CRT), because ORT and the vcpkg LAME DLL use it too; mixing CRTs across DLL boundaries is avoided. The installer ships the VC++ runtime DLLs app-locally, which Microsoft's redistribution terms allow for the listed files (**verify the list** for VS 2022). A static `/MT` app was rejected because it would still need the dynamic CRT for the two DLLs.

## 10. Open questions
1. Exact LLVM version for clang-format/clang-tidy on the CI image.
2. Whether `/Brepro` is sufficient for bit-identical PE output with `/GL /LTCG`.
3. App-local VC++ runtime file list for VS 2022 v143 (vs requiring the VC++ redistributable installer, which needs admin rights and breaks the per-user install).
