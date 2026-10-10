# Render Pipeline (incl. Cache Strategy)

Implements REQ-RND-001…007, REQ-PERF-005/006. Distilled from [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §5 and [TIME_STRETCH_ENGINE.md](../04_algorithms/TIME_STRETCH_ENGINE.md). Decision record: [ADR-007](../09_decisions/ADR-007-rendering-pipeline.md).

## Objective
Turn `(PcmCache, WarpMap, engine params)` into corrected float32 audio. The render must be fast enough for interactive preview, deterministic for any core count, and incremental under local edits.

## 1. Pipeline

```
 WarpMap (immutable, validated) ─┐
 PcmCache (source, native SR) ───┼─► ChunkPlanner ──► job queue (ordered by |chunk − playhead|)
 EngineParams ───────────────────┘          │
                                            ▼
                         render pool (N = cores − 1 workers)
                         per job: fresh ITimeStretcher
                           seek(pre-roll) → process segments via W⁻¹ → overlap tail
                                            │
                                            ▼
                       ChunkValidator (NaN/Inf, silence, discontinuity, peak)  ← QUALITY_VALIDATION.md
                                            │
                                            ▼
                     ChunkCache (memory + %LOCALAPPDATA%\BeatFix\cache)
                                            │
                    Stitcher (crossfade O between chunk i tail and i+1 head)
                                            │
                     RenderGeneration g ──atomic publish──► PlaybackEngine / Export
```

## 2. Chunk planning (pure function of WarpMap)
1. Candidate cut points are the anchors `x_i`.
2. Within each window of 8–16 beats (≈ 4–8 s), cut at the anchor whose short-term RMS (±50 ms) is **lowest**. That places cuts in sustained or quiet material, away from transients.
3. Windows are fixed in beat index (`[16k, 16k+16)`), so a local edit moves only the cuts inside its own window. Chunk boundaries stay stable under local edits, which is what makes the cache incremental.
4. Each chunk records its source span `[s_a, s_b)`, its target span `[W(s_a), W(s_b))` and the list of segments with ratios `r_k` inside it.

## 3. Rendering one chunk
```
job(chunk c):
    st = makeStretcher(params); st.reset()
    preroll = st.inputLatency() + 1 block
    st.seek(pcm, s_a − preroll)                  # warm-up, output discarded
    for each output block [τ, τ+H) within [W(s_a), W(s_b) + O):
        src0 = round(W⁻¹(τ)·fs); src1 = round(W⁻¹(τ+H)·fs)    # absolute, never accumulated
        st.process(pcm[src0:src1], out[τ:τ+H])
    if c is last: st.flush()
    return out    # length = target span + overlap O
```
- `O` (overlap) is 50–100 ms (PROVISIONAL, fixed in Phase 0 by the chunk-join null test).
- Stitching uses an equal-gain linear crossfade over `O`. Both sides render the same source material through the same `W`, so they are near-identical, and equal-gain avoids a level bump on correlated signals.
- Mono and stereo are processed with linked channels (STEREO_PHASE_PRESERVATION.md).

## 4. Determinism
- The chunk plan and each chunk's input are pure functions of `(PCM, WarpMap, params)`. Workers write into preallocated slots, and the stitch order is by index. The result is **bit-identical for any thread count** on the same binary and CPU path (level D1).
- Across CPUs (SIMD dispatch, FMA) there are tiny differences, so golden tests use tolerances (level D2: null ≤ −90 dBFS peak). See [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md).
- The **single-pass reference** (one stretcher, whole file, same W) is kept for null tests and as the fallback if chunking ever fails validation.

## 5. Progressive preview and generations
- Every validated WarpMap change creates **generation g+1**. Jobs are queued by distance from the playhead.
- Generation g keeps playing until the chunk under the playhead exists in g+1. Then the playback engine swaps (atomic pointer, deferred free; see THREADING_MODEL.md §3). If B playback reaches a chunk of g+1 that is not rendered yet, B pauses at the chunk boundary with a buffering indicator and resumes automatically. A stays instantly available ([PLAYBACK_AND_AB.md](../06_ui/PLAYBACK_AND_AB.md) §4). Audio from g is never played as g+1, and uncorrected audio is never presented as B (REQ-RND-007).
- Jobs of a superseded generation are cancelled via `stop_token` unless their chunk key also exists in the new generation (then they finish and are reused).
- Target: corrected audio audible ≤ 500 ms after an edit near the playhead (REQ-PERF-006, PROVISIONAL).

## 6. Cache strategy

### 6.1 Keys (content-addressed; the key *is* the invalidation rule)
```
renderKey = SHA256( pcmHash, decoderId@ver,
                    stretcherId@ver, canonicalJSON(params),
                    renderFormatVersion )
chunkKey  = SHA256( renderKey, chunk.srcRange, chunk.segments(x_i, T_i), preroll, overlap )
pcmKey    = SHA256( fileHash, decoderId@ver )
analysisKey = SHA256( pcmHash, analyzerId@modelHash, analysisParams )
```
There are no dirty flags. An edit produces new keys for exactly the chunks whose inputs changed (REQ-RND-005).

### 6.2 What triggers a re-render

| Edit | Affected chunks |
|---|---|
| Target BPM | all (every `T_i` moves) |
| Natural↔Strict detent | chunks where smoothed anchors `x_i` changed (usually all, a little) |
| Stretch mode | all |
| Beat-level relabel (×2/÷2/candidate) | all |
| Downbeat shift | all with Preserve start; the warp is otherwise identical, so mostly φ moves |
| Change-point keep/flatten | chunks in and next to the section |
| Undo / redo / A-B toggles | none (cache hits) |

### 6.3 Storage
- Memory: the current generation plus up to 2 previous ones (undo hits), within the REQ-PERF-007 budget.
- Disk: `%LOCALAPPDATA%\BeatFix\cache\<2 hex>\<key>.f32`, raw interleaved float32 with a 32-byte header (magic, version, frames, channels, sample rate, CRC32). Written temp → `FlushFileBuffers` → rename. A CRC failure or short file means delete and re-render (corruption detection).
- Eviction: LRU by access time, default budget **5 GB** (setting). Chunks of the open project's current generation are never evicted.
- The PCM cache uses the same file format, keyed by `pcmKey`, and is memory-mapped for long files. Workers pre-touch pages around the playhead so the realtime thread never page-faults.

## 7. Export path
Export takes the **complete** current generation (rendering any missing chunks first, with progress), then runs the export chain in [AUDIO_PIPELINE.md](AUDIO_PIPELINE.md) §7. Export never uses an estimate or fallback audio.

## Risks
| Risk | Mitigation |
|---|---|
| Audible chunk seams | Phase 0 null test chunked vs single-pass; cuts in low-RMS spots; increase O |
| Signalsmith pre-roll/latency semantics differ from assumption | Phase 0 impulse-alignment test (REQ-STR-005) |
| Disk cache growth | LRU budget + "Clear cache" |

## Implementation Implications
`ChunkPlanner` (pure, unit-tested: stable cuts under local edits), `RenderScheduler` (priority queue + generations), `ChunkCache` (keyed store), `Stitcher`. All live in `bf_engine`. Only `ITimeStretcher` is an interface.
