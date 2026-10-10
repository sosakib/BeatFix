# Audio Pipeline (decode, cache, playback, export, file integrity)

Status: 2026-10-08, v1.0. Covers brief §13 (audio fidelity architecture), the I/O part of §15, and §38 (file and data integrity). Decisions: DECISION_SUMMARY §1 rows Decode, Encode, Loudness, Internal precision, Sample rate, and §6 (A/B 20 ms, export defaults, peak gate). Research basis: [AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md) (numbers and measurements live there). UI owners: [PLAYBACK_AND_AB.md](../06_ui/PLAYBACK_AND_AB.md), [EXPORT_UI.md](../06_ui/EXPORT_UI.md), [UX_BEHAVIOR.md](../06_ui/UX_BEHAVIOR.md).

## Objective
Get audio in and out with no degradation beyond the warp itself, with **0-sample timing offsets**, one lossy encode at most, and no silent changes to level or content.

## 1. Fidelity rules (brief §13 → mechanism)

| Brief §13 requirement | Mechanism | Where |
|---|---|---|
| Immutable original source | Opened read-only, closed after decode, never written, export cannot target it (incl. hard links) | §2.4, §7.6 |
| Decode once | One gapless decode into the PCM cache. Analysis, render, playback and validation all read that buffer | §2 |
| High-precision internal processing | f32 samples, f64 accumulators, f64/i64 time | §4 |
| Non-destructive analysis | Analysis reads a separate mono low-SR copy and never writes PCM | §2.3 |
| Render from source + project state | Every render reads the PCM cache plus the persisted inputs. Never a previous render or export | RENDER_PIPELINE.md |
| No repeated lossy encoding; final export once | Encoders run only in the export chain. Re-import of a BeatFix lossy export is detected and warned (AUDIO_QUALITY_RESEARCH §4.10) | §6 |
| Proper SRC | r8brain, export tier, at most once, only on request or codec constraint ([RESAMPLING.md](../04_algorithms/RESAMPLING.md)) | §5, §6 |
| Proper bit-depth conversion + optional dither | TPDF on float → int only, seeded, bypass when already on grid ([DITHERING.md](../04_algorithms/DITHERING.md)) | §6 |
| True-peak protection, clipping detection | libebur128 4× true peak, sample peak, NaN/Inf; peak gate with explicit user choice | §6.2 |
| Phase/stereo integrity checks | Linked-stereo stretch (one stretcher for all channels); export validation correlates L/R of output vs pre-encode buffer; mono stays mono, no silent upmix | §6, RENDER_PIPELINE |

The UI uses the claim vocabulary in AUDIO_QUALITY_RESEARCH §4.1. It never implies that lossless export or upsampling restores missing information.

## 2. Import: decode once, gapless

```
path ──► probe (≤ 200 ms: container, codec, SR, ch, est. frames, encoder delay info)
     │     unsupported / >2 ch → typed error at probe time (UX_BEHAVIOR §15), never after a long decode
     ├─► pass 1: SHA-256 over file bytes (streaming 1 MiB, CNG)          ─► fileSha
     │     PCM cache lookup by key H(fileSha, decoderId@version) ── hit ──► map + verify CRC ──► done
     └─► pass 2 (miss): decode ──► float32 planar blocks ──► PCM cache file (temp → rename)
                                  ├─► PCM hash (SHA-256, canonical form §3.2)
                                  ├─► DecodeReport (trims applied, damaged spans, NaN count, truncation)
                                  └─► peaks worker (incremental pyramid)
```
Two passes over the file (hash first) because the hash decides whether decoding is needed at all. For a 1 GB WAV the extra pass costs about 1 s of disk read. The OS file cache serves pass 2.

### 2.1 Decoders (ADR-009)
| Format | Decoder | Timing rule | Phase 1 gate |
|---|---|---|---|
| WAV / RF64 / BW64 (PCM u8/s16/s24/s32, float32/64, EXTENSIBLE) | JUCE `WavAudioFormat` | sample-exact | click track, 0 offset, exact length |
| AIFF / AIFF-C | JUCE | sample-exact | same |
| FLAC | JUCE (bundled libFLAC) | sample-exact; STREAMINFO MD5 verified when present → mismatch = Issues entry "File content does not match its checksum" | same + MD5 |
| Ogg Vorbis | JUCE (bundled libvorbis) | granule trimming | same |
| MP3 | `BfMp3AudioFormat` = **minimp3_ex** | skip Xing/Info/VBRI frame; trim `enc_delay + 529` head and `enc_padding − 529` tail from the LAME tag; iTunSMPB fallback. No tag → **no trim**, `DecodeReport.encoderDelayKnown = false`, shown as "encoder delay unknown" (AUDIO_QUALITY_RESEARCH §4.3) | click track MP3 (LAME CBR/VBR, non-LAME): first click at the exact sample; untrimmed would be +1105 samples (MEASURED, F1) |

AAC/M4A/Opus are **Post-MVP** (DECISION §3.7). JUCE's `MP3AudioFormat` and `WindowsMediaAudioFormat` are not registered.

### 2.2 Damage handling during decode
Skippable decode errors → silence of the exact skipped length, with the span recorded. Truncation → continue and record. NaN/Inf → 0 and counted. Behaviour and messages: UX_BEHAVIOR §14, [ERROR_HANDLING.md](ERROR_HANDLING.md).

### 2.3 Analysis copy
Mono downmix (arithmetic mean L/R, the Beat This! reference, BEAT_TRACKING_RESEARCH) → `resampleOffline(Analysis tier)` to the analyzer's `inputSampleRate()`. It is zero-phase, so beat times in seconds are identical in both domains (RESAMPLING.md test R-A1). It lives only for the duration of the analysis job (52.9 MB for 10 min) and never feeds render.

### 2.4 Source file handling
- Opened with `GENERIC_READ`, `FILE_SHARE_READ | FILE_SHARE_DELETE`. Other programs can still read, rename or move the file. Writers are blocked only while BeatFix reads it.
- **Closed when decode ends.** All later work uses the PCM cache, so moving or editing the source while BeatFix runs cannot corrupt a render or an export.
- Never opened with write access (test DSP-INT-1: source SHA-256 unchanged after the full pipeline, run on a read-only file).

## 3. PCM cache

### 3.1 File
`%LOCALAPPDATA%\BeatFix\cache\pcm\<key>.f32`, key = SHA-256(fileSha ‖ decoderId@version)[0:32 hex]. Layout:
```
header 64 B: magic "BFPCM\0\0\1", formatVersion u32, sampleRate f64, channels u32,
             frames i64, channelStrideFrames i64, pcmSha256[32]-ref offset, payloadCrc32c u32, decoderId[16]
payload:     planar float32, channel c at offset 64 + c·stride·4   (stride ≥ frames)
```
- **Planar with a stride.** `PcmView.ch[c]` points straight into the mapping, with no de-interleaving and no copy. The stride is preallocated from the probe's upper-bound frame estimate (+5 %). If decode overruns it (rare, e.g. a header-less CBR file), decoding restarts with stride × 1.5 and a log line. ponytail: restart on overrun, single-pass growth only if logs show it happens.
- Memory-mapped read-only for everything after decode. The working set is what is touched, which keeps 10-min / 48 kHz within P4 (≤ 1.0 GB) and makes 60-min files possible.
- Integrity: CRC32C (SSE4.2 `_mm_crc32_u64`, ~10 GB/s, a few lines) verified on every open. A mismatch or short file deletes the entry and re-decodes. The full PCM SHA-256 is computed once at decode and stored in the header and the project.
- Eviction: LRU with the other caches (RENDER_PIPELINE §6). The PCM cache of an open document is pinned.

### 3.2 Fingerprints
| Fingerprint | Over | Detects | Stored |
|---|---|---|---|
| `fileSha256` | raw file bytes | any byte change, incl. tag edits | project `source`, cache key |
| `pcmSha256` | decoded, gapless-trimmed PCM in canonical form: **lossless** → interleaved little-endian int32 of the native integer samples (decoder-independent); **lossy** → interleaved little-endian float32 bits, valid only together with `decoderId@version` | audio identity across containers/re-tagging | project `source.pcm`, PCM cache header, warp map `source.fingerprint`, render cache keys |
| `sizeBytes`, `mtimeUtc` | filesystem | fast "probably unchanged" check before hashing | project |

SHA-256 via Windows CNG (TECH_STACK: no xxHash). Speed is hardware-dependent (SHA-NI). It runs once per import on the I/O worker. XXH3 is added only if profiling puts hashing on the critical path.

Reopen decisions are in [PROJECT_SESSION_MODEL.md](PROJECT_SESSION_MODEL.md) §6.

## 4. Internal precision (ADR-002)
| Quantity | Type | Why (AUDIO_QUALITY_RESEARCH §4.5) |
|---|---|---|
| Sample buffers (PCM cache, renders, playback) | float32 | 24-bit mantissa ≈ 144 dB. Half the memory of f64 |
| Accumulators (OLA sums, FIR MACs, K-weighting, energy, RMS) | float64 | No error growth over 10⁷–10⁸ terms |
| Times, positions, warp anchors, ratios, BPM | float64 seconds or int64 frames | f32 cannot hold integer frame indices above 2²⁴ = 349.5 s at 48 kHz |
| Sample counts per block | from **absolute** positions: `round(W⁻¹(τ)·fs)`, never accumulated (WARP_FUNCTION §7) | No drift over long files |

## 5. Sample rate
- Decode, analysis anchors (seconds), render and the default export all run at the **native source rate**. No upsampling "for quality".
- **Export**: at most one SRC (export tier), after render, before measurement. It runs only when the user picks another rate or the codec forces it (MP3 > 48 kHz source → 48 k from 96 k, 44.1 k from 88.2 k).
- **Analysis**: a separate analysis-tier SRC on a mono copy (§2.3). It never feeds output.

### 5.4 Playback when device SR ≠ source SR
1. The app first asks JUCE `AudioDeviceManager` to open the device at the source rate. If the device accepts, no SRC runs.
2. Otherwise a **monitor-tier `StreamingResampler`** (r8brain, fixed ratio, preallocated in `prepare()`) runs as the last stage of the RT chain. A and B are mixed at the source rate first, so the A/B crossfade and click timing stay sample-exact in source/target frames.
3. The monitor SRC never touches render, cache or export output. DECISION §1 "at most one SRC, only at export" governs the output path. Monitoring a 44.1 kHz file on a 48 kHz shared-mode device needs a conversion either way, and doing it here keeps the stored audio untouched.
4. Why not a pre-converted copy at device rate (ARCHITECTURE_RESEARCH §4): it doubles PCM memory (+230 MB) and must re-run for every render chunk, with seams at chunk boundaries. Why not WASAPI's own conversion: JUCE shared-mode behaviour at a non-mixer rate is **UNVERIFIED**.
5. Gate: allocation-free steady state of r8brain `process()` (Phase 1 test, THREADING_MODEL §2.2). Seek or switch → `clear()`, also verified allocation-free.

## 6. Playback chain (RT, `PlaybackEngine::process`)

```
 SourceBuffer (mmap PCM, source frames s) ──► Voice A reader ─┐
                                                              ├─► crossfade gains ─► + click overlay ─► [monitor SRC] ─► device
 RenderGeneration g (target frames t) ─────► Voice B reader ──┘   (≤ 2 voices)        (active voice's   (only if SR ≠)    channel map
   span-ready check per block; W/W⁻¹ anchors; S_i, T_k click lists                    beat list)                         mono→L+R
```

### 6.1 Transport
- One **musical position** `(s, t = W(s))` (PLAYBACK_AND_AB §1). The active voice owns its integer read position and advances 1 frame per output frame. The other coordinate is derived per block by binary search on g's anchor arrays (`O(log n)`), using the same W/W⁻¹ as the renderer, including edge extrapolation (WARP_FUNCTION §6).
- Commands arrive over the SPSC queue (THREADING_MODEL §3.1): Play, Pause, Stop (returns to the start position, set by the UI), Seek (musical position as source frame `s`), SetActive, SetLoop (target frames `[t_a, t_b)`; A loops `[W⁻¹(t_a), W⁻¹(t_b))`), SetClick, SetXfadeMs.
- No automatic gain difference between A and B (PLAYBACK_AND_AB §3).
- Before analysis exists there is no generation, so only A plays.

### 6.2 A/B switch
- Map the position (`t = W(s)` or `s = W⁻¹(t)`), start the incoming voice there, and run an **equal-power crossfade, default 20 ms** (setting 5–50 ms): `g_out = cos(πx/2)`, `g_in = sin(πx/2)`, x: 0→1 over the fade (DECISION §6).
- Why equal-power and not equal-gain: A and B play different time bases and the stretch re-phases partials. The two signals are **not** sample-correlated over the fade, so equal-gain would dip about 3 dB mid-fade on uncorrelated content.
- Gain ramps are computed per sample from a precomputed table (2 × 2401 floats at 48 kHz/50 ms max), so no `sin`/`cos` runs in the callback.
- If B's span at `t` is not ready, A fades out and B waits (buffering, below).

### 6.3 B buffering and the stale-audio rule
B never plays audio from a superseded edit state (PLAYBACK_AND_AB §4.5). When the generation changes while B plays (an edit):
- New span at `t_new = W_new(s)` ready → 20 ms crossfade from the old generation (fade-out only of what was already playing) to the new one.
- Not ready → the old generation fades out over 20 ms, B outputs silence, the position is held, `bufferingB = 1`. B fades in from the held position when the span becomes ready.
- B reaching an unready span during normal playback → the same hold-and-resume. A is always playable.
- No realtime ("live") stretching in the RT thread in the MVP. It would put `ITimeStretcher` on T2, which THREADING_MODEL §2.1 forbids. Progressive chunk rendering targets P3 ≤ 500 ms instead.

### 6.4 Loop
At the loop end the voice jumps to the loop start through the same two-voice crossfade (configured length). No click at the wrap. Changing the loop while playing applies at the next wrap.

### 6.5 Metronome overlay
- Click sounds are synthesised once in `prepare()` (preallocated). Normal: short decaying sine. Downbeat: higher pitch, +6 dB (PLAYBACK_AND_AB §2). Level default −12 dBFS peak.
- A clicks at **detected** beats `S_i` (analysis, refined). B clicks at **grid slots** `T_k`. Both lists are frame-converted arrays inside the generation (the A list comes with the analysis even before the first generation).
- A monotone cursor advances per block. Seek, switch and loop wrap re-find it by binary search. During a crossfade each voice contributes its own clicks at its own gain.
- The click exists only in `PlaybackEngine`. The export chain never instantiates it, so it cannot leak into a file.

### 6.6 Device events
Device lost or changed → the app stops the callback, keeps the position, calls `PlaybackEngine::prepare(newRate, maxBlock)` off the RT thread (allocation allowed), and restarts. Toast per PLAYBACK_AND_AB §6.

## 7. Export chain (export worker, on a snapshot of E)

```
snapshot(E, W)                                         stage label (EXPORT_UI §4)
 → render chunks at Quality::High (cache reuse, export priority in pool)      Rendering
     into an export buffer (mmapped temp, not the live generation)
 → [Enhancement slot — Post-MVP, bypass = bit-identical]
 → SRC once (export tier) if export SR ≠ source SR or codec forces it
 → output gain (user, EXPORT_UI Advanced)
 → MEASURE: sample peak, true peak (libebur128, 4×), LUFS-I, LRA, NaN/Inf, silence
 → PEAK GATE  int lossless: sample peak > 0 dBFS | lossy: true peak > −1.0 dBTP | float WAV: warn only
       exceeded → ask user (non-blocking card; worker waits on cv_any with stop_token):
       [Lower gain by X dB (default)] [True-peak limiter (opt-in)] [Export anyway — N clipped samples]
       choice applied → re-measure (a limiter must reach ceiling + 0.05 dB, AUDIO_QUALITY_RESEARCH §4.7)
 → QUANTIZE  int target: TPDF ±1 LSB, seeded PRNG, bypass if already on grid | float: none | lossy: float to encoder   Encoding
 → ENCODE + TAGS  WAV (auto RF64 > 4 GiB) | FLAC (MD5) | MP3 LAME CBR 320 default, LAME tag written
       tags: normalised source tags copied, BPM tag, stale timing/gain tags stripped,
             BEATFIX_RENDER=<projectId>;<renderPcmSha> marker (AUDIO_QUALITY_RESEARCH §4.10)
       written to  <target dir>\<name>.<ext>.partial  (CREATE_NEW), FlushFileBuffers
 → VALIDATE: re-decode .partial through the import path (AUDIO_QUALITY_RESEARCH §4.11):          Checking
       format/SR/ch/bit depth; length exact (MP3 exact via LAME tag); lossless: decoded ints == quantised buffer
       (FLAC: + STREAMINFO MD5); lossy: alignment offset = 0 samples (xcorr first/last 10 s), corr > 0.98;
       NaN/Inf none; peak vs gate; silence; ΔLUFS
 → ATOMIC RENAME (§8.2) → export report (UI card + project exportHistory)
```
- **Order: SRC before measurement.** SRC turns inter-sample peaks into sample peaks (AUDIO_QUALITY_RESEARCH §4.8), so the gate must see post-SRC audio. The brief's ordering "peak gate → SRC" would miss overs created by SRC.
- Defaults (DECISION §6): WAV 24-bit + TPDF, FLAC 24, MP3 CBR 320 + LAME tag. Float WAV is never dithered. Headless default for the gate = lower gain.
- Export renders from the PCM cache at High quality. It never re-uses a Fast-quality preview chunk, a previous export, or the live generation buffer, so later edits cannot change a running export.
- Cancel at any stage: stop token → the RAII guard deletes `.partial` and the export buffer. Status `Cancelled`, no error card.

## 8. File and data integrity (brief §38)

### 8.1 Temporary files
| Temp | Location | Why there | Cleanup |
|---|---|---|---|
| Export `name.ext.partial` | target directory | the rename must stay on the same volume to be atomic | RAII guard on cancel/failure. The path is also recorded in the session lock file, so crash recovery deletes it (PROJECT_SESSION_MODEL §5) |
| Project `name.beatfix.tmp` | project directory | same-volume `ReplaceFileW` | deleted on failure; a stale one older than the project is deleted on next save |
| Cache writes `cache\tmp\<rand>.tmp` | `%LOCALAPPDATA%\BeatFix\cache\tmp` | renamed into the cache when complete | startup orphan sweep (anything not owned by a live session lock) |
| Generation / export buffers | `cache\tmp`, `FILE_ATTRIBUTE_TEMPORARY \| FILE_FLAG_DELETE_ON_CLOSE` | backs the memory mapping; never persisted | the OS deletes them when the handle closes, including after a crash |

Never `%TEMP%` (shared, cleaned by third-party tools). Never next to the source.

### 8.2 Atomic writes
- **Replace existing**: write temp in the same directory → `FlushFileBuffers` → `ReplaceFileW(target, temp, backup, REPLACEFILE_IGNORE_MERGE_ERRORS…)`. This preserves ACLs and attributes. Projects keep `name.beatfix.bak`.
- **New file**: `MoveFileExW(temp, target, MOVEFILE_WRITE_THROUGH)` without `REPLACE_EXISTING`. If the target appeared meanwhile, the call fails and the user gets the collision choice (EXPORT_UI §3) instead of a silent overwrite.
- Cache entries: temp → `MoveFileExW(…, MOVEFILE_REPLACE_EXISTING)`. Content-addressed, so a concurrent identical write is harmless.

### 8.3 Partial renders
- Generation buffers are memory only (delete-on-close) and are never treated as complete files.
- A chunk cache file exists only after a complete write + rename. Its header carries frames and a CRC32C.
- An export file reaches its final name only after validation passes. Partial output never lands at the final name.

### 8.4 Corruption detection
| Artifact | Check | On failure |
|---|---|---|
| PCM cache, chunk cache | header magic/version, size, CRC32C on open | delete entry, re-decode / re-render, log WARN |
| Source on reopen | size/mtime fast path, then `fileSha256` in the background | PROJECT_SESSION_MODEL §6 flow |
| FLAC import | STREAMINFO MD5 | Issues entry, continue |
| Project file | JSON parse + schema validation + `WarpMap::validate` on the derived map | typed error. Offer `.bak` / autosave (ERROR_HANDLING) |
| Model file | SHA-256 vs build-time constant | analysis falls back to the DSP tracker with an Issues entry |
| Export | full validation (§7) | never renamed; precise error card |

### 8.5 File locking
- Source: share-read + share-delete, held only during hash and decode (§2.4).
- Project file: never held open. Single-writer between BeatFix instances is enforced by the session lock file (PROJECT_SESSION_MODEL §5), not by OS locks, because users' sync tools (OneDrive, Dropbox) break long-held handles.
- Export target held open by another program (a DAW playing it) → `ReplaceFileW` fails with `ERROR_SHARING_VIOLATION` → "The file is in use by another program." [Retry] [Keep both] [Cancel]. The partial is deleted unless the user retries.

### 8.6 Source protection: hard links and aliases
Before export (pre-flight and again just before rename), if the target exists or resolves to an existing file:
1. Canonical compare: `GetFinalPathNameByHandleW` on both, case-insensitive. Equal → refuse.
2. Identity compare: `GetFileInformationByHandle` → equal `dwVolumeSerialNumber` and `nFileIndexHigh/Low` → refuse. This catches hard links, junctions and SUBST/mapped-drive aliases.
Message: "Can't overwrite the original file. Choose another name." (EXPORT_UI §3).

## Open Questions
- Tag writing coverage of JUCE writers (FLAC Vorbis comments, WAV LIST/INFO and `id3 ` chunk) is **UNVERIFIED**. TagLib (LGPL DLL) is the candidate, **for legal review**.
- MMCSS registration and WASAPI shared-mode rate handling in JUCE 9 (§5.4) are **UNVERIFIED**, Phase 1.
- Whether DAWs/DJ apps honour the LAME gapless tag (AUDIO_QUALITY_RESEARCH Q2) decides whether the MP3 export card warns about ~25 ms offsets in other apps.

## Implementation Implications
- Phase 1 gates: click-track golden per codec (0-sample offset, exact length); DSP-INT-1 immutability; PCM cache CRC fault injection; RT allocation test including the monitor SRC.
- Phase 7 gates: export validation catches injected faults (truncated file, NaN, 1105-sample shift); hard-link refusal test (`CreateHardLinkW` to the source as target).
