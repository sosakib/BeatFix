# Performance Benchmarks

Status: v1.0, 2026-10-10. All targets are **PROVISIONAL** proposals: no published realtime-factor numbers exist for Signalsmith Stretch or Rubber Band R3, and Beat This CPU timings on x86 laptops are unverified (QA_RESEARCH F29–F30). They are confirmed or revised by measurement in the Phase-0 spike on the reference machine. Brief §16: "Do not invent unrealistic performance guarantees."

## Objective
Define what is measured (analysis/render/preview/export time, memory, UI responsiveness, cancellation), how, on which machine, with which inputs, and which targets the MVP aims for.

## Why It Matters
BeatFix processes whole songs offline; users tolerate seconds, not minutes, between "change target BPM" and hearing the result. Memory must stay predictable for long, high-sample-rate files on 8–16 GB laptops.

## Key Findings
- Decoded audio size is the dominant, exactly computable memory term: 10 min, 48 kHz, stereo, float32 = **230.4 MB** (219.7 MiB). Table §3.
- Rendering cost scales with *output* length (ratio 2.0 doubles output).
- Analysis cost is dominated by the beat model (Beat This ~20 M params full / ~2 M small, 50 fps, 30 s chunks); the only timing datapoint found is a young Rust port on Apple M4 (≈0.017× RT) — not transferable to mid-range x86 without measurement.

## Technical Details

### 1. Reference machines (proposal — orchestrator to confirm)
| Role | Spec | Purpose |
|---|---|---|
| REF-MID (gating) | 4 cores / 8 threads x86-64 with AVX2, 2020–2022 mid-range laptop class (e.g. Intel Core i5-1135G7 or AMD Ryzen 5 5500U class), 16 GB RAM, NVMe SSD, Windows 11, AC power, "Balanced" power plan, no GPU use | release gating numbers |
| REF-LOW (floor) | 4 cores / 4 threads, AVX2, 8 GB RAM, SATA SSD | "must still work" (2× REF-MID targets acceptable) |
| CI runner | GitHub `windows-latest` | trend only, never gating (shared, noisy) |

### 2. Metrics and procedure
| ID | Metric | Measurement |
|---|---|---|
| P1 | Analysis RTF = analysis wall time / audio duration | CLI `analyze`, per-stage timers (decode→resample→features→model→decode beats→refine→tempo/drift) |
| P2 | Render RTF (per mode/preset) | CLI `render`, input fixed W; report vs input and vs output duration |
| P3 | Preview latency | time from parameter change to first corrected audio buffer at playhead (UI probe event timestamps) |
| P4 | Memory | peak working set and peak private bytes (`GetProcessMemoryInfo`: PeakWorkingSetSize, PeakPagefileUsage) per stage |
| P5 | UI stall | max main-thread message-loop gap during analysis/render (ETW or in-app watchdog timestamps) |
| P6 | Cancel latency | request → worker acknowledges + resources freed |
| P7 | Decode RTF | per codec |
| P8 | Export RTF | per format (WAV/FLAC/MP3 VBR/CBR) |
| P9 | Thread scaling | P2 at 1/2/4/8 threads → speed-up curve |
| P10 | Audio-thread health | xrun count during playback while P1/P2 run (512-sample buffer @ 48 kHz) |

Protocol: one warm-up + median of 5 runs (warm file cache); separate cold run (first read after reboot); record CPU model, clocks (min/avg), temperature throttling flag, power source; release build with symbols; no debugger attached. Report median, min, max.

### 3. Memory model (computed)
Interleaved float32 PCM bytes = seconds × sr × channels × 4.

| Duration | 44.1 kHz stereo | 48 kHz stereo | 96 kHz stereo |
|---|---|---|---|
| 5 min | 105.8 MB | 115.2 MB | 230.4 MB |
| 10 min | 211.7 MB | 230.4 MB | 460.8 MB |
| 20 min | 423.4 MB | 460.8 MB | 921.6 MB |

Other terms for a 10-min song (48 kHz stereo):
- Analysis audio 22.05 kHz mono float32: 600 × 22050 × 4 = 52.9 MB; mel 128 bins × 50 fps × 600 s × 4 B = 15.4 MB; activations negligible.
- Model + ONNX Runtime: model file ~78 MB (full; baseline log) plus runtime arena — **UNVERIFIED**, budget 250 MB transient during analysis, released after.
- Waveform peak pyramid (min/max per 64 samples + coarser levels): ≈ 10 MB.
- Corrected preview buffer if held in RAM: ≈ source size × mean ratio (≈ 230 MB); per-thread render chunk buffers: tens of MB.
- **Naive all-in-RAM peak ≈ 0.8–0.9 GB**; with source/preview memory-mapped or streamed (ARCHITECTURE: memory-mapped readers, content-addressed chunk cache) the resident set can stay well below that.

### 4. Targets (PROVISIONAL, REF-MID, 48 kHz stereo, 4-min song unless stated)
| ID | Target | Stretch goal | Rationale |
|---|---|---|---|
| P7 | decode MP3/AAC ≤ 0.02× RT (≤ 5 s); WAV/FLAC ≤ 0.01× | — | mature decoders are far faster; mostly I/O |
| P1 | analysis (full model) ≤ 0.15× RT (≤ 36 s); first BPM/beat preview ≤ 10 s via progressive 30 s chunks | ≤ 0.05× RT | M4 datapoint 0.017× RT; mid-range x86 assumed 3–8× slower |
| P1s | analysis (small model) ≤ 0.05× RT | ≤ 0.02× | ~10× fewer parameters |
| — | warp-map rebuild after target-BPM/override change ≤ 100 ms | ≤ 20 ms | pure math on ≤ 10⁴ anchors |
| P3 | preview latency ≤ 500 ms to audible corrected audio at playhead | ≤ 200 ms | progressive render outward from playhead |
| P2 | full render, "Smooth" default preset ≤ 0.25× RT (≤ 60 s) using all cores | ≤ 0.1× | Signalsmith described as CPU-efficient; chunk-parallel |
| P2hq | "High quality" preset ≤ 0.5× RT | — | longer blocks |
| P2rs | "Resample" mode ≤ 0.05× RT | — | windowed-sinc only |
| P8 | export WAV/FLAC ≤ 0.05× RT; MP3 ≤ 0.15× RT | — | LAME single-threaded speed **UNVERIFIED** |
| P4 | peak working set ≤ 1.0 GB for 10-min 48 kHz stereo; ≤ 1.5 GB for 20-min 96 kHz stereo (requires streaming/mmap) | ≤ 0.6 GB | §3 model |
| P5 | main-thread stall ≤ 50 ms typical, ≤ 100 ms hard during background work | — | responsive UI |
| P6 | cancel latency ≤ 250 ms (analysis: ORT `SetTerminate`; render: per-chunk check) | ≤ 100 ms | |
| P10 | 0 xruns during 10-min playback while analysis+render run | — | RT thread isolation |

REF-LOW: ≤ 2× the REF-MID time targets; memory targets unchanged.

### 5. Long-song and extreme tests
| ID | Input | Assertion |
|---|---|---|
| PERF-LONG-1 | 20 min, 48 kHz stereo (S01 + S16 content) | P1/P2 scale linearly (± 10 %); P4 within target |
| PERF-LONG-2 | 20 min, 96 kHz stereo | P4 ≤ 1.5 GB; completes |
| PERF-LONG-3 | 60 min, 44.1 kHz stereo | completes or refuses with clear message per spec; O3 drift ≤ 5 ms (METRICS); no float-time accumulation error |
| PERF-LONG-4 | 3 h, 44.1 kHz (stress) | graceful refusal or streaming; no crash, no OOM |
| PERF-EXT-1 | ratio 0.5 / 2.0 constant | render time ∝ output length; O16/O17 pass |
| PERF-EXT-2 | rapidly varying W (r alternates 0.9/1.1 every beat) | no slowdown > 2× vs constant ratio |
| PERF-SCALE-1 | threads 1/2/4/8 | speed-up ≥ 2.5× at 4 threads (PROVISIONAL); determinism D2 holds |
| PERF-CANCEL-1 | 100 random cancels during analysis and render | P6 met every time; no leaked temp files / threads |

### 6. Regression policy
Release blocked if REF-MID P1/P2 median worsens > 15 % or P4 > 10 % vs previous release without a documented reason. CI perf is trend-only (alert at > 25 %).

## Alternatives Considered
- Quoting vendor/library speed claims as targets: rejected — none published for the relevant engines/configs.
- GPU inference for analysis: out of MVP scope (deployment complexity; CPU target sufficient if P1 is met).

## Evidence / Sources
QA_RESEARCH.md derived facts (memory, drift); F29–F30; ARCHITECTURE_RESEARCH recs 4–5 (threading, chunk-parallel render, ORT terminate); Beat This paper (model sizes, 50 fps).

## Recommendation
Instrument stage timers and memory probes in the CLI from the first commit; run the Phase-0 spike (Beat This ONNX full/small + Signalsmith default/HQ + Rubber Band R3) on REF-MID with the §4 table and replace PROVISIONAL numbers with measured ones ×1.2 headroom.

## Risks
- Beat This full model on CPU may miss 0.15× RT on REF-LOW → small model as default on weak machines (decision after measurement).
- Thermal throttling on thin laptops can double render times on long songs → report sustained, not burst, numbers.

## Open Questions
- Exact REF-MID model to buy/borrow.
- Is full-song preview kept in RAM or streamed from chunk cache (affects P4)?
- Maximum supported duration (product decision; PERF-LONG-3/4 expected behaviour).

## Implementation Implications
- CLI `--profile` flag writing `perf.json` (per-stage times, peak memory, thread count, CPU info).
- Watchdog timestamps on the UI thread in debug/profiling builds.
