# Architecture Research — Stack, Threading, Render/Cache, Project Format, Build & Packaging

Agent H · 2026-10-08 · Inputs: full brief §16–19, §36–40; `_baseline_research_log.md` (not repeated here, only extended/verified).
Status: **research + proposal**. Orchestrator decides. Nothing here is final.

---

## Objective

Compare concrete architecture options for BeatFix (Windows-first, offline, single-purpose: decode → analyse (ONNX + DSP) → warp map → offline multithreaded time-stretch (Signalsmith Stretch MIT / Rubber Band) → A/B preview → export), then propose one layered architecture: modules, minimal interfaces, data flow, threading, render/cache, project format, memory budget, diagnostics, repo layout, build, CI, dependency management, coding standards, packaging, signing, updates.

## Why It Matters

- The realtime audio thread, the offline render pool and the UI must never block each other; this is decided by the stack and the threading contract, not by later tuning.
- The time-stretch libraries and ONNX Runtime are C/C++. Every non-C++ stack adds an FFI boundary on the hottest path (render) and a second build system.
- Licence costs/obligations (JUCE AGPL/commercial, Qt LGPL/commercial, LAME LGPL, Rubber Band GPL/commercial, WiX OSMF) are fixed at stack choice.
- Distribution on Windows is dominated by SmartScreen + code-signing rules that changed in 2024–2026.

---

## Key Findings

| # | Finding | Confidence |
|---|---|---|
| F1 | **Audio playback/A-B quality is stack-independent**: in all 4 options the audio callback lives in native code (C++ or Rust). The real differentiators are FFI count, UI dev speed, licence, and packaging. | High |
| F2 | **JUCE is now at 9.0.3 (2026-09-28)**; 8.0.15 was the last 8.x (2026-07-21). Brief says "JUCE 8". JUCE 9 EULA is new (`juce.com/legal/juce-9-licence/`); JUCE 9 tier prices **UNVERIFIED** (page would not render). JUCE 8 tiers: Starter free ≤ $20k revenue, Indie ≤ $300k $40/mo or $800 perpetual, Pro $175/mo or $3,500 perpetual; no splash screen in 8. Open-source alternative: AGPLv3. | High (8) / Low (9 prices) |
| F3 | JUCE API (Context7, `docs.juce.com/master`) confirms everything needed: `AudioDeviceManager::initialiseWithDefaultDevices(in,out)`, `AudioSourcePlayer`, `AudioTransportSource::setSource(src, readAheadBufferSize, TimeSliceThread*, sourceSampleRateToCorrectFor, maxNumChannels)`, `AudioFormatManager::registerBasicFormats()` + `createReaderFor(File)`, `WavAudioFormat::createMemoryMappedReader`, `ThreadPool::addJob(job, deleteWhenFinished)` + `ThreadPoolJob::runJob()/shouldExit()`, `AudioThumbnail` w/ `AudioThumbnailCache`, `MP3AudioFormat` (decode), `WindowsMediaAudioFormat` (Media Foundation → AAC/M4A decode on Windows), `FlacAudioFormat` (r/w), `LAMEEncoderAudioFormat` (shells out to a LAME exe). **Breaking change in current master**: writers are now `std::unique_ptr<AudioFormatWriter> createWriterFor(std::unique_ptr<OutputStream>&, const AudioFormatWriterOptions&)`; old raw-pointer overload deprecated. | High |
| F4 | ONNX Runtime upstream **1.30.0 (2026-09-10)**; C++ API: `Ort::Env`, `Ort::Session(env, path, opts)` (path is `ORTCHAR_T*` = `wchar_t*` on Windows — **Med**), `Ort::RunOptions::SetTerminate()` (cancel an in-flight `Run`). **Windows official ORT builds emit TraceLogging telemetry ON by default**; disable with `Ort::Env::DisableTelemetryEvents()`. Official win-x64 zip = 82.6 MB (includes libs/PDB/headers; shipped DLL smaller — size **UNVERIFIED**). | High |
| F5 | Rust `ort` is still **2.0.0-rc.13** (no stable 2.x). Context7 main-branch docs show API churn (`ort::init().build()?` → `Session::builder(&env)` vs rc.10 `Session::builder()?`). Default feature `download-binaries` fetches ORT binaries from pyke's CDN at **build** time (CI/offline-build concern; `load-dynamic` / `ORT_LIB_PATH` avoid it). `ort-tract` / `tract-onnx 0.23.8` = pure-Rust alternative. | High |
| F6 | `cpal` **0.18.2**: `build_output_stream<T>(config: StreamConfig /*by value*/, data_cb: FnMut(&mut [T], &OutputCallbackInfo), err_cb: FnMut(Error), timeout: Option<Duration>)`; buffer pre-filled with silence; `BufferSize::Fixed(n)` is a request, not a guarantee. Breaking vs 0.17 (`&StreamConfig`, `BuildStreamError`). | High |
| F7 | Tauri **2.12.1** stable; **3.0.0-alpha.4** exists (2026-10-01). Binary IPC verified: `tauri::ipc::Response::new(Vec<u8>)` returns ArrayBuffer without JSON; `tauri::ipc::Channel<&[u8]>` streams ordered chunks; `invoke(cmd, Uint8Array)` sends raw body (`InvokeBody::Raw`). Updater plugin: mandatory signature (keypair via `tauri signer generate`), Windows NSIS/MSI artifacts, install modes passive/basicUi/quiet. MSI bundler uses **WiX 3.14.1** (verified in `tauri-bundler` source). WebView2 bootstrapper options: download (0 MB), embed (~1.8 MB), offline (~127 MB), fixed (~180 MB). | High |
| F8 | Rust audio crates (crates.io, today): symphonia 0.6.1 (MPL-2.0), rubato 5.0.1 (MIT/Apache), rtrb 0.4.0 (SPSC), signalsmith-stretch 0.1.3 (MIT, last update 2025-09), ssstretch 0.1.0 (cxx bindings, 2025-03), beat-this 1.1.0 (MIT, 2026-10-02, very young), flacenc 0.5.1 (Apache), mp3lame-encoder 0.2.5 (**LGPL-3.0**). No maintained Rubber Band crate. | High |
| F9 | Signalsmith Stretch upstream tag **1.4.0**; API supports exact offline stretch of a fixed-length chunk: `presetDefault(ch, sr)`, `process(in, inN, out, outN)` with differing in/out sizes, `inputLatency()/outputLatency()`, `seek(in, n, rateHint)` for pre-roll, `flush(out, n)` for tail, `reset()`, optional `splitComputation`. README warns `-ffast-math` breaks on Apple Clang 16.0.0 only. This is what makes **chunk-parallel render** feasible. | High |
| F10 | vcpkg ports exist (checked raw `ports/*/vcpkg.json` on master): juce 8.0.7, onnxruntime 1.23.2, mp3lame 3.100, libflac 1.5.0, mpg123 1.33.7, signalsmith-stretch **1.1.0** (upstream 1.4.0), rubberband 4.0.0, xxhash 0.8.4, blake3, catch2 3.16.0, nlohmann-json 3.12.0, spdlog 1.17.0, sentry-native 0.17.1, readerwriterqueue, concurrentqueue, libsamplerate, soxr. No port: r8brain-free-src (MIT), farbot. **vcpkg lags upstream** on JUCE, ORT and Signalsmith. | High |
| F11 | Code signing: since 2024 **EV no longer grants instant SmartScreen reputation** (EV and OV build reputation by download volume). Since **2026-03-01** (CA/B Forum CSC-31) new code-signing certs max **460 days**. Microsoft **Artifact Signing** (ex-Trusted Signing): Basic/Premium SKU (5,000 / 100,000 signatures/mo), "starting at $9.99/mo" (pricing page renders "$-" today → **Med**); paid Azure subscription required; Public Trust for orgs in US/CA/EU/UK/AU/NZ/JP/KR/SG/CH/NO/IL; **individual developers only in US/Canada**; no EV. | High |
| F12 | **Microsoft Store**: individual developer registration free since 2025-09 (~200 markets), company registration free since 2026-05; MSIX packages are **signed and hosted by Microsoft for free** with Store-managed updates. Win32 full-trust apps accepted. This removes signing cost + in-app updater for the Store channel. | High |
| F13 | Installer licences: **Inno Setup** free; since 6.5 commercial users are *asked* (not strictly required) to buy a licence. **WiX v6+** requires the Open Source Maintenance Fee for orgs > $10k revenue ($10/mo ≤ 20 staff). NSIS zlib licence (free). Velopack MIT (installer + delta updates, C++/Rust SDKs). | High (Inno/WiX), Med (Velopack C++ SDK maturity) |
| F14 | Qt 6: LGPLv3 (dynamic linking + relinkability obligations) or commercial; Small Business tier requires < €1M annual revenue incl. funding, max 3 discounted dev seats; price not on the exhibit (third-party reports ~$500/yr — **Low**). | High (terms) / Low (price) |
| F15 | Owner context (from user memory, not research): the owner has shipped a **JUCE** audio plugin (Aureole shimmer reverb) and a **Tauri 2** desktop app (Notepad Super Plus). Both stacks are within existing skill. | Med |

---

## Technical Details

### 1. Option comparison

All four options share: native audio callback, Signalsmith Stretch (C++ header-only) as default stretcher, optional Rubber Band (C++, GPL/commercial) as second engine, ONNX Runtime for the beat model, offline-first.

#### (1) C++20 + JUCE 8/9 (GUI + audio device + formats) + CMake

- **Licence cost**: AGPLv3 (would force AGPL on BeatFix) or commercial: free ≤ $20k revenue; $800 perpetual Indie once > $20k (JUCE 8 tiers; JUCE 9 **UNVERIFIED**). Bundled third-party code inventoried in `JUCE.spdx.json` (verified in repo LICENSE.md) — **for legal review**.
- **Audio thread safety story**: strongest. `AudioIODeviceCallback::audioDeviceIOCallbackWithContext(...)` is a plain C++ callback; same language as stretch/ORT; full control of allocation. JUCE's own `AudioTransportSource` + `BufferingAudioSource` use a `TimeSliceThread` for read-ahead (disk I/O off the RT thread). Pitfalls: `juce::String`, `juce::var`, `MessageManagerLock`, `Logger` all allocate/lock → banned in callback.
- **Waveform performance**: Direct2D renderer on Windows (JUCE 8+; `FontOptions::withDirect2DHinting` doc confirms D2D path), optional `OpenGLContext::attachTo()`. `AudioThumbnail` is fine for the source lane; the corrected lane + intensity overlay needs a custom min/max LOD pyramid (trivial; see §6). Drawing O(pixel columns) per repaint → negligible cost at 4K.
- **UI dev speed**: slowest-to-medium. Everything custom-drawn (`Component::paint`, `LookAndFeel`). No layout designer. Owner already knows it. JUCE 8 also offers `WebBrowserComponent` with native integration (web UI inside JUCE) as an escape hatch (variant 1b) — not recommended for MVP (two UI paradigms).
- **Testability**: good if the DSP/engine core is kept free of `juce_gui_*`: pure C++ unit tests + JUCE console-app integration tests. Headless CI fine (JUCE 9 notes "improved CMake build system for headless environments").
- **Packaging size**: single exe ~5–15 MB (static JUCE) + `onnxruntime.dll` + model file (Beat This small ≈ 8 MB / final ≈ 78 MB checkpoint sizes per baseline) + `libmp3lame.dll`. **Estimate** ~25–110 MB installed depending on model; **UNVERIFIED** until built.
- **Installer**: any (Inno/MSIX/NSIS/WiX). CMake `install()` + CPack optional.
- **Signing / update**: standard Authenticode; no framework-imposed updater.
- **Team skills**: JUCE devs are a niche but the dominant pool for audio apps; owner has JUCE experience.
- **CLI/batch later**: trivial — `juce_add_console_app` linking the same core (juce_core + juce_audio_formats, no GUI).

#### (2) C++20 core + Qt 6 Widgets / QML

- **Licence**: LGPLv3 (dynamic link, ship Qt source offer/relink ability, notices) or commercial (Small Business < €1M revenue, price **UNVERIFIED**). **For legal review**.
- **Audio thread**: Qt Multimedia (`QAudioSink`) is not designed for low-latency sample-accurate A/B; you would add miniaudio (public domain/MIT-0) or RtAudio (MIT-like) or JUCE's audio modules (then you pay JUCE anyway). Audio core is plain C++ either way.
- **Formats**: none suitable built in → add libsndfile (LGPL) or dr_libs/miniaudio decoders + libFLAC + LAME + Media Foundation for AAC. More glue than JUCE.
- **Waveform**: Widgets `QPainter` raster is fine; QML needs `QQuickPaintedItem` or custom scene-graph geometry (more work, GPU-fast).
- **UI dev speed**: fastest of the C++ options (Designer, layouts, models, accessibility, HiDPI mature).
- **Testability**: good (QtTest, core separable).
- **Packaging**: `windeployqt`; Widgets ~20–35 MB, QML ~40–70 MB (**estimates**).
- **Team skills**: large Qt/C++ pool; fewer audio specialists. Owner: no stated Qt experience.
- **CLI**: easy (QtCore console or no Qt at all in core).
- **Net**: best UI toolkit in C++, but you assemble the audio/format stack yourself and take on LGPL compliance or a subscription.

#### (3) Rust core (cpal, symphonia, rubato, ort/tract, FFI to C++ stretch) + Tauri 2 webview UI

- **Licence**: Tauri/cpal/rubato MIT/Apache; symphonia MPL-2.0 (file-level copyleft, fine for closed app — **legal review**); LAME LGPL; ORT MIT. Cheapest licence position.
- **Audio thread**: cpal callback `FnMut(&mut [T], &OutputCallbackInfo)`; Rust ownership helps (no accidental sharing), `rtrb` SPSC, `arc-swap`/atomics for buffer swap. But: the stretcher (Signalsmith/Rubber Band) runs only offline here, so FFI is on the render pool, not the RT path — acceptable. Allocation in callback must still be policed manually (`assert_no_alloc` crate pattern).
- **FFI cost**: Signalsmith via `signalsmith-stretch` 0.1.3 / `ssstretch` 0.1.0 (both young, single-maintainer); Rubber Band needs hand-written bindings (`cc`/`cxx`) — C++ toolchain still required in the build → two toolchains, two package managers.
- **ONNX**: `ort` RC with API churn and build-time binary download; or `tract`/`rten` pure Rust (slower, op coverage risk — **UNVERIFIED** for Beat This graph; the `beat-this` crate claims rten or ort backends).
- **Waveform**: Canvas2D/WebGL in WebView2; ship only LOD peak arrays via `ipc::Response` (ArrayBuffer, no JSON). Playhead drawn from JS clock + periodic sync events. Performance fine if you never ship PCM to JS.
- **UI dev speed**: fastest overall (HTML/CSS/TS, component libraries, hot reload, accessibility via DOM). Owner knows Tauri 2.
- **Testability**: Rust core `cargo test` excellent; UI via Vitest + `mockIPC`, e2e via Playwright-for-Tauri. Two test stacks.
- **Packaging**: app 5–15 MB + WebView2 (preinstalled on Win11; bootstrapper modes F7) + ORT DLL + model. NSIS or MSI(WiX 3.14) from `tauri build`; updater plugin built in.
- **Team skills**: web devs abundant; Rust-audio devs rare.
- **CLI**: easy (`cargo` bin target sharing core crate).
- **Net**: best UI velocity and licence cost; pays with a 3-language system (Rust + TS + C++ for stretch libs), an IPC boundary for all editing state, and the least mature ML/stretch bindings.

#### (4) C++ core + Dear ImGui (MIT; + ImPlot MIT)

- **Licence**: MIT; plus whatever audio/format libs (as Qt option: miniaudio/dr_libs/libFLAC/LAME).
- **Audio thread**: plain C++, same as (1)/(2).
- **Waveform**: excellent for engineering views (ImDrawList per column, GPU backend D3D11). Continuous redraw by default (throttle when idle).
- **UI dev speed**: fastest for internal tools; poor for a polished consumer product: no native file dialogs (need nativefiledialog-extended), weak text/IME/accessibility (no screen-reader support), non-native look; design-system work is hard.
- **Testability / CLI**: good.
- **Packaging**: smallest.
- **Net**: ideal for an **internal debug/benchmark viewer** (warp-map inspection, golden-audio diff), not the shipped UI.

#### Scoring (1 = poor, 5 = best; weights reflect brief §16–19, §39)

| Criterion (weight) | (1) JUCE | (2) Qt | (3) Rust+Tauri | (4) ImGui |
|---|---|---|---|---|
| RT audio safety & single-language hot path (5) | 5 | 4 | 4 | 4 |
| Integration with C++ stretch libs + ORT, no FFI (5) | 5 | 5 | 2 | 5 |
| Built-in decode/encode/devices (4) | 5 | 2 | 3 | 2 |
| Waveform perf (3) | 4 | 4 | 4 | 5 |
| UI dev speed / polish (4) | 2 | 4 | 5 | 2 |
| Testability (3) | 4 | 4 | 4 | 4 |
| Packaging size/simplicity (2) | 4 | 3 | 4 | 5 |
| Licence cost/obligation (3) | 3 | 3 | 5 | 5 |
| Dependency maturity (3) | 5 | 5 | 2 | 4 |
| Owner/team skill fit (3) | 5 | 2 | 4 | 3 |
| CLI/batch later (2) | 5 | 5 | 5 | 5 |
| **Weighted total (/185)** | **159** | **139** | **137** | **144** |

Scores are judgement, not measurement. JUCE leads by 15–22 points; no single ±1 change flips the lead. Tauri overtakes JUCE only if the "UI dev speed" weight rises from 4 to ≥ 12 (Tauri gains 3 points per weight unit). ImGui scores second on paper but is excluded as a shipped consumer UI (accessibility, native dialogs, polish — see option 4).

### 2. Verified API notes used by the design

```cpp
// JUCE (docs.juce.com/master, Context7)
juce::AudioDeviceManager dm;  dm.initialiseWithDefaultDevices(0, 2);   // returns error String
juce::AudioSourcePlayer player; dm.addAudioCallback(&player);           // player.setSource(AudioSource*)
// AudioTransportSource::setSource(src, readAheadBufferSize, TimeSliceThread*, srcRateToCorrectFor, maxCh)
juce::AudioFormatManager fm;  fm.registerBasicFormats();                // WAV, AIFF + enabled (FLAC/Ogg/MP3/WMF)
std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor(file));  // caller owns
// WavAudioFormat::createMemoryMappedReader(File) -> MemoryMappedAudioFormatReader*  (for decoded cache)
// Writers (current API): std::unique_ptr<AudioFormatWriter>
//   createWriterFor(std::unique_ptr<OutputStream>&, const AudioFormatWriterOptions&)
// ThreadPool::addJob(ThreadPoolJob*, bool deleteJobWhenFinished); runJob() -> jobHasFinished|jobNeedsRunningAgain; shouldExit()

// ONNX Runtime C++ (onnxruntime.ai/docs/api/c)
Ort::Env env; env.DisableTelemetryEvents();                // Windows builds: telemetry ON by default
Ort::SessionOptions so; so.SetIntraOpNumThreads(n);
Ort::Session s(env, L"beat_this_small.onnx", so);           // ORTCHAR_T path on Windows
Ort::RunOptions ro; /* other thread: */ ro.SetTerminate();  // cooperative abort of Run()
```

```rust
// cpal 0.18.2
device.build_output_stream::<f32, _, _>(config /*StreamConfig by value*/,
    move |out: &mut [f32], _info: &cpal::OutputCallbackInfo| { /* RT */ },
    move |e: cpal::Error| { /* report */ }, None)?;
// tauri 2.x
#[tauri::command] fn peaks(lod: u32) -> tauri::ipc::Response { tauri::ipc::Response::new(bytes) }
#[tauri::command] async fn progress(ch: tauri::ipc::Channel<&[u8]>) { ch.send(&buf)?; }
// ort 2.0.0-rc.13 (main docs; API differs from rc.10 — pin exact version)
let env = ort::init().build()?;
let mut s = Session::builder(&env)?.with_intra_threads(4)?.commit_from_file("m.onnx")?;
let out = s.run(ort::inputs!["spec" => t])?;
```

### 3. Proposed architecture (for Option 1; module boundaries carry over to any option)

#### 3.1 Layers — revised from brief §17

The brief's 10-layer stack is too deep for a single-track app. Collapse to **4 libraries + 2 executables**; layers are folders, not processes or plugins.

```
apps/beatfix (JUCE GUI)        apps/beatfix-cli (JUCE console, Phase 2+)
        │                                 │
        └────────────┬────────────────────┘
                 bf_engine     (session, jobs, cache, playback engine, project I/O)  — juce_core, juce_audio_basics/devices/formats
                 ├── bf_io     (decode→PCM cache, export writers, LAME writer)       — juce_audio_formats, libmp3lame
                 ├── bf_analysis (beat model + DSP features, tempo hypotheses)        — onnxruntime, bf_dsp
                 └── bf_dsp    (warp map math, stretch adapters, resampler, peaks, crossfades) — std only + signalsmith(+rubberband)
```

Rules:
- `bf_dsp` is **pure C++20 + std**: `std::span<const float>`, no JUCE types → unit-testable, reusable in CLI, portable if UI stack changes.
- Only `apps/beatfix` links `juce_gui_*`. CLI = same `bf_engine` without GUI.
- Dependencies point downward only. UI talks to `bf_engine` through one façade (`Session`) and immutable snapshots.

#### 3.2 Interfaces — only where swapping is a real requirement

| Seam | Real requirement? | Form |
|---|---|---|
| `ITimeStretcher` | **Yes** — Signalsmith (MIT) default, Rubber Band R3 (GPL/£590+ commercial) optional, Bungee (MPL) candidate; licence may force swap. | virtual interface, factory by enum |
| `IBeatAnalyzer` | **Yes** — ONNX Beat This vs deterministic DSP fallback (brief §36 requires fallback & comparison). | virtual interface |
| Resampler | Swap is plausible but only one impl needed now (r8brain-free MIT or libsamplerate BSD). | one free function in `bf_dsp/resample.h`; no interface (add when a 2nd impl exists) |
| Codec | **No new interface**: `juce::AudioFormat` / `AudioFormatReader` / `AudioFormatWriter` already *is* the codec seam. MP3 export = one custom `AudioFormatWriter` subclass wrapping libmp3lame. | reuse JUCE |

```cpp
// bf_dsp/stretch.h
struct WarpSegment { int64_t srcBegin, srcEnd; int64_t dstBegin, dstEnd; }; // piecewise-linear, monotonic
struct StretchParams { int channels; double sampleRate; int engineMode; /* formant, transient pref, ... */ };

class ITimeStretcher {
public:
    virtual ~ITimeStretcher() = default;
    virtual void prepare(const StretchParams&, int maxBlock) = 0;   // allocate here, never in render loop
    virtual int  inputLatency() const = 0;
    virtual int  outputLatency() const = 0;
    virtual void reset() = 0;
    // Render dst range of one chunk; src includes pre-roll context. Deterministic for identical inputs.
    virtual void renderChunk(std::span<const float* const> src, int64_t srcFrames,
                             std::span<const WarpSegment> segs,
                             std::span<float* const> dst, int64_t dstFrames,
                             const std::stop_token&) = 0;
    virtual std::string_view id() const = 0;                         // "signalsmith-1.4.0", goes into cache key
};

// bf_analysis/analyzer.h
struct BeatResult { std::vector<double> beatsSec, downbeatsSec; std::vector<float> beatConf;
                    std::vector<TempoHypothesis> tempoHyps; std::string modelId; };
class IBeatAnalyzer {
public:
    virtual ~IBeatAnalyzer() = default;
    virtual BeatResult analyze(const PcmView& mono22k, ProgressSink&, const std::stop_token&) = 0;
    virtual std::string_view id() const = 0;                         // model hash + version → cache key
};
```

#### 3.3 Data flow

```
Import(path)
  → hash file bytes (XXH3-128, streaming)               [import worker]
  → decode once → float32 PCM cache file (memory-mapped) [import worker]  (immutable source never touched)
  → peaks pyramid (source)                               [peaks worker]
  → analysis: resample 22.05k mono → IBeatAnalyzer → BeatResult (+ DSP features) [analysis worker]
  → user/auto choices → WarpMap (pure function of BeatResult + target BPM + user edits)  [UI thread, cheap]
  → RenderPlan = chunks(WarpMap, low-energy beat boundaries)                              [UI thread, cheap]
  → RenderGeneration g: preallocated dst buffer + per-chunk ready flags                   [render pool]
       priority: chunks nearest playhead first, then outward (progressive preview)
  → peaks pyramid (corrected) per finished chunk                                          [peaks worker]
  → Playback engine reads source or g (A/B) via atomic pointers                           [audio thread]
  → Export: (optional resample to target SR) → TPDF dither if int → true-peak/clip check → writer → temp → rename  [export worker]
```

Everything downstream of the PCM cache is a pure function of (source PCM, project state, engine versions) → **render-from-source, reproducible, cacheable**.

### 4. Threading model

| Thread | Owner | Does | Never |
|---|---|---|---|
| UI / message thread | JUCE `MessageManager` | edits project state, builds WarpMap/RenderPlan, publishes new generations, polls progress at 30 Hz (`juce::Timer`), paints | decode, analyse, render, file I/O > a few ms, wait on workers |
| Audio callback (RT) | `AudioIODevice` (WASAPI) | mixes source/corrected block, A/B crossfade, meters | see rule list below |
| Import worker (1) | engine | file hash, decode → PCM cache | — |
| Analysis worker (1; ORT uses its own intra-op threads = cores/2) | engine | features, ONNX inference, tempo hypotheses | touch RenderGeneration |
| Peaks worker (1) | engine | min/max pyramids for source and finished render chunks | — |
| Render pool (N = hw threads − 2, min 1) | `juce::ThreadPool` | one chunk per job, own pre-prepared `ITimeStretcher` per worker | share stretcher instances |
| Export worker (1) | engine | final render (reuses pool), SRC, dither, encode, validate, atomic rename | run while a render generation is being mutated (export takes its own immutable snapshot) |
| Housekeeping (timer on UI thread) | engine | retire old generations after audio thread ack, cache LRU eviction, autosave | — |

**Realtime callback rules (MUST NOT)**: allocate/free (`new`, `std::vector` growth, `std::string`, `juce::String`, `std::function` capture creation), lock (mutex, `CriticalSection`, `MessageManagerLock`, `shared_ptr` refcount drop to zero → deletion), file/network I/O, logging (`Logger`, `printf`, `OutputDebugString`), syscalls that may block, exceptions, ORT calls, page-faulting first touches on fresh memory (pre-touch buffers on worker), unbounded loops (e.g. binary search is fine, O(log n); linear scans over the beat list are not).
**MAY**: read immutable data reachable from an atomically loaded pointer; `std::atomic` load/store/exchange (lock-free, check `is_always_lock_free`); SPSC queue pop (bounded, preallocated); write counters for xruns/max callback µs.

**Primitives** (ladder: stdlib first):
- Cancellation: `std::stop_source` / `std::stop_token` (C++20) passed to every job; checked per block (≤ 10 ms of work) — plus `ThreadPoolJob::shouldExit()` when inside JUCE pool; ORT inference cancelled via `RunOptions::SetTerminate()` from a `std::stop_callback`.
- UI→audio commands: one bounded SPSC queue (preallocated ring; e.g. `farbot`/own 60-line ring, or `readerwriterqueue` from vcpkg) carrying POD commands: `Play/Stop/Seek(pos)/SetAB(view)/SetLoop`.
- Audio→UI: atomics only (playhead sample position, xrun count, peak meters). UI polls.
- Buffer publication: `std::atomic<const RenderGeneration*> current;` (release store on UI thread, acquire load on audio thread). Old generation retired only when the audio thread's `std::atomic<uint64_t> ackGen` ≥ new generation id (audio stores the id it loaded at the top of each callback). Deletion on UI thread. No `shared_ptr` in RT path.
- Progressive fill: `RenderGeneration` owns a **preallocated** dst buffer (length known from WarpMap) and `std::unique_ptr<std::atomic<uint8_t>[]> chunkReady`; render job writes samples then `chunkReady[i].store(1, release)`; audio checks `load(acquire)` before reading that chunk; unready → fallback (see A/B below) and UI shows "rendering" hatch.

```cpp
// audio callback sketch (RT)
void audioDeviceIOCallbackWithContext(..., float* const* out, int nCh, int n, ...) {
    Command c; while (cmdQ.pop(c)) apply(c);                        // POD, bounded
    auto* g = current.load(std::memory_order_acquire);
    ackGen.store(g ? g->id : 0, std::memory_order_release);
    const int64_t pos = playheadTarget;                              // in corrected timeline samples
    renderBlock(view, g, src, pos, out, n);                          // reads only; crossfades on A/B change
    playheadTarget += n; playheadPublished.store(playheadTarget, std::memory_order_relaxed);
}
```

**A/B switching** (low latency, musically aligned):
- Playhead lives in the **corrected (target) timeline** τ. Original view reads source at s = W⁻¹(τ) (piecewise-linear inverse, precomputed arrays in the generation, binary search per block). So toggling A/B keeps the same bar/beat.
- On toggle: 10 ms equal-gain crossfade (signals are near-correlated) between the two read positions; both reads already resident → switch latency = one device buffer (typ. 10–20 ms WASAPI shared; **target, measure**).
- Original playback through W⁻¹ at 1× speed means the original drifts relative to the grid — by design (that is what A/B demonstrates). Alternative mode "A/B at same wall-clock time" is a UI option, same machinery.
- Device SR ≠ source SR: never resample on the audio thread. Preview renders target device SR directly; a source-at-device-SR copy is produced once in the background. Export renders at export SR from source.

### 5. Render / cache strategy

**Chunk-parallel stretch** (deterministic, independent of core count):
1. Chunk plan from WarpMap only: candidate cut points = beat positions; choose cuts every ~8–16 beats (≈4–8 s) at the beat with minimum short-term RMS (±50 ms window) → cut inside sustained/silent material, not on transients.
2. Each job: fresh `reset()` stretcher; feed pre-roll `P = inputLatency + 1 block` of source before chunk start via `seek()`; process piecewise segments (per beat interval k ratio r_k = (τ_{k+1} − τ_k)/(s_{k+1} − s_k)) by handing differing input/output sizes to `process()`; render an extra overlap tail `O` (≈ 50–100 ms, configurable) past the chunk end; `flush()` at file end.
3. Stitch: crossfade overlap O between chunk i tail and chunk i+1 head (equal-gain linear; verify null-test vs single-pass render on golden corpus — QA track).
4. Determinism: chunk plan and each chunk's input are pure functions of (PCM, WarpMap, params) → bit-identical result regardless of thread scheduling on the same binary/CPU path. Across CPUs (SIMD dispatch, FMA) expect tiny differences → golden tests use tolerance (e.g. −100 dBFS RMS diff), not hashes.
5. Rubber Band (if licensed): offline path = whole-file `study()` + `setKeyFrameMap()` + `process()` (single-threaded, per baseline) or the same chunk scheme in realtime mode; keep behind `ITimeStretcher`.

**Progressive preview**: on WarpMap change → new generation; job queue ordered by |chunk centre − playhead| ascending, then by position; previous generation keeps playing until the chunk under the playhead is ready in the new one (then swap). Target: first audible corrected audio ≤ 1 s after an edit near the playhead (**target, to benchmark**; depends on stretcher speed per core, **UNVERIFIED**).

**Content-addressed cache** (`%LOCALAPPDATA%\BeatFix\cache\`):
```
key = XXH3_128(
   sourceFileHash128, decoderId@version,          // MP3/AAC decoders differ by version → part of key
   canonicalJSON(warpMap) , stretcherId@version + canonicalJSON(params),
   renderSampleRate, channelLayout, BEATFIX_RENDER_FORMAT_VERSION)
chunkKey = XXH3_128(key-parts-minus-warpMap, chunk src range, chunk segments, preroll, overlap)
```
- Cache **per chunk**, not per file: a local warp edit invalidates only chunks whose source span or segments changed → re-render is local. (Chunk boundaries must be stable under local edits: cut search is local to each 8–16-beat window.)
- Files: `cache/ab/abcdef….f32` raw interleaved float32 with 32-byte header (magic, version, frames, channels, sr, CRC32 of payload). Written temp → `FlushFileBuffers` → rename. Corrupt/short/CRC-fail → delete + re-render (corruption detection).
- Decoded source PCM cache uses the same scheme, keyed by (fileHash, decoder version); memory-mapped for playback/analysis.
- Analysis cache: `BeatResult` JSON keyed by (sourcePcmKey, analyzerId) → reopening a project is instant.
- Eviction: LRU by atime-in-index, default budget 5 GB (setting), never evict chunks of the open project's current generation.
- Invalidation: implicit (new key). No "dirty flags" to maintain — the key *is* the invalidation rule.

### 6. Waveform / peaks

- Pyramid of min/max per channel at 256, 1024, 4096, 16384 samples/bin; int16-quantised min/max (sufficient for display).
- Size, 10 min @ 48 kHz stereo: level-256 = 600·48000/256 = 112,500 bins × 2 ch × 2 values × 2 B = **0.9 MB**; whole pyramid ≈ 1.2 MB. Two lanes (source + corrected) ≈ 2.4 MB. Cheap enough to keep in RAM and in the cache.
- Correction-intensity lane: per beat interval |r_k − 1| and stretch-mode indicator, drawn from WarpMap directly (no audio needed).
- Paint: choose the coarsest level with ≥ 1 bin per pixel; draw one vertical line per pixel column; cache the lane as an `Image` per (zoom, scroll) and only redraw the playhead overlay at 60 Hz.

### 7. Memory budget

Formula: bytes = seconds × sampleRate × channels × 4 (float32).

| Item | 10 min, 48 kHz stereo | 10 min, 96 kHz stereo |
|---|---|---|
| Decoded source (float32) | 600·48000·2·4 = **230.4 MB** (219.7 MiB) | 460.8 MB |
| Source at device SR (only if SR differs) | 0–230 MB | 0–461 MB |
| Current render generation (±stretch length) | ≈230 MB (scales with output duration) | ≈461 MB |
| Previous generation during swap | ≤230 MB transient | ≤461 MB |
| Analysis input 22.05 kHz mono float32 | 52.9 MB (freed after analysis) | same |
| Mel spectrogram (128 × 50 fps assumption, Beat This-like) | 30,000 × 128 × 4 = 15.4 MB | same |
| Peaks (both lanes) | ≈2.4 MB | ≈4.8 MB |
| Stretcher instances (N workers) | few MB each (**UNVERIFIED**) | — |
| ORT session + model | ≈ model size × 1–3 (8–250 MB, **UNVERIFIED**) | — |
| **Peak working set target** | **≤ 1.0 GB** | **≤ 1.8 GB** |

Mitigation if needed: source PCM and render generations as **memory-mapped cache files** (OS pages them; working set ≈ what's playing). JUCE `MemoryMappedAudioFormatReader` exists for WAV; our raw `.f32` cache can use `juce::MemoryMappedFile` directly. Pre-touch (fault-in) the region around the playhead on a worker so the RT thread never page-faults on a cold mapping. Long files (> 30 min) → mapped mode mandatory.

### 8. Project / session format (`.beatfix`, JSON, UTF-8)

```json
{
  "schemaVersion": 1,
  "app": { "name": "BeatFix", "version": "0.1.0", "renderFormatVersion": 1 },
  "projectId": "6f1c…uuid",
  "source": {
    "pathAbsolute": "D:\\Music\\song.mp3",
    "pathRelative": "..\\Music\\song.mp3",
    "fileHashXXH3_128": "9f…", "sizeBytes": 8123456, "mtimeUtc": "2026-10-08T10:00:00Z",
    "decoder": "juce-mp3@9.0.3",
    "pcm": { "sampleRate": 44100, "channels": 2, "frames": 26460000, "pcmHashXXH3_128": "1a…" }
  },
  "analysis": { "analyzer": "beatthis-small-onnx@<sha256-8>", "cacheKey": "…",
                "selectedTempoHypothesis": 0, "userOverrides": { "halfDouble": "none" } },
  "beatMap": { "beatsSec": [ ... ], "downbeatIdx": [ ... ], "confidence": [ ... ], "edited": true },
  "target": { "bpm": 120.0, "anchorBeat": 0, "anchorTimeSec": 0.512 },
  "warpMap": { "kind": "piecewiseLinear", "pointsSrcSec": [ ... ], "pointsDstSec": [ ... ] },
  "stretch": { "engine": "signalsmith", "engineVersion": "1.4.0", "mode": "auto", "params": { } },
  "enhancement": null,
  "export": { "format": "wav", "sampleRate": "source", "bitDepth": 24, "dither": "tpdf" },
  "ui": { "zoom": 1.0, "scrollSec": 0.0, "abView": "corrected" }
}
```
- **Non-destructive**: no audio inside the project; caches are rebuildable. Beat/warp arrays in seconds (double) so they survive re-decoding and SR changes; sample positions derived at render time.
- **schemaVersion**: integer; loader runs a migration chain `vN → vN+1` (pure functions on JSON); unknown future version → open read-only with warning. Never silently drop unknown fields (preserve under `"_unknown"` on save).
- **Atomic save**: write `name.beatfix.tmp` in same directory → `FlushFileBuffers` → `ReplaceFileW` (keeps ACL/attributes; falls back to `MoveFileExW(MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)` if target absent). Keep one `name.beatfix.bak` (previous version).
- **Autosave & crash recovery**: when dirty, every 60 s (setting) write `%LOCALAPPDATA%\BeatFix\autosave\<projectId>.beatfix` (same atomic routine) — never overwrite the user's file implicitly. Session lock `%LOCALAPPDATA%\BeatFix\sessions\<projectId>.lock` with PID + start time; on launch, a lock whose PID is dead + an autosave newer than the project → "Recover unsaved changes?" (diff summary: target BPM, #edited beats).
- **Source relocation** (brief §19): on open, (1) absolute path + size/mtime → verify hash lazily in background; (2) relative path from project dir; (3) same filename in project dir; (4) prompt "Locate source…". Accept candidate if `fileHash` matches (exact). If only `pcm.frames/sampleRate/channels` match and `pcmHash` matches → accept (re-encoded container, same audio). If duration matches within 1 ms but PCM differs → warn "different audio; analysis and warp map may not fit", offer re-analyse. Paths stored are the user's own; never logged in full (see §9).
- **File locking**: open source files with share-read only; detect external modification via size/mtime at playback start and export (export re-verifies hash → refuses on mismatch).

### 9. Logging, diagnostics, privacy

- Local rotating text log `%LOCALAPPDATA%\BeatFix\logs\beatfix-YYYYMMDD.log`, keep 7 files / 10 MB each. Start with `juce::FileLogger` (already present); move to spdlog `rotating_file_sink` (vcpkg 1.17.0, MIT) only if async logging is needed.
- Log events, durations, versions, device names, errors. **Never** log audio content, analysis arrays, or full paths — log `fileHash[0:8]` + extension; full path only if user enables "verbose diagnostics".
- RT thread never logs: it increments atomics (xruns, overload count, max callback µs); UI thread logs deltas once per second.
- Crash dumps: `SetUnhandledExceptionFilter` → `MiniDumpWriteDump` (dbghelp, `MiniDumpWithIndirectlyReferencedMemory`, **not** full memory → no audio buffers) to `%LOCALAPPDATA%\BeatFix\crashes\`. Next launch: "BeatFix closed unexpectedly. Open crash folder?" — user decides whether to send anything. **No automatic upload, no telemetry** (sentry-native/crashpad only if an opt-in upload is ever wanted).
- ONNX Runtime: call `DisableTelemetryEvents()` at startup (F4).
- Temp files only under `%LOCALAPPDATA%\BeatFix\` (not `%TEMP%` shared dirs); deleted on cancel; orphan sweep on startup.

### 10. Repository structure

```
BeatFix/
  CMakeLists.txt  CMakePresets.json  vcpkg.json  .clang-format  .clang-tidy
  cmake/            # Deps.cmake (FetchContent pins), Warnings.cmake
  src/
    bf_dsp/         # warp.h/.cpp, stretch.h, stretch_signalsmith.cpp, stretch_rubberband.cpp (opt), resample.h, peaks.h, xfade.h
    bf_analysis/    # analyzer.h, beatthis_onnx.cpp, dsp_onset_fallback.cpp, tempo_hypotheses.cpp
    bf_io/          # pcm_cache.cpp, decode.cpp, export.cpp, lame_writer.cpp, atomic_file.cpp
    bf_engine/      # session.cpp, project_json.cpp, jobs.cpp, render_scheduler.cpp, cache.cpp, playback.cpp
  apps/
    beatfix/        # JUCE GUI: MainWindow, WaveformLane, Transport, Inspector
    beatfix-cli/    # Phase 2+ (empty target stub NOT created until needed)
  models/           # beat model .onnx + SHA256 + LICENSE (Git LFS)
  tests/
    unit/           # bf_dsp, warp math, project migrations, cache keys (Catch2)
    integration/    # decode→analyse→render→export on short fixtures
    golden/         # golden audio refs (LFS) + tolerance config
  tools/            # bench_render, null_test, (optional) ImGui debug viewer
  installer/        # BeatFix.iss (Inno), AppxManifest.xml (Store MSIX, Phase 2)
  docs/
  .github/workflows/ci.yml
```

### 11. Build & dependency management

- **CMake ≥ 3.25**, C++20, MSVC v143 (VS 2022) x64 primary; clang-cl second config for warnings/tidy.
- JUCE via its CMake API: `juce_add_gui_app(beatfix …)`, `juce_add_console_app(beatfix-cli …)`, `juce_generate_juce_header` not needed (include modules directly).
- **Dependency policy (ladder: CMake built-in before extra tooling)**:
  - `FetchContent` (built into CMake; CPM is just a wrapper → skip) pinned to **tag + commit hash**: JUCE (8.0.15 or 9.0.x — decision), Signalsmith Stretch 1.4.0 (header-only; vcpkg port lags at 1.1.0), Catch2 v3.
  - ONNX Runtime: official prebuilt `onnxruntime-win-x64-1.30.0.zip` via `FetchContent_Declare(URL … URL_HASH SHA256=…)` — avoids building ORT from source (vcpkg port builds from source, version 1.23.2).
  - **vcpkg manifest mode** only for compiled C libs without good CMake: `mp3lame` (3.100), `xxhash` (0.8.4) — or vendor xxhash single header. Pin with `builtin-baseline`.
  - Rubber Band only behind `BF_WITH_RUBBERBAND=OFF` default (GPL / commercial — **legal review**).
- Build flags: `/W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc`, `/fp:precise` for DSP (no fast-math until null-tests prove equivalence), LTO in Release, `/guard:cf`, PDBs archived per release (for crash-dump symbolication).
- Sanitizers: MSVC `/fsanitize=address` CI job for unit + integration tests. Clang RealtimeSanitizer (`-fsanitize=realtime`, `[[clang::nonblocking]]`) on a Linux CI job for `bf_dsp`/playback mixer — Windows support **UNVERIFIED**; optional Phase 9.

### 12. CI (GitHub Actions)

```yaml
# .github/workflows/ci.yml (sketch)
on: [push, pull_request]
jobs:
  windows:
    runs-on: windows-latest            # image currently Windows Server 2025 + VS 2022 (verify on runner)
    steps:
      - uses: actions/checkout@v4
        with: { lfs: true }
      - uses: actions/cache@v4         # FetchContent downloads + vcpkg binary cache dir
        with: { path: "build/_deps\n${{ runner.temp }}/vcpkg-bin", key: deps-${{ hashFiles('cmake/Deps.cmake','vcpkg.json') }} }
      - run: cmake --preset ci-msvc     # VCPKG_BINARY_SOURCES=clear;files,${{ runner.temp }}/vcpkg-bin,readwrite
      - run: cmake --build --preset ci-msvc --config Release
      - run: ctest --preset ci-msvc --output-on-failure   # unit + short integration + golden (tolerance)
      - run: cmake --build --preset ci-msvc --target package_inno   # on tags only
      # signing step (tags only): Artifact Signing GitHub Action or signtool with cloud-HSM cert
```
- Golden-audio tests run on short (10–30 s) fixtures in CI; full corpus benchmarks run manually / nightly.
- Release job: build, sign exe + DLLs + installer, upload PDBs as private artifact, produce SHA256SUMS.

### 13. Coding standards (minimal, enforceable)

- C++20; `clang-format` (file in repo), `clang-tidy` subset: `bugprone-*`, `performance-*`, `modernize-use-nullptr`, `cppcoreguidelines-pro-type-member-init`; warnings as errors in CI.
- RT code lives in `playback.cpp` + `bf_dsp` mixers only; functions callable from the callback are marked `// [RT]` and may call only other `[RT]` functions (reviewed; RTSan later).
- No exceptions across thread or module boundaries; workers return `std::expected<T, Error>` (C++23 — MSVC supports in `/std:c++latest`; otherwise a tiny `Result<T>`) — **decision for orchestrator** (C++20 vs C++23).
- Ownership: `std::unique_ptr` by default; no `shared_ptr` reachable from RT path.
- Units in names: `…Sec`, `…Frames`, `…Hz`; sample positions `int64_t`.
- Every heuristic constant (chunk size, overlap, crossfade ms, RMS window) lives in one `constexpr` config header with a comment citing the doc/test that validates it (brief §39 "no undocumented heuristics").
- `ponytail:`-style comments for deliberate shortcuts with their ceiling and upgrade path.

### 14. Packaging, signing, updates

| Channel | Mechanism | Signing | Updates | Network by default |
|---|---|---|---|---|
| **MVP: direct download** | Inno Setup per-user installer (no admin; `%LOCALAPPDATA%\Programs\BeatFix`) + portable zip | Authenticode on exe/DLLs/installer: Artifact Signing (if eligible: org in listed countries or individual in US/CA) else OV cert on cloud HSM (≤ 460-day validity since 2026-03-01; price **UNVERIFIED**) | none in-app; "Check for updates" = opens website (user action) | **none** |
| **Phase 2: Microsoft Store** | MSIX (full-trust Win32) | **signed by Microsoft for free** (F12) | Store-managed | none from app |
| Optional later | Velopack (MIT) installer + delta updates | own cert | opt-in check | opt-in only |

- **Why not WiX/MSI**: WiX v6+ OSMF fee for revenue > $10k; MSI authoring overhead; no benefit for a per-user consumer app. (Tauri option would use WiX 3.14 or NSIS.)
- **MSIX caveats**: writes to `%LOCALAPPDATA%` are virtualised into the package's private store (transparent to the app; cache location differs on disk → document for support); uninstall removes caches (good); file-type association `.beatfix` via manifest.
- **SmartScreen**: expect warnings for new releases regardless of EV/OV until download reputation accrues (F11). Store channel avoids this.
- **Signing scope**: sign `BeatFix.exe`, `onnxruntime.dll` (already Microsoft-signed — verify; don't re-sign if signed), `libmp3lame.dll`, installer. Timestamp every signature (RFC 3161) so binaries stay valid after cert expiry.
- **CLI later**: ship `beatfix-cli.exe` in the same installer; no separate packaging.

---

## Alternatives Considered

| Alternative | Why not (now) |
|---|---|
| Brief's 10-layer stack as separate modules/libraries | Over-layered for one track; more seams than replaceable parts. Kept as folders inside 4 libs. |
| Interfaces for resampler and codec | Only one implementation each; JUCE `AudioFormat` already is the codec seam. Add `IResampler` when a 2nd impl lands. |
| Rust core + Tauri (Option 3) | Strong runner-up. Loses on FFI to C++ stretch libs, RC-status `ort`, young `signalsmith-stretch`/`beat-this` crates, 3 languages, IPC for all edit state. Wins on UI speed & licence. Revisit if UI velocity becomes the bottleneck. |
| JUCE backend + WebView UI (`WebBrowserComponent` native integration) | Gets web UI speed with C++ core, but two UI paradigms + JS↔C++ bridge for waveform/edit state; reconsider post-MVP for non-waveform panels only. |
| Qt 6 (Option 2) | Better widgets, but audio devices/formats assembled by hand; LGPL obligations or subscription; no owner experience. |
| Dear ImGui (Option 4) | Not a consumer UI (accessibility, native dialogs, polish). Use for an internal debug viewer under `tools/` only if needed. |
| Separate render process (crash isolation) | Premature (brief §39 "no premature microservices"). Revisit only if third-party stretch libs crash in the field. |
| SQLite project DB | JSON is diffable, human-readable, tiny (< 1 MB). SQLite only if multi-track/stems history appears. |
| CPM.cmake / Conan | FetchContent covers header-only/source deps; vcpkg covers LAME. One less tool. |
| Whole-file single-threaded render | Simple but too slow for interactive edits on 10-min songs; chunk scheme gives local invalidation + parallelism. Keep single-pass render as the **reference** for null tests. |

## Evidence / Sources

- JUCE API (Context7 `/websites/juce_master` → docs.juce.com/master): AudioTransportSource, AudioSourcePlayer, AudioDeviceManager, AudioFormatManager, WavAudioFormat::createMemoryMappedReader, AudioFormat::createWriterFor(unique_ptr, AudioFormatWriterOptions), MP3AudioFormat, WindowsMediaAudioFormat, FlacAudioFormat, LAMEEncoderAudioFormat, ThreadPool/ThreadPoolJob, AudioThumbnail, OpenGLContext, FontOptions::withDirect2DHinting. High.
- JUCE releases & licence: https://api.github.com/repos/juce-framework/JUCE/releases (9.0.3 2026-09-28; 8.0.15 2026-07-21); https://github.com/juce-framework/JUCE/blob/master/LICENSE.md (AGPLv3 / JUCE 9 EULA, SPDX SBOM). JUCE 8 tiers: https://forum.juce.com/t/amendments-to-the-juce-end-user-licence-agreement-for-juce-8/61265 . JUCE 9 tiers: https://juce.com/legal/juce-9-licence/ (did not render → UNVERIFIED).
- ONNX Runtime C/C++ API (Context7 `/websites/onnxruntime_ai_api_c`): Session ctors, RunOptions SetTerminate. Telemetry: https://raw.githubusercontent.com/Microsoft/onnxruntime/HEAD/docs/Privacy.md . Release 1.30.0 assets: https://github.com/microsoft/onnxruntime/releases . High.
- ort crate (Context7 `/pykeio/ort`): Session builder, linking (`load-dynamic`, `ORT_LIB_PATH`), multiversion (rc.13), ort-tract. crates.io API for versions. High.
- cpal 0.18.2: https://docs.rs/cpal/0.18.2/cpal/traits/trait.DeviceTrait.html ; 0.17 docs via Context7. High.
- Tauri 2 IPC (Context7 `/websites/tauri_app`): ipc::Response, ipc::Channel, raw invoke body. Updater: https://v2.tauri.app/plugin/updater/ . Windows installer/WebView2 modes: https://v2.tauri.app/distribute/windows-installer/ . WiX 3.14 pin: tauri-bundler `msi/mod.rs` (dev branch). High.
- Signalsmith Stretch README: https://github.com/Signalsmith-Audio/signalsmith-stretch (seek/flush/latency/splitComputation); tags (1.4.0). High.
- vcpkg port versions: https://github.com/microsoft/vcpkg/tree/master/ports (raw vcpkg.json per port, fetched 2026-10-08). High.
- Artifact Signing: https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart (geography, individual US/CA), /faq (no EV, paid subscription, SmartScreen builds over time), https://azure.microsoft.com/pricing/details/artifact-signing/ (quotas 5,000/100,000; prices not rendered). High/Med.
- EV SmartScreen change: https://www.todesktop.com/blog/posts/windows-apps-psa-ev-certs-do-not-grant-immediate-reputation-anymore ; https://learn.microsoft.com/en-us/answers/questions/1850140/ . Med-High.
- 460-day cert validity: https://www.digicert.com/blog/understanding-the-new-code-signing-certificate-validity-change ; https://knowledge.digicert.com/alerts/code-signing-certificates-459-day-validity . High.
- Microsoft Store free registration & MSIX signing: https://blogs.windows.com/windowsdeveloper/2025/09/10/free-developer-registration-for-individual-developers-on-microsoft-store/ ; https://blogs.windows.com/windowsdeveloper/2026/05/07/publish-to-microsoft-store-as-a-company-now-with-free-registration-and-faster-onboarding/ . High.
- Inno Setup licensing: https://jrsoftware.org/isorder.php , https://jrsoftware.org/ishelp/topic_purchase.htm . WiX OSMF: https://docs.firegiant.com/wix/osmf/ . Velopack: https://github.com/velopack/velopack (MIT). High.
- Qt licensing: https://doc.qt.io/qt-6/licensing.html ; Small Business exhibit https://www.qt.io/terms-conditions/qt-dev-framework/exhibit-small-business-2026-01 . High (terms).
- r8brain-free-src MIT: https://github.com/avaneev/r8brain-free-src/blob/master/LICENSE . High.

## Recommendation (proposal — orchestrator decides)

1. **Stack: Option 1 — C++20 + JUCE + CMake**, with a GUI-free core (`bf_dsp` pure std C++; `bf_engine` on juce_core/audio modules only). Confidence **Med-High**.
   - Reason: one language from audio callback through stretch libs and ONNX Runtime (zero FFI on the render path), JUCE supplies WASAPI devices, WAV/AIFF/FLAC/Ogg r/w, MP3 + Media Foundation AAC decode, memory-mapped readers, thread pool, D2D rendering; owner has shipped JUCE; CLI is a second JUCE target over the same core.
   - **Key tradeoff**: slower, more manual UI building (custom-painted components, weaker accessibility than DOM/Qt) and a JUCE licence fee once revenue > $20k (AGPL otherwise) — accepted in exchange for the lowest technical risk on the audio/DSP path. Runner-up: Rust + Tauri (best UI velocity and licence cost; loses on FFI + RC/young crates).
2. **JUCE version**: decide between 8.0.15 (brief; last 8.x) and 9.0.x (current, new EULA, prices **UNVERIFIED**). Recommend 9.0.x if JUCE 9 EULA tiers are confirmed equivalent — **legal review**; otherwise pin 8.0.15.
3. **Interfaces**: only `ITimeStretcher` and `IBeatAnalyzer`. Resampler = one function; codec = reuse `juce::AudioFormat` + one LAME writer.
4. **Threading**: UI / RT callback / import / analysis / peaks / render pool / export; SPSC POD commands in, atomics out; atomic generation pointer + audio-ack retirement; `std::stop_token` cancellation; ORT `RunOptions::SetTerminate`.
5. **Render**: chunk-parallel Signalsmith with low-energy beat-boundary cuts, pre-roll via `seek()`, overlap crossfade, `flush()` at end; per-chunk content-addressed cache; progressive render outward from playhead; single-pass render kept as reference for null tests.
6. **Project**: JSON `.beatfix` with `schemaVersion` + migration chain, seconds-based beat/warp arrays, file hash + PCM hash, relocation flow, temp→`ReplaceFileW` atomic save, autosave to app-data + PID lock recovery.
7. **Build**: CMake + FetchContent (JUCE, Signalsmith 1.4.0, Catch2, ORT prebuilt zip w/ SHA256) + vcpkg manifest for `mp3lame`; GitHub Actions `windows-latest` with ASan job.
8. **Distribution**: MVP Inno Setup per-user + portable zip, signed (Artifact Signing if eligible, else OV cloud-HSM); Phase 2 Microsoft Store MSIX (free MS signing + Store updates). No in-app network calls in MVP; ORT telemetry disabled; crash dumps local-only.

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| JUCE 9 EULA terms/prices differ from JUCE 8 | Med | Med | Legal review before pinning 9.x; 8.0.15 fallback. |
| Chunk-boundary artifacts in stitched render | Med | High | Low-energy cut choice, overlap crossfade, null-test vs single-pass reference on corpus (QA track); fall back to single-pass for export if a chunk fails a boundary check. |
| Signalsmith CPU cost makes progressive preview > 1 s | Med | Med | `presetCheaper` for preview, `presetDefault` for export; smaller first chunk around playhead. Benchmark early (Phase 1–5). |
| Memory > 1 GB for long / 96 kHz files | Med | Med | Memory-mapped PCM + generation files; pre-touch around playhead. |
| RT callback page faults on mapped memory | Med | High | Fault-in window ahead of playhead on worker; xrun counter in diagnostics. |
| LAME LGPL / Rubber Band GPL / symphonia MPL obligations | Med | High | Dynamic-link LAME DLL; Rubber Band off by default; **legal review**. |
| SmartScreen warnings hurt first-run trust | High | Med | Sign everything; Store channel; publish hashes. |
| Artifact Signing unavailable (individual outside US/CA) | High for non-US individual | Med | OV cert on cloud HSM; or org entity in a listed country; Store channel. |
| ORT Windows telemetry violates offline/privacy stance | Low (if handled) | Med | `DisableTelemetryEvents()` at init; test with ETW listener. |
| UI build speed in JUCE slows MVP | Med | Med | Keep UI scope strict (brief §34); reuse JUCE LookAndFeel; consider WebView panels post-MVP. |
| Determinism across CPUs/SIMD paths | Med | Low | Tolerance-based golden tests; pin compiler flags; `/fp:precise`. |

## Open Questions

1. JUCE 8.0.15 vs 9.0.x — need JUCE 9 EULA tier/price confirmation (**legal review**).
2. Is Beat This (ONNX) in MVP, or DSP-only first (brief §36)? Affects ORT (≈ +10–80 MB package) and memory budget.
3. Owner's legal entity & country → Artifact Signing eligibility vs OV certificate purchase.
4. C++20 strict or allow C++23 `std::expected` (MSVC `/std:c++latest`)?
5. Default cache budget (5 GB proposed) and location (MSIX virtualisation acceptable?).
6. Does Agent A's stretch research confirm Signalsmith quality at per-beat ratio changes (piecewise ratios inside one `process()` stream) and the required overlap length for seamless chunk stitching?
7. Export path: always single-pass reference render (slower, safest) or the chunked render (faster, needs boundary QA)?
8. A/B default: "same musical position" (W⁻¹ mapping) vs "same wall-clock time" — UX track to decide.

## Implementation Implications

- **Phase 0**: repo skeleton per §10, CMake presets, FetchContent pins, CI green with one Catch2 test in `bf_dsp`, empty JUCE window, Inno script producing an unsigned installer.
- **Phase 1**: import → hash → decode → PCM cache (mapped) → source peaks → playback via own `AudioIODeviceCallback` (not `AudioTransportSource`, because A/B needs two synchronised readers and W⁻¹ mapping) → transport + waveform lane.
- **Phase 2–3**: analysis worker + `IBeatAnalyzer` (DSP fallback first, ONNX second) with cancellation and analysis cache.
- **Phase 4–5**: WarpMap + RenderPlan + chunk-parallel `ITimeStretcher` (Signalsmith) + generations + progressive preview + per-chunk cache; null-test tool vs single-pass.
- **Phase 6**: corrected lane + intensity overlay + A/B crossfade.
- **Phase 7**: export worker (SRC, TPDF dither, true-peak check, LAME writer, temp→rename, post-export re-decode validation).
- Cross-cutting from Phase 0: RT rules in code review checklist, atomic file helper, log/crash-dump setup, ORT telemetry off, `schemaVersion` + migration test.
