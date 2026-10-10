# System Architecture

Status: 2026-10-08, v1.0. Covers brief §17 (architecture), §39 (constraints) and §40 (future extensibility). Decisions come from [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) (stack, ADR-001) and [TECH_STACK.md](../08_engineering/TECH_STACK.md) §1.1 (module ↔ dependency map). Research basis: [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §3. Threads: [THREADING_MODEL.md](THREADING_MODEL.md). Audio paths: [AUDIO_PIPELINE.md](AUDIO_PIPELINE.md). Render and cache: [RENDER_PIPELINE.md](RENDER_PIPELINE.md).

## Objective
Fix module boundaries, allowed dependencies, the only two abstract interfaces, and the end-to-end data flow. The beat detector, stretch engine, resampler and codec must each be replaceable without rewriting the application.

## 1. Layering: brief §17 vs chosen

The brief proposes a 10-layer vertical stack: UI → Application/State → Project Model → Audio I/O → Analysis → Tempo/Beat → Warp Map → Stretch → Render → Export/Codec.

**Rejected as a dependency structure.** Why:
1. **It is a pipeline drawn as a stack.** The arrows describe data flow, not compile-time dependency. Read as dependencies, "Analysis depends on Audio I/O" and "Warp Map depends on the Tempo Engine" would force decode code into analysis tests and make the warp map uncallable without an analyser. In reality the warp map is a pure function of plain data (beats, target BPM, N_c).
2. **The Warp Map is not a layer.** It is an immutable value read by four consumers: UI (drawing), renderer, playback (A/B position mapping), cache key. A layer position cannot express that.
3. **Render and Export are siblings.** Both consume (source PCM, warp map, engine). Export is render plus SRC, gate, dither and encode. It does not sit "below" render.
4. **The realtime playback path is missing.** It is the most constrained component (no allocation, no locks) and needs its own boundary.
5. **Ten seams for four replaceable parts** (brief §17 names four) means six abstractions with one implementation each. Brief §39 asks to avoid over-engineered architecture.

**Chosen:** 4 static libraries + 1 GUI executable (+ a CLI target later). The brief's layers survive as **folders and files** inside the libraries. They are not seams.

```
                  ┌──────────────────────────────────────────────────────────┐
                  │ app/beatfix  (JUCE GUI, AudioIODeviceCallback adapter,   │
                  │              crash handler, log sink)                    │
                  │ app/beatfix-cli  (Post-MVP, juce_add_console_app)        │
                  └───────────────────────────┬──────────────────────────────┘
                                              │ Session façade (C++), immutable snapshots
                  ┌───────────────────────────▼──────────────────────────────┐
                  │ bf_engine  Session, project JSON, undo stack, job system,│
                  │            render scheduler, cache, PlaybackEngine (RT), │
                  │            hashing (CNG), atomic files, logging          │
                  └──────┬───────────────────────┬──────────────────┬────────┘
                         │                       │                  │
          ┌──────────────▼────────┐  ┌───────────▼─────────┐        │
          │ bf_io  probe, decode  │  │ bf_analysis          │        │
          │ (JUCE readers private,│  │ IBeatAnalyzer impls, │        │
          │ minimp3), encoders    │  │ beat decoder, tempo, │        │
          │ (JUCE writers, LAME), │  │ stability, confidence│        │
          │ tags                  │  │ (+ ONNX Runtime)     │        │
          └──────────────┬────────┘  └───────────┬──────────┘        │
                         └───────────┬───────────┘                   │
                         ┌───────────▼───────────────────────────────▼┐
                         │ bf_dsp  (std only) warp math, chunk plan,   │
                         │ ITimeStretcher impls, resampleOffline,      │
                         │ peaks, crossfades, dither, meters, mixer    │
                         └─────────────────────────────────────────────┘
```

Mapping of the brief's layers:

| Brief layer | Lives in |
|---|---|
| UI | `app/beatfix` |
| Application / State | `bf_engine/session.*`, `undo.*` |
| Project Model | `bf_engine/project_json.*` ([PROJECT_SESSION_MODEL.md](PROJECT_SESSION_MODEL.md)) |
| Audio I/O | `bf_io` (files) + `app` device adapter + `bf_engine/playback.*` |
| Analysis Engine + Tempo/Beat Engine | `bf_analysis` (activations → decoder → tempo → confidence) |
| Warp Map | `bf_dsp/warp.*` (pure functions, [WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md), [WARP_MAP.md](../04_algorithms/WARP_MAP.md)) |
| Stretch Engine | `bf_dsp/stretch_*.cpp` behind `ITimeStretcher` |
| Render Engine | `bf_engine/render_scheduler.*` + `bf_dsp/chunk_plan.*` |
| Export / Codec | `bf_engine/export.*` (chain) + `bf_io` (encoders) |

## 2. Modules

| Module | Responsibility | May link | Must not | Notes |
|---|---|---|---|---|
| `bf_dsp` | Warp model (W, W⁻¹, validate, buildWarp), chunk planner, stretch adapters, `resampleOffline`, peak pyramid, crossfade laws, TPDF quantizer, meters (libebur128 wrapper), RT mixer primitives | std, Signalsmith 1.4.0, r8brain, libebur128; Rubber Band / Bungee only behind build flags | JUCE, ORT, OS headers, logging, file I/O | Pure functions + value types. Fully unit-testable with synthetic buffers |
| `bf_analysis` | `IBeatAnalyzer` (Beat This! ONNX, DSP fallback), owned DP beat decoder, repair, sub-frame refinement, tempo (Huber IRLS + Theil–Sen), octave candidates, stability, confidence | std, `bf_dsp`, ONNX Runtime 1.30 | JUCE, file I/O except loading the model bytes handed to it | Deterministic after activations ([BPM_ESTIMATION.md](../04_algorithms/BPM_ESTIMATION.md)) |
| `bf_io` | Probe, gapless decode to float blocks, encoders (WAV/RF64, FLAC, MP3 via LAME DLL), tag read/normalise/write | std, `bf_dsp`, JUCE `juce_audio_formats` **in .cpp only**, minimp3, LAME | JUCE in public headers, threads, caching | Streams blocks to a callback; never owns files longer than one call |
| `bf_engine` | `Session` façade, project (de)serialise + migrations, undo, job system + cancellation, render scheduler + generations, content-addressed cache, `PlaybackEngine` (RT `process()`), export chain, hashing (Windows CNG SHA-256), atomic files, log calls | everything above, nlohmann/json, Windows API | JUCE in public headers | The only stateful library |
| `app/beatfix` | Windows, painting, input, JUCE `AudioDeviceManager` + `AudioIODeviceCallback` that forwards to `PlaybackEngine::process`, installs `juce::FileLogger` as log sink, crash handler | `bf_engine`, JUCE GUI/device modules | Calling `bf_dsp`/`bf_io` directly for anything stateful | Talks only to `Session` |
| `app/beatfix-cli` | **Post-MVP** batch/CLI | `bf_engine`, `juce_core` | — | Not created until needed |

**Dependency rules (CI-checked by include-path layout + a header grep):**
- Dependencies point down only. No cycles. `bf_io` and `bf_analysis` do not know each other.
- **No JUCE type in any public header of `bf_*`** (TECH_STACK §1.1). Core value types: `std::span`, `std::vector`, `std::string`, plain structs, `int64_t` frames, `double` seconds.
- `bf_engine` does not include JUCE at all. Logging goes through a function pointer `bf::setLogSink(void(*)(LogLevel, std::string_view))` installed by the app (default: no-op). This keeps the engine usable by a CLI or a non-JUCE shell.
- **C-compatible façade.** `Session`'s public API takes and returns only POD/std types and `Result<T>`, never throws (ERROR_HANDLING.md §2), and owns all memory it hands out through snapshots. DECISION_SUMMARY's "thin C-compatible API" is satisfied by these rules. The literal `extern "C"` header (`bf_api.h`, opaque `bf_session*` handle, error codes) is a mechanical wrapper written when the first non-C++ consumer exists (Tauri fallback). Writing it now would be an unused second API.

### 2.1 The `Session` façade (UI ↔ engine contract)
- UI → engine: commands (`openFile`, `analyse`, `setTarget`, `setCutoff`, `setMode`, `editBeatLevel`, `shiftDownbeat`, `setChangePointPolicy`, `undo`, `redo`, `save`, `exportTo`, `cancel(jobId)`, transport commands). Every call returns immediately; long work becomes a job.
- Engine → UI: `Session::snapshot()` returns `std::shared_ptr<const SessionSnapshot>` (UI thread only; never reaches the RT thread): edit state E, warp map, analysis summary, job progress, render coverage, issues. The UI polls at 30 Hz with a `juce::Timer` and repaints only on a changed snapshot version. No callbacks into UI code from worker threads.

## 3. The only two interfaces

Interfaces exist only where more than one implementation is real now (rule: no interface with one implementation).

### 3.1 `ITimeStretcher` (bf_dsp/stretch.h)
Real implementations: **Signalsmith Stretch 1.4.0** ("Smooth", default), **Varispeed** via r8brain ("Resample" mode, constant ratio only), **Rubber Band R3** (dev/test builds, quality reference, never shipped), later **Transient hybrid** (Phase 1.5) and **Bungee** (fallback). Five candidates justify the seam.

```cpp
namespace bf {
struct PcmView  { const float* const* ch; int channels; int64_t frames; double sampleRate; }; // planar, non-owning
struct PcmSpan  { float* const* ch; int channels; int64_t frames; };                          // planar, writable
struct WarpSlice { std::span<const double> srcSec, dstSec; };   // monotone anchors covering chunk + context (f64)

enum class Quality { Fast, High };          // Signalsmith presetCheaper / presetDefault
struct StretchConfig { int channels; double sampleRate; Quality quality; int maxBlockFrames; };

struct ChunkRequest {
    int64_t dstBegin, dstEnd;               // output frames [dstBegin, dstEnd), absolute target timeline
    int64_t prerollFrames;                  // warm-up output rendered before dstBegin and discarded
    WarpSlice warp;                         // renderer computes src counts from ABSOLUTE positions (WARP_FUNCTION §7)
};

class ITimeStretcher {
public:
    virtual ~ITimeStretcher() = default;
    virtual Status prepare(const StretchConfig&) = 0;      // allocates; once per worker, never per chunk
    virtual void   reset() noexcept = 0;                   // no allocation; called before every chunk
    virtual Status renderChunk(const PcmView& source, const ChunkRequest& req,
                               PcmSpan out, std::stop_token stop) = 0;   // out.frames == dstEnd - dstBegin
    virtual std::string_view id() const noexcept = 0;      // "signalsmith-1.4.0" → cache key
    virtual std::string paramsKey() const = 0;             // canonical preset/params → cache key
};

enum class StretchEngine { Signalsmith, Varispeed, RubberBandRef /*dev only*/ };
std::unique_ptr<ITimeStretcher> makeStretcher(StretchEngine);   // the only factory; a switch
}
```
Contract (tested per implementation, [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md)): deterministic for identical inputs after `reset()`; output aligned to W⁻¹ with impulse-train error < 0.5 ms; no allocation inside `renderChunk` (debug allocation counter); checks `stop` at least every 10 ms of work; returns `Cancelled` (not an error, ERROR_HANDLING.md §3) when stopped.

### 3.2 `IBeatAnalyzer` (bf_analysis/analyzer.h)
Real implementations: **Beat This! ONNX** (default) and the **DSP tracker** (HPSS + SuperFlux; fallback and cross-check, required by brief §36). The seam is at **activations**. Both feed the same owned decoder (DECISION_SUMMARY §1), so decoding, repair, tempo and confidence are written and tested once.

```cpp
namespace bf {
struct MonoView { const float* x; int64_t frames; double sampleRate; };
struct Activations { double fps; std::vector<float> beat, downbeat; };    // probabilities 0..1, frame-wise
struct Progress { std::atomic<float> fraction{0}; std::atomic<int> stage{0}; };

class IBeatAnalyzer {
public:
    virtual ~IBeatAnalyzer() = default;
    virtual double inputSampleRate() const noexcept = 0;   // 22050 for Beat This!; engine resamples once
    virtual Result<Activations> activations(const MonoView& mono, Progress&, std::stop_token) = 0;
    virtual std::string_view id() const noexcept = 0;      // "beatthis-full@<sha256-12>" or "dsp-superflux@1"
};

// Pure, owned, shared by both analyzers:
Result<BeatResult> decodeBeats(const Activations&, const DecoderParams&);  // DP decoder, repair, refinement
}
```
`BeatResult` (beats, downbeats, per-beat confidence, tempo hypotheses, change points, regions) is defined in [BPM_ESTIMATION.md](../04_algorithms/BPM_ESTIMATION.md) and persisted per [PROJECT_SESSION_MODEL.md](PROJECT_SESSION_MODEL.md).

### 3.3 Resampler: a function, not an interface
One implementation (r8brain-free-src, ADR-009). Own polyphase SRC is a *fallback*, not a co-existing implementation.
```cpp
// bf_dsp/resample.h
enum class SrcTier { Export, Analysis, Monitor };
Status resampleOffline(const PcmView& in, double fsOut, SrcTier, std::vector<std::vector<float>>& out,
                       std::stop_token);       // exact length round(N·fsOut/fsIn), latency trimmed, fsIn==fsOut → bit-exact copy
class StreamingResampler;                      // Monitor tier only, see AUDIO_PIPELINE §5.4; preallocated, RT-safe after construction
```
The contract (length, zero delay, ≥ 120 dB stopband at export tier) is enforced by the RESAMPLING.md acceptance tests, not by a type.

### 3.4 Codec: `juce::AudioFormat` is the seam
No BeatFix codec interface. Inside `bf_io/*.cpp`, codecs are `juce::AudioFormat` objects registered in one `juce::AudioFormatManager`:

| Format | Read | Write |
|---|---|---|
| WAV/RF64, AIFF | JUCE `WavAudioFormat` / `AiffAudioFormat` | JUCE WAV writer (current API: `createWriterFor(std::unique_ptr<OutputStream>&, const AudioFormatWriterOptions&)`) |
| FLAC, Ogg Vorbis | JUCE (bundled libFLAC / libvorbis) | FLAC: JUCE writer. Ogg: not exported (MVP) |
| MP3 | `BfMp3AudioFormat` (ours): `createReaderFor` wraps **minimp3_ex** with LAME/Xing gapless trim | same class: `createWriterFor` returns `LameWriter : juce::AudioFormatWriter` over `libmp3lame.dll` (LGPL, dynamic), LAME tag written |

JUCE's own `MP3AudioFormat` is **not** registered (no gapless guarantee; ADR-009). The public `bf_io` API exposes only std types:
```cpp
Result<ProbeInfo> probe(const std::filesystem::path&);                     // ≤ 200 ms, no full decode
Status decode(const std::filesystem::path&, const std::function<Status(const PcmView& block)>& sink,
              DecodeReport& report, std::stop_token);                       // gapless-trimmed float32, native SR
Status encode(const EncodeSpec&, const PcmView& pcm, const std::filesystem::path& tempPath, std::stop_token);
```

## 4. Swapping each replaceable part

| Part | What changes | What does not change | Cache/project effect |
|---|---|---|---|
| Beat detector | New `IBeatAnalyzer` class + one case in the analyzer selector | Decoder, tempo, warp, render, UI | `id()` differs → new analysis cache key. Projects keep their persisted activations; re-analysis is an explicit user action (DECISION §3.4) |
| Stretch engine | New `ITimeStretcher` class + one `StretchEngine` enum value | Warp, chunk planner, scheduler, playback, export | `id()`/`paramsKey()` are in every chunk key → old chunks are simply not hit. Project stores engine id; a newer engine version shows the "output may differ" notice (DSP_TEST_STRATEGY D3) |
| Resampler | Body of `resampleOffline` / `StreamingResampler` | Every caller | SRC tier id is in the export report; renders are unaffected (no SRC in render) |
| Codec | Register a different `juce::AudioFormat` in `bf_io/formats.cpp` | Everything outside `bf_io` | Decoder name+version is part of the PCM fingerprint record (lossy decoders may differ in LSBs) → new PCM cache entry |
| GUI framework (fallback) | Replace `app/` (Tauri shell over `bf_api.h`) + device adapter | `bf_dsp`, `bf_analysis`, `bf_engine` | JUCE-backed readers/writers in `bf_io` must be replaced too (TECH_STACK §1.1, known cost) |

## 5. End-to-end data flow

```
 user: Open song.mp3
   │
   ▼ [import worker]                                                      source file: opened read-only,
 probe ─► hash file bytes (SHA-256) ─► decode once (gapless) ──────────►  closed after decode, never written
                                         │ float32 blocks, native SR
                                         ├─► PCM cache file  %LOCALAPPDATA%\BeatFix\cache\pcm\<pcmSha>.f32 (mmap)
                                         ├─► PCM hash (SHA-256 over canonical PCM)
                                         └─► [peaks worker] source peak pyramid (incremental)
   │
   ▼ [analysis worker]
 mono downmix ─► resampleOffline(Analysis tier, fs → 22.05 kHz, zero-phase)
   ─► IBeatAnalyzer::activations (ORT, cancellable via SetTerminate)
   ─► decodeBeats ─► tempo/octave candidates ─► confidence           (persisted: activations + BeatResult)
   │
   ▼ [UI thread, synchronous, ≤ 100 ms]
 edit state E (target BPM, N_c, mode, beat-level/downbeat/change-point edits)
   ─► buildWarp(BeatResult, E) = WarpMap W (immutable, inputsHash)
   ─► predicted corrected waveform = source pyramid mapped through W (instant)
   ─► chunk plan (quiet points between beats, every 8–16 beats)
   ─► new RenderGeneration g (published by atomic pointer)
   │
   ▼ [render pool, N = hw threads − 2]
 per chunk, nearest-to-playhead first:  cache hit? read : ITimeStretcher::renderChunk (pre-roll + overlap)
   ─► stitch overlaps (linear crossfade) ─► mark span ready ─► write chunk to cache
   ─► [peaks worker] corrected peaks for that chunk replace the estimate
   │
   ├──────────────► [RT audio thread] PlaybackEngine::process:
   │                  A = PCM cache at s, B = g at t = W(s); 20 ms equal-power A/B crossfade;
   │                  click overlay; (monitor SRC if device SR ≠ source SR)
   │
   ▼ [export worker, on snapshot of E]
 chunked render at High quality (cache reuse) ─► SRC once if needed ─► measure (peak, TP, LUFS)
   ─► peak gate (user choice) ─► TPDF dither (int targets) ─► encode + tags ─► name.ext.partial
   ─► re-decode validation ─► atomic rename ─► export report (+ project exportHistory)
```

Everything downstream of the PCM cache is a pure function of (PCM, persisted inputs, engine id/version/params). That is what makes render-from-source, caching by content address and reproducible exports possible.

## 6. Future extensibility (brief §40)

The design leaves room for these. **None is built or scaffolded in the MVP** (DECISION_SUMMARY §4).

| Future feature | What already makes it cheap | What would have to be added |
|---|---|---|
| Manual warp markers / Ableton-style markers | User edits are already inputs to `buildWarp` (WARP_MAP.md). Chunk keys hash the **local warp slice** (RENDER_PIPELINE §5), so a local marker edit re-renders only nearby chunks | Marker edit commands, UI dragging |
| Tempo ramps, per-section target BPM | Target grid T_k is a function inside `buildWarp`. W stays piecewise-linear and monotone whatever the grid is | Grid generator from a tempo curve: T_k = ∫ of 60/BPM(k) |
| Groove templates, swing, quantization strength | Same seam: an offset function applied to T_k before W is built. Strength is a blend of x_i and T_k | Template data model |
| MIDI beat / tempo-map export | Derived WarpMap already has `localBpm`, downbeats (WARP_MAP.md "Interchange") | A writer in `bf_io` |
| Multiple tracks, stems | Session state is one value type (source + analysis + E). A track would be a list of them. The PCM cache, chunk cache and generations are already keyed per source fingerprint. The RT mixer sums voices | `Track` list in Session, per-track generations, mixer bus. Stems: a separator producing N PCM caches keyed by (pcmSha, separatorId) |
| Vocal-specific modes | New `ITimeStretcher` or new params in `paramsKey()`; per-segment `mode` already exists in the warp map | Mode resolution (region Auto, Post-MVP) |
| Plugin integration (hosting effects) | The export chain has a defined slot after render and before SRC (AUDIO_QUALITY_RESEARCH §4.13 Enhancement stage); latency must be compensated to 0 samples | JUCE plugin hosting in `app`, latency compensation test |
| Batch / command line | `Session` has no GUI dependency. Peak-gate default for headless = "lower gain" (AUDIO_QUALITY_RESEARCH §4.7) | `apps/beatfix-cli` (`juce_add_console_app`) |
| Spectral repair, AI-assisted correction | Enhancement slot (above). ML analysers plug in as `IBeatAnalyzer` (activations seam). ORT is already a dependency | Models, labelling per AUDIO_QUALITY_RESEARCH §4.1 vocabulary |

Deliberately **not** prepared (would be speculative): a plugin API for third-party extensions, a generic processing graph, a `Track` abstraction, a separate render process, an IPC layer.

## 7. Facts, recommendations, assumptions, open questions

- **Verified** (research): Signalsmith 1.4.0 supports differing in/out sizes per `process()`, `seek`/`outputSeek`, `flush` (ARCHITECTURE_RESEARCH F9). JUCE writer API change to `std::unique_ptr` (F3). ORT `RunOptions::SetTerminate` and `DisableTelemetryEvents` (F4).
- **Recommendation**: the two interfaces above. Do not add `IResampler`, `ICodec`, `IProjectStore` or `IRenderer` until a second implementation is real.
- **Assumption**: minimp3 wrapped as a `juce::AudioFormatReader` gives sample-exact gapless output. Phase 1 gate: click-track test, 0-sample offset (DECISION §1 "Decode").
- **Open**: tag writing coverage of the JUCE FLAC/WAV writers (Vorbis comments, LIST/INFO, `id3 ` chunk) is **UNVERIFIED**. If insufficient, TagLib (LGPL, DLL) is the candidate, **for legal review**.

## Implementation Implications
- Phase 0: create the 4 libraries + app with the include layout that enforces §2. Add a CI grep that fails if `juce` appears in `src/bf_*/include/**`.
- One `constexpr` config header (`bf_dsp/constants.h`) for every heuristic (chunk beats, overlap, crossfade ms, RMS window). Each constant carries a comment naming the doc/test that validates it (brief §39).
