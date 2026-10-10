# Tech Stack

Status: 2026-10-08, v1.0. Synthesis of [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1 and [LIBRARY_EVALUATION.md](../03_research/LIBRARY_EVALUATION.md). If this file and DECISION_SUMMARY disagree, DECISION_SUMMARY wins. Licence statements are engineering findings, **not legal advice**. Items marked **for legal review** need counsel before a commercial release ([LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md)).

Tags carried over from research: **[V]** verified against a primary source on 2026-10-08, **[S]** secondary source, **UNVERIFIED** = confirm before relying on it.

---

## 1. Final stack (MVP)

"Linking" is how the component ends up in the shipped product. *Static* = compiled into `BeatFix.exe`. *DLL* = separate file next to the exe. *Dev-only* = never in an installer or zip.

| Role | Component | Exact pin | Licence (SPDX) | Linking | Fallback | ADR |
|---|---|---|---|---|---|---|
| Language | C++20 (MSVC 19.4x, `/std:c++20`) | — | — | — | — | [ADR-001](../09_decisions/ADR-001-desktop-framework-and-language.md) |
| GUI, audio device (WASAPI), WAV/AIFF/FLAC/Ogg read, WAV/FLAC write | **JUCE** | tag `9.0.3` [V] (2026-09-28) | JUCE 9 EULA, Starter tier (AGPL-3.0 option **never** used) | Static (source modules) | Tauri 2 shell over the same C API | ADR-001 |
| FLAC + Ogg Vorbis codecs | libFLAC / libvorbis as **bundled inside JUCE** | version = whatever JUCE 9.0.3 bundles (**UNVERIFIED**, read `JUCE.spdx.json` at pin time) | BSD-3-Clause | Static (via JUCE) | — | [ADR-009](../09_decisions/ADR-009-codecs-resampling-export.md) |
| MP3 decode | **minimp3** (`minimp3_ex.h`) | commit SHA (no release tags; repo active 2026-07 [V]) | CC0-1.0 | Static (header) | libmpg123 (LGPL-2.1) as DLL, gapless on | ADR-009 |
| MP3 encode | **LAME** (`libmp3lame`) via vcpkg port `mp3lame` | `3.100` (vcpkg port [V]; upstream release date **UNVERIFIED**) | LGPL-2.0-or-later (**confirm in source**) | **DLL**, delay-loaded, unrenamed | none (MP3 export disabled if DLL missing) | ADR-009 |
| Loudness / true peak | **libebur128** | tag `v1.2.6` [V] | MIT | Static | own BS.1770 implementation | ADR-009 |
| Sample-rate conversion (export SRC, Resample mode, analysis 22.05 kHz) | **r8brain-free-src** | tag `version-6.5` [V] | MIT | Static (header + 1 TU) | own polyphase SRC ([RESAMPLING.md](../04_algorithms/RESAMPLING.md)) | ADR-009 |
| Time-stretch "Smooth" | **Signalsmith Stretch** | tag `1.4.0` [V] (vcpkg lags at 1.1.0 → do not use vcpkg) | MIT | Static (header-only) | Bungee `v2.4.30` (MPL-2.0, unmodified) | [ADR-006](../09_decisions/ADR-006-time-stretch-architecture.md) |
| Beat/downbeat activations | **Beat This!** checkpoint `final0` exported to ONNX by our script | upstream `v1.1.0` [V]; our ONNX file pinned by SHA-256 | MIT (code + weights); training-data caveat **for legal review** | Data file (`models/`) | `small0` if Phase 3 perf gate fails; DSP tracker if model unusable | [ADR-003](../09_decisions/ADR-003-beat-analysis-strategy.md) |
| Inference runtime | **ONNX Runtime**, CPU EP, official prebuilt `onnxruntime-win-x64-1.30.0.zip` | `1.30.0` [V] (2026-09-10) | MIT | **DLL** (`onnxruntime.dll`, Microsoft-signed: verify) | built-in DSP tracker | ADR-003 |
| JSON (project, warp map) | **nlohmann/json** | `v3.12.0` (version per vcpkg port listing [V]; confirm upstream tag at pin) | MIT | Static (header) | — | [ADR-010](../09_decisions/ADR-010-project-storage.md) |
| Hashing (fingerprints, cache keys, model check) | **Windows CNG** `BCryptHash` SHA-256 | OS | OS component | System DLL | — | ADR-010 |
| Unit/DSP tests | **Catch2 v3** | `v3.16.0` (per vcpkg port listing; confirm upstream tag) | BSL-1.0 | Dev-only | — | [ADR-012](../09_decisions/ADR-012-testing-strategy.md) |
| Benchmark harness | Python 3 + **mir_eval**, numpy, scipy, soundfile; PyTorch + `beat_this` for ONNX export/verification | pinned in `bench/requirements.txt` with hashes | mir_eval MIT; others BSD/MIT/Apache (soundfile bundles LGPL libsndfile: dev-only, fine) | Dev-only | — | ADR-012 |
| Quality reference stretcher | **Rubber Band** R3 (`setKeyFrameMap`) | tag `v4.0.0` [V] | **GPL-2.0-or-later** | **Dev/test builds only**, `BF_WITH_RUBBERBAND=OFF` by default, never in a package | commercial licence (£590 / £1,490) if listening tests justify it | ADR-006 |
| Build | CMake ≥ 3.28, FetchContent, vcpkg (manifest mode, `mp3lame` only) | vcpkg `builtin-baseline` commit pinned | BSD-3 (CMake), MIT (vcpkg) | Tooling | vcpkg manifest for all deps | [ADR-013](../09_decisions/ADR-013-build-packaging-distribution.md) |
| Installer | **Inno Setup 6** (per-user) + portable zip | 6.x, exact version recorded in CI (**UNVERIFIED** which 6.x is current) | Inno Setup licence (free; commercial users *asked* to buy since 6.5 [V]) | Tooling | MSIX via Microsoft Store (Phase 2) | ADR-013 |

What is deliberately **not** in the stack (each was considered, see §3): Qt, Tauri (fallback only), dr_libs, libsndfile, FFmpeg, miniaudio, libsamplerate, soxr, madmom, Essentia, aubio, BeatNet, xxHash, spdlog, sentry-native, Conan, CPM, WiX.

### 1.1 Module ↔ dependency map

| Module | May link | Must not link |
|---|---|---|
| `bf_dsp` | std, Signalsmith, r8brain, libebur128, (Rubber Band / Bungee behind flags) | JUCE, ORT, OS headers |
| `bf_analysis` | std, `bf_dsp`, ONNX Runtime | JUCE |
| `bf_io` | std, `bf_dsp`, JUCE `juce_audio_formats` (private, in `.cpp` only), minimp3, LAME | JUCE in public headers |
| `bf_engine` | everything above, nlohmann/json, Windows API | JUCE in public headers |
| `app` | `bf_engine`, all JUCE GUI/device modules | — |

Rule: no JUCE type appears in any public header of `bf_*`. That keeps the Tauri fallback real. With Tauri, `bf_io`'s JUCE-backed WAV/AIFF/FLAC/Ogg readers and the JUCE WAV/FLAC writers would have to be replaced (dr_libs + libFLAC per LIBRARY_EVALUATION). That is the known cost of the fallback.

### 1.2 Version facts that differ from the original brief
- The brief says "JUCE 8". JUCE 9.0.3 is current. The JUCE 9 EULA tiers are reported unchanged from JUCE 8 [S forum]. The official tier page could not be fetched → **re-verify prices before purchase** ([LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) §2).
- ONNX Runtime official Windows builds emit TraceLogging telemetry by default. `Ort::Env::DisableTelemetryEvents()` is called at startup ([ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) F4).

---

## 2. Licence policy (enforced in CI)

Source: [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) "Licence policy for the repo". The CI licence gate ([CI_CD.md](CI_CD.md) job `licence`) fails the build on any DENY or unknown entry.

```
ALLOW, static or vendored:     MIT, MIT-0, BSD-2-Clause, BSD-3-Clause, ISC, Zlib, Apache-2.0,
                               Unlicense, CC0-1.0, BSL-1.0
ALLOW, unmodified only:        MPL-2.0   (Bungee, if ever enabled; adapters live in our own files)
ALLOW, DLL only + source offer: LGPL-2.0+/2.1+   (LAME; libmpg123 only if the minimp3 fallback is used)
REQUIRE LICENCE RECORD:        JUCE EULA tier (Starter while ≤ US$20k revenue/funding),
                               Rubber Band commercial (only if ever shipped)
DENY:                          GPL-*, AGPL-*, CC-BY-NC-*, CC-BY-SA (code/weights), SSPL,
                               fdk-aac, FFmpeg built with --enable-gpl or --enable-nonfree,
                               anything without a licence file (e.g. Audacity StaffPad sources)
DEV-ONLY EXCEPTION:            Rubber Band (GPL) in BF_WITH_RUBBERBAND=ON dev/test builds.
                               The gate checks that no release artifact links it.
```

### 2.1 Traps (GPL / AGPL / NC), and what we do instead

| Trap | Why it is a trap | Our position |
|---|---|---|
| JUCE under AGPL-3.0 | Shipping closed-source under AGPL is a licence violation | JUCE only under the EULA; tier recorded in `third_party/manifest.json` |
| Rubber Band (GPL-2.0+) | GPL linking forces GPL on BeatFix | Dev/test reference only; commercial licence is the only shipping path |
| Audacity StaffPad TimeAndPitch | Files have no header → GPLv2+ by Audacity's licence rule [V] | Never copied, never read for implementation |
| madmom models | CC BY-NC-SA 4.0 [V]; madmom code is BSD but the shipped models are NC | Never bundled; no weights derived from them |
| BeatNet | CC BY 4.0, but needs madmom ≥ 0.16.1 [V] → drags in NC models + Python | Not used |
| Essentia (AGPL-3.0), aubio (GPL-3.0) | Copyleft | Not used |
| FFmpeg `--enable-gpl` / `--enable-nonfree`, fdk-aac | GPL or non-redistributable | Not used in MVP |
| TSM Toolbox, PyTSMod, LTFAT/phaseret | GPL reference implementations | Papers only; no code copied |
| `rubberband-sys` Rust crate | GPL | Not used |
| Beat This! training data | Weights MIT, but README warns some training data is copyrighted / limited CC [V] | **For legal review** before commercial release; model hash + source recorded in SBOM |
| madmom-style DBN re-implementation | Single low-quality source claims a patent | Not implemented; own Ellis-style DP decoder instead ([ADR-003](../09_decisions/ADR-003-beat-analysis-strategy.md)) |
| AAC (encode or decode) | Via LA patent pool, per-unit royalties [V] | Not in MVP |
| Transient copy/reinsert (US 9,230,558 B2, expiry ~2029) | Possible patent read on "Slice" mode | Slice not in MVP; **for legal review** before any transient-copy mode |

---

## 3. Library decision matrix

Distilled from [LIBRARY_EVALUATION.md](../03_research/LIBRARY_EVALUATION.md) §Decision Matrix and aligned with DECISION_SUMMARY (which overrides LIBRARY_EVALUATION on decode and WAV write: JUCE readers + minimp3 instead of dr_libs). Ratings H/M/L are engineering judgement from the research evidence. Q = quality, P = performance, API = API maturity. "Commercial" = usable in a closed-source commercial product.

| Purpose | Candidate (chosen) | Alternatives | Q | P | API | Licence | Commercial | Platform | Maintenance | Build cx | Recommendation | Fallback |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Engine language | C++20, no JUCE in engine | Rust core | — | H | — | — | — | All | — | M | **Adopt**: every quality TSM library and ORT are C/C++; no FFI on the render path | Rust core over FFI |
| App shell / UI | **JUCE 9.0.3** (EULA) | Tauri 2.12, Qt 6.12 LGPL, Dear ImGui, wxWidgets, WPF+NAudio | H | H | H | JUCE EULA / AGPL | Yes, EULA only (Starter free ≤ $20k; Indie $800 perpetual [S]) | Win/mac/Linux | H (monthly patches) | L–M | **Primary**. Weighted score 159 vs ImGui 144, Qt 139, Tauri 137 ([ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §1) | **Tauri 2** |
| Audio device I/O | JUCE `AudioDeviceManager` (WASAPI) + own transport | miniaudio, cpal | H | H | H | JUCE | Yes | Win | H | L | **Primary** (already a dependency) | miniaudio (with Tauri) |
| Decode WAV/RF64/AIFF/FLAC/Ogg | **JUCE readers** | dr_libs, libsndfile (LGPL DLL), Symphonia | H | H | H | JUCE / BSD-3 | Yes | All | H | L | **Primary**. Already linked; one less dependency | dr_libs + libFLAC |
| Decode MP3 | **minimp3 `minimp3_ex`** + LAME/Xing gapless trim | JUCE `MP3AudioFormat`, dr_mp3, libmpg123 | H | H | M | CC0 | Yes | All | M | L | **Primary**, gated by click-track test (0-sample offset) in Phase 1 | **libmpg123 DLL** (LGPL), gapless on |
| Decode M4A/AAC/ALAC | — | Windows Media Foundation, FFmpeg LGPL | H | H | M | OS / LGPL | AAC patents **for legal review** | Win | H | M | **Post-MVP** | — |
| Beat/downbeat model | **Beat This! (ONNX)** | own DSP tracker; Beat This small; madmom/Essentia/aubio/BeatNet excluded | H (SOTA 2024) | H on CPU | M (ports young) | MIT (+ data caveat) | Yes, after legal review of data caveat | All | M | M (own export + mel front-end parity) | **Primary**: full model; small if perf gate fails | DSP tracker (HPSS + SuperFlux → same decoder) |
| Inference runtime | **ONNX Runtime 1.30 CPU EP** | rten, tract, Windows ML, DirectML EP | H | H | H | MIT | Yes | All | H | M (DLL ship) | **Primary**. DirectML is in sustained engineering → CPU only | DSP tracker (no runtime) |
| Time-stretch | **Signalsmith Stretch 1.4.0** | Bungee (MPL), Rubber Band R3 (GPL/paid), SoundTouch (LGPL), élastique (paid), StaffPad (GPL trap) | H (quality vs RB unconfirmed by our listening) | H | M–H | MIT | Yes | All | H | L (header-only) | **Primary**: native variable ratio via per-call in/out counts; per-bin linked stereo verified in source | **Bungee** (unmodified) |
| Time-stretch, premium | Rubber Band R3 | Bungee Pro, élastique | H | M | H | GPL / £590+ | Only with commercial licence | All | M (v4.0.0, slow) | M | **Dev/test reference only** | — |
| Resampling | **r8brain-free-src** | libsamplerate (97 dB, too low for 24-bit), rubato (Rust), soxr (LGPL, dormant), JUCE interpolators | H | H | M–H | MIT | Yes | All | H | L | **Primary**, wrapped with exact-length/zero-delay contract | own polyphase SRC |
| Loudness / true peak | **libebur128** | Rust `ebur128`, own BS.1770 | H | H | H | MIT | Yes | All | L (stable, spec frozen) | L | **Primary** | own implementation |
| Encode WAV 16/24/32f, RF64 | **JUCE** `WavAudioFormat` writer | dr_wav, libsndfile, own writer | H | H | H | JUCE | Yes | All | H | L | **Primary**. RF64 write behaviour of the JUCE writer **UNVERIFIED** → test > 4 GiB in Phase 7 | own RIFF/RF64 writer (~200 LOC) |
| Encode FLAC 16/24 | **JUCE** `FlacAudioFormat` (bundled libFLAC) | standalone libFLAC, libsndfile | H | H | H | BSD-3 | Yes | All | H | L | **Primary** | standalone libFLAC 1.5.0 |
| Encode MP3 | **LAME 3.100 DLL** + our `AudioFormatWriter` | JUCE `LAMEEncoderAudioFormat` (shells out to an exe: rejected), libsndfile→LAME, MF MP3 encoder (**UNVERIFIED** existence) | H | H | M (old API) | LGPL | Yes: DLL, unrenamed, notices, source offer | All | L (dormant) | M | **Primary** | none in MVP |
| Encode AAC/Opus/Ogg/AIFF | — | MF AAC-LC (16-bit, 96–192 kbps only), libopus | M–H | H | M | OS / BSD | AAC: legal review | — | — | M | **Post-MVP** | — |
| JSON | **nlohmann/json** | JUCE `var`/`JSON` (would put JUCE into the engine), RapidJSON, simdjson | H | M (files < 1 MB) | H | MIT | Yes | All | H | L | **Primary** | — |
| Hashing | **Windows CNG SHA-256** | xxHash (BSD-2), BLAKE3 | H | M–H (SHA-NI on modern CPUs) | H | OS | Yes | Win | H | L (no dependency) | **Primary**. Add XXH3 only if profiling shows hashing on the critical path | xxHash |
| Unit tests | **Catch2 v3** | GoogleTest | H | — | H | BSL-1.0 | n/a (dev) | All | H | L | **Primary** | GoogleTest |
| Benchmark metrics | **mir_eval** (Python) | own C++ metrics | H | — | H | MIT | n/a (dev) | All | M | L | **Primary**; metrics live only in Python (one source of truth, [QA_STRATEGY.md](../07_quality/QA_STRATEGY.md) Risks) | — |
| Installer | **Inno Setup 6** | WiX v6 (OSMF fee > $10k revenue [V]), NSIS, Velopack, MSIX | H | — | H | free | Yes | Win | H | L | **Primary** + portable zip | MSIX (Phase 2) |
| Universal import | — | FFmpeg 8.1 LGPL DLLs | H | H | M | LGPL-2.1+ | Yes with full LGPL checklist | All | H | H | **Not in MVP** | — |

### 3.1 Rejections with the main reason
- **Qt 6**: LGPLv3 relink/installation-information duties or a subscription; no audio device/format stack; no owner experience.
- **Tauri 2 / Rust core**: three languages, FFI to C++ stretch libs, `ort` still RC (2.0.0-rc.13), young `signalsmith-stretch`/`beat-this` crates. Kept as the shell fallback because the engine is JUCE-free.
- **Dear ImGui**: no accessibility, no native dialogs. Allowed only for an internal debug viewer if one is ever needed (not planned).
- **dr_libs**: good, but JUCE readers are already linked (DECISION_SUMMARY §6).
- **libsndfile**: LGPL DLL for things JUCE already does; slow release cadence (1.2.2, 2023).
- **libsamplerate**: 97 dB SNR, below the 24-bit noise floor.
- **xxHash / spdlog / sentry-native**: not needed yet. SHA-256 is fast enough; `juce::FileLogger` covers logging; crash dumps use `MiniDumpWriteDump` ([LOGGING_AND_DIAGNOSTICS.md](../05_architecture/LOGGING_AND_DIAGNOSTICS.md)).

---

## 4. Paid-upgrade path (not budgeted for MVP)
1. JUCE Indie ($800 perpetual/seat [S]) once revenue or funding exceeds US$20k. Pro ($3,500 perpetual) above $300k.
2. Rubber Band commercial: £590 (prominent credit) or £1,490 non-attribution for < 10 staff [V]. Buy only if blind listening tests show R3 beats Signalsmith on the QA corpus ([SUBJECTIVE_LISTENING_TESTS.md](../07_quality/SUBJECTIVE_LISTENING_TESTS.md)).
3. Bungee Pro / zplane élastique: price on request. Only if both MIT/MPL engines and R3 fail listening tests.

## 5. Open questions
1. JUCE 9 EULA tier/price confirmation and the definition of "revenue/funding" pre-launch (**for legal review**).
2. Beat This training-data caveat (**for legal review**).
3. Exact files ORT 1.30 needs for CPU-only (`onnxruntime.dll` alone, or also `onnxruntime_providers_shared.dll`): read `dumpbin /dependents` at pin time.
4. Whether JUCE 9.0.3's WAV writer emits RF64 for > 4 GiB outputs.
5. LAME licence version string in the 3.100 source tree (expected LGPL-2.0-or-later).
