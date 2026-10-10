# Threading Model

Status: 2026-10-08, v1.0. Covers brief §18 and §16 (responsive UI, cancellable jobs, no blocking of the realtime thread). Decision: DECISION_SUMMARY §1 "Threading" (ADR-011). Research basis: [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §4. Targets: [PERFORMANCE_BENCHMARKS.md](../07_quality/PERFORMANCE_BENCHMARKS.md) P5 (UI stall ≤ 50 ms typical), P6 (cancel ≤ 250 ms), P10 (0 xruns while analysis and render run).

## Objective
Name every thread, what it owns, what it may and may not do, and the exact primitives between them. A violation of this document is a bug even if no test catches it yet.

## 1. Threads

| # | Thread | Count / created by | Owns | May | Must not |
|---|---|---|---|---|---|
| T1 | **UI / message** | 1, JUCE `MessageManager` | `Session` edit state E, undo stack, current `WarpMap`, generation list, job handles | Edit E; `buildWarp` + chunk plan (≤ 100 ms, pure math); publish generations; poll progress/atomics at 30 Hz (`juce::Timer`); paint; serialise project JSON (< 1 MB, ms); issue `PrefetchVirtualMemory` hints | Decode, analyse, render, hash, write files, wait on any worker (`join`, `future::get`, blocking lock held by a worker) |
| T2 | **RT audio callback** | 1, WASAPI device thread via JUCE `AudioIODeviceCallback` → `PlaybackEngine::process` | Voice state (≤ 2 voices), transport position, click cursor, monitor resampler state | See §2 | See §2 |
| T3 | **I/O worker** | 1, `std::jthread` in bf_engine | Import job, file writes for autosave/save, cache eviction, deferred generation teardown, orphan sweep | Hash file, decode, write PCM cache, write project files, delete cache files | Touch the edit state; run analysis/render |
| T4 | **Analysis worker** | 1, `std::jthread` | One analysis job at a time; ORT session | Downmix, analysis-tier SRC, `IBeatAnalyzer::activations`, `decodeBeats`, tempo, confidence | Touch generations or the playback state |
| T4′ | ORT intra-op threads | ORT-owned, count set explicitly = physical cores / 2 (fixed for D1 determinism, DSP_TEST_STRATEGY §6) | — | ONNX kernels | — |
| T5 | **Peaks worker** | 1, `std::jthread` | Peak pyramids (source, corrected) | Build pyramid levels from PCM / ready render spans ([WAVEFORM_VIEW.md](../06_ui/WAVEFORM_VIEW.md) §4) | Write into render buffers |
| T6 | **Render pool** | N = hardware threads − 2 (min 1), `std::jthread`s + one priority queue (mutex + `condition_variable_any`) | One prepared `ITimeStretcher` **per worker** (never shared) | Render one chunk per job; stitch overlaps; write chunk cache files | Share stretcher instances; read mutable edit state (jobs carry immutable inputs) |
| T7 | **Export worker** | 1, `std::jthread` | Export job: immutable snapshot of E + WarpMap | Submit export-priority chunks to T6; SRC, measure, dither, encode, validate, rename | Mutate any live generation; prompt the user directly (asks via the snapshot, §5.4) |

No other threads. Housekeeping (generation retirement checks, autosave trigger, cache-eviction trigger) runs on the T1 timer and hands file work to T3. Logging is allowed on every thread **except T2** (log sink is mutex-protected; [LOGGING_AND_DIAGNOSTICS.md](LOGGING_AND_DIAGNOSTICS.md)).

Why a std-based pool and not `juce::ThreadPool`: `bf_engine` has no JUCE dependency (SYSTEM_ARCHITECTURE §2). A priority queue + N `std::jthread` is ~80 lines and gives the custom ordering of §4.

## 2. Realtime thread rules

### 2.1 NEVER on T2
| Forbidden | Why / common hidden form |
|---|---|
| Heap allocation or free | `new`/`delete`, `std::vector` growth, `std::string`, `juce::String`, `std::function` construction with captures, `std::shared_ptr` creation, last `shared_ptr` release (runs a destructor) |
| Any lock or wait | `std::mutex`, `CriticalSection`, `MessageManagerLock`, `condition_variable`, `future::get`, spin-waits on another thread |
| File, network, registry, console I/O | includes `OutputDebugString`, `printf`, `juce::Logger`, our log sink |
| Blocking or unbounded syscalls | `Sleep`, `WaitForSingleObject`, `VirtualAlloc`, thread creation |
| Exceptions | a throw allocates and unwinds unpredictably; RT code is `noexcept` |
| ORT, `ITimeStretcher`, `buildWarp`, `resampleOffline`, JSON, hashing | unbounded or allocating work |
| First touch of cold memory | page fault on a fresh mapping = disk read. Mitigated by prefetch (§6) and preallocated, pre-touched buffers |
| Unbounded loops | linear scans over beats or chunks. Binary search (O(log n)) and monotone cursors are fine |
| Denormal arithmetic | FTZ/DAZ set at the top of every callback (`juce::ScopedNoDenormals` equivalent in std: `_mm_setcsr`) |

### 2.2 MAY on T2
- Read immutable data reached through an atomically loaded pointer (`RenderGeneration`, `SourceBuffer`).
- `std::atomic` load/store/exchange on types with `is_always_lock_free` (static_assert in code).
- Pop from the bounded, preallocated UI→RT SPSC command queue; drain at most its capacity per callback.
- Write counters (callbacks, max callback µs, B-underrun count) and published state (position, active source, buffering flag) as atomics.
- Run the preallocated `StreamingResampler` (monitor tier) when device SR ≠ source SR ([AUDIO_PIPELINE.md](AUDIO_PIPELINE.md) §5.4). Its allocation-free steady state is **UNVERIFIED** for r8brain and is a Phase 1 gate (allocation-counting test).

Enforcement: functions callable from T2 are tagged `// [RT]` and may call only `[RT]` functions (code review). A debug-build global `operator new` hook asserts when called on the thread registered as T2. Clang RealtimeSanitizer on a Linux CI job is optional (Windows support **UNVERIFIED**, ARCHITECTURE_RESEARCH §11).

## 3. Primitives

### 3.1 UI → RT: one SPSC command queue
Bounded ring (capacity 256, preallocated, power of two; ~60 lines with two `std::atomic<uint32_t>` indices, acquire/release). POD commands only:
```cpp
struct Cmd { enum Kind : uint8_t { Play, Pause, Stop, Seek, SetActive, SetLoop, ClearLoop, SetClick, SetXfadeMs } kind;
             int64_t a, b; float f; };   // Seek: a = source frame s (musical position); SetLoop: a,b = target frames
```
Full queue: UI drops the oldest *coalescable* command (Seek) and retries next tick. Never blocks. No other thread writes to it.

### 3.2 RT → UI: atomics only
`positionSrcFrame`, `positionDstFrame`, `activeSource`, `isPlaying`, `bufferingB` (bool), `inUseMinGenId`, counters. UI reads them in its 30 Hz timer. RT never calls into UI code.

### 3.3 Atomic buffer swap + deferred free
```cpp
std::atomic<const RenderGeneration*> current;   // written by T1 only (release), read by T2 (acquire)
std::atomic<uint64_t> inUseMinGenId;            // written by T2 only
```
- T1 publishes: `g->id = ++genCounter; current.store(g, release); retired.push_back(prev);` Ids are strictly increasing. `current` never moves back. Undo creates a *new* generation object (cache hits make it instant).
- T2, top of each callback: `g = current.load(acquire)`. If `g` differs from the voice's generation, the old one is kept as the fading voice for ≤ 20 ms (§5.3). Then `inUseMinGenId.store(min(id of every generation a voice references), release)`.
- T1 timer: a retired `r` is freed only when `r->id < inUseMinGenId.load(acquire)` **and** `r->jobsInFlight.load() == 0`. Teardown (unmap, close a delete-on-close file) is posted to T3, so T1 never stalls on it.
- Invariant: T2 only ever touches `current` or a generation it already referenced, and ids are monotone. So any id below the last published minimum is unreachable from T2.
- The decoded `SourceBuffer` uses the same scheme. It changes only when a new document opens, after a `Stop` has been acknowledged.

No `shared_ptr` is reachable from T2. Generations are plain objects owned by T1's list.

### 3.4 Progressive fill (render → RT)
`RenderGeneration` contains a preallocated output buffer and span arrays `spanEndFrame[]` (sorted) + `std::unique_ptr<std::atomic<uint8_t>[]> spanReady` ([RENDER_PIPELINE.md](RENDER_PIPELINE.md) §3). T6 writes samples, then `spanReady[i].store(1, release)`. T2 binary-searches the span for its read position and does `load(acquire)` before reading. Not ready → B buffering (§5.3).

### 3.5 Cancellation: `std::stop_token`
- Every job owns or receives a `std::stop_token` from a `std::stop_source` held by its owner: per import, per analysis, **per generation** (all of its render jobs), per export.
- Work loops check `stop.stop_requested()` at least every 10 ms of work (per stretcher block, per decode block, per 30 s analysis chunk, per encoder block). The target is P6 (≤ 250 ms).
- Pool jobs also check the token **before starting**. Queued jobs of a superseded generation are dropped without running.
- **ONNX Runtime**: each `Run` uses its own `Ort::RunOptions ro`. A `std::stop_callback cb(token, [&]{ ro.SetTerminate(); });` aborts the in-flight `Run` from whichever thread calls `request_stop()`. The non-OK status that follows is mapped to `Cancelled` when `token.stop_requested()`, otherwise to `AnalysisFailed`.
- Waiting with cancellation (export waiting for the gate choice): `std::condition_variable_any::wait(lock, token, pred)`.
- Cancellation returns `Status::Cancelled`. It is not an error ([ERROR_HANDLING.md](ERROR_HANDLING.md) §3). Partial outputs are discarded and temp files deleted by the job itself (RAII guard).

### 3.6 Progress reporting
Each job exposes `Progress { atomic<float> fraction; atomic<int> stage; }` plus, for render, the span-ready array. T1 polls at 30 Hz into the `SessionSnapshot`. ETA is computed on T1 (shown after 2 s, MAIN_WINDOW.md §4.2). Workers never post messages to the UI. This removes lifetime races with closed windows.

### 3.7 Priorities
| Thread | Priority | Why |
|---|---|---|
| T2 | Set by JUCE's WASAPI device thread (MMCSS "Pro Audio" registration by JUCE **UNVERIFIED**, check in Phase 1; if absent, call `AvSetMmThreadCharacteristicsW` on the first callback) | Glitch-free playback |
| T1 | normal | Responsiveness |
| T3, T7 | normal | The user waits on import and export |
| T4, T5, T6 | `THREAD_PRIORITY_BELOW_NORMAL` | Background work must not starve T1/T2. Pool size hw − 2 leaves headroom for T1 and T2 |
| T4′ | ORT default (custom thread-creation hooks exist in ORT, use for priority **UNVERIFIED**) | Analysis rarely overlaps rendering: re-analysis cancels the current generation |

Pool queue order (lowest first): (0) chunks of the live generation within ±1 chunk of the playhead, (1) export chunks, (2) remaining live chunks by distance from the playhead, forward before backward at equal distance. Why export sits above background preview: the user explicitly waits for export, while the far preview is speculative.

## 4. Ownership of shared data

| Data | Writer | Readers | Mechanism |
|---|---|---|---|
| Edit state E, undo stack | T1 | T1 | none (single thread) |
| `WarpMap` | T1 (built) | T1, T6, T7, T2 (through its generation) | immutable after build |
| `SourceBuffer` (mmapped PCM cache) | T3 (once) | T2, T4, T5, T6, T7 | immutable after decode completes; published by atomic pointer |
| `RenderGeneration` output buffer | T6 (disjoint spans) | T2, T5, T7 | span-ready atomics; stitch regions written once by the second finisher (RENDER_PIPELINE §3.3) |
| Cache index | T3 + T6 (insert) + T1 (touch) | all | mutex (never taken on T2) |
| Analysis result | T4 (once per job) | T1 | handed over by value at job completion |
| Logs | T1, T3–T7 | — | sink mutex |

## 5. Sequence diagrams

### 5.1 Analyse
```
T1 (UI)                 T4 (analysis)                 ORT threads           T1 timer
 │ analyse() ─────────► │ downmix + SRC→22.05k         │                     │
 │  returns jobId       │ (stop checked per 10 ms)     │                     │
 │                      │ for each 30 s chunk:         │                     │
 │                      │   Run(ro) ──────────────────►│ kernels             │
 │                      │   ◄──────────────────────────│ logits              │
 │                      │   progress.fraction = k/K    │                     │
 │                      │ decodeBeats, tempo, conf     │                     │
 │                      │ result ready (atomic flag)   │                     │
 │ ◄──────────────────────────────────────────────────────────────────────── │ poll: take result
 │ build defaults, buildWarp, new generation g1 (→ §5.2)                     │
```

### 5.2 Render after an edit (progressive)
```
T1                         T6 pool                       T2 (RT)               T5
 │ edit → E'               │                             │ playing B in g0     │
 │ buildWarp → W'          │                             │                     │
 │ plan chunks, keys       │                             │                     │
 │ g1 = new generation     │                             │                     │
 │ g0.stop.request_stop()  │ g0 jobs: drop / abort       │                     │
 │ enqueue g1 jobs ───────►│ chunk at playhead first     │                     │
 │ current.store(g1) ─────────────────────────────────── │ sees g1 ≠ g0        │
 │                         │                             │ span not ready:     │
 │                         │                             │ fade g0 out 20 ms,  │
 │                         │                             │ bufferingB = 1      │
 │                         │ render, stitch,             │                     │
 │                         │ spanReady[i] = 1 ──────────►│ ready: fade in g1   │
 │                         │ write chunk cache file      │ bufferingB = 0      │
 │                         │ notify peaks ───────────────────────────────────► │ corrected peaks
 │ timer: g0 retired when id < inUseMinGenId && jobsInFlight == 0 → T3 teardown
```

### 5.3 A/B switch while playing
```
T1                               T2 (RT callback)
 │ key B → push SetActive(B) ───► │ pop cmd
 │                                │ s = voiceA.pos  →  t = W(s)  (binary search in g's anchors)
 │                                │ span(t) ready?
 │                                │   yes: voiceB at t, equal-power crossfade 20 ms (A out, B in)
 │                                │   no : fade A out 20 ms, B waits at t, bufferingB = 1
 │                                │ store activeSource, positions
 │ timer reads atomics, updates lane highlight, announces "Playing corrected"
```
B → A maps `s = W⁻¹(t)`. A is always ready once decoded. Latency = queue pop at the next callback + 20 ms fade (target ≤ 50 ms, UX_BEHAVIOR §16).

### 5.4 Cancel (export example; render and analysis are the same shape)
```
T1                         T7 (export)                    T6 pool
 │ cancel(jobId)           │                              │
 │ export.stop.request_stop() ──► stop_callback fires     │
 │                         │ waits ended (cv_any wakes)   │ export chunk jobs see token:
 │                         │                              │ abort within one block (≤ 10 ms)
 │                         │ RAII guard deletes .partial  │
 │                         │ status = Cancelled           │
 │ ◄── poll: job state Cancelled → card closes, no error card, no log at ERROR level
```

## 6. Memory paging and the RT thread
Source PCM and generation buffers are memory-mapped files (AUDIO_PIPELINE §3). The T1 timer calls `PrefetchVirtualMemory` (asynchronous hint, does not block) for the window [playhead, playhead + 2 s] of every buffer a voice may read. It does this on each tick, on seek, and on A/B switch. The B-underrun counter and xrun counter (LOGGING_AND_DIAGNOSTICS §5) show whether this is enough. Upgrade path if not: `VirtualLock` a sliding window (needs working-set quota adjustment). ponytail: prefetch hint only.

## Risks
| Risk | Mitigation |
|---|---|
| r8brain streaming not allocation-free | Phase 1 allocation test. Fallback: own fixed-ratio polyphase in the monitor path |
| ORT ignores `SetTerminate` in some kernels (latency > 250 ms) | Analysis runs in 30 s chunks. The stop is also checked between chunks, which bounds the worst case at one chunk's inference time. Measure in PERF-CANCEL-1 |
| Oversubscription (ORT + pool) on 4C/8T | Re-analysis cancels render. ORT intra-op fixed to cores/2 |
| Hard page fault on T2 under memory pressure | Prefetch hint, B-underrun counter, mapped-file working set stays small |

## Implementation Implications
- Phase 0: SPSC ring + unit test (two threads, 10⁷ commands, no loss, ordering preserved). `[RT]` tag convention in CONTRIBUTING.
- Phase 1: `PlaybackEngine::process` with allocation hook test. Generation swap stress test: 10 k swaps during playback, ASan clean, no use-after-free.
- PERF-CANCEL-1 (100 random cancels) runs in CI on short fixtures.
