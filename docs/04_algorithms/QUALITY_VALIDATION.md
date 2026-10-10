# In-App Quality Validation

## Objective
Specify the checks that run **inside the app** on every rendered chunk, every full render and every export, and what the user sees when a check fails. Offline QA metrics (corpus, listening) live in [07_quality](../07_quality/). This doc reuses the metric definitions in [METRICS.md](../07_quality/METRICS.md) (O1–O14) and never redefines them.

## Why It Matters
A producer distrusts a tool that silently ships clicks, overs or misaligned beats. Validation turns invisible failures into Issues entries, and it guarantees that **nothing is exported unchecked** (REQ-AUDIO-011, REQ-EXP-007).

## 1. Three validation levels

| Level | When | Budget | Blocking? |
|---|---|---|---|
| V1 chunk | after each chunk renders, before it can be played | ≤ 2 % of chunk render time | yes: an invalid chunk never plays |
| V2 render report | when a generation completes (background) | ≤ 0.02 × RT | no: Issues entries + quality chip |
| V3 export | after encode, on the written temp file | ≤ 0.05 × RT | yes: failure means no file |

## 2. V1: chunk checks

| Check | Rule | On failure |
|---|---|---|
| Finite | no NaN/Inf | re-render chunk single-pass once → still bad: mark failed (F12) |
| Length | `frames == round(W(s_b)·fs) − round(W(s_a)·fs) + overlap` | same as above |
| Unexpected silence | RMS over 50 ms < −90 dBFS where source RMS at `W⁻¹` > −60 dBFS | same as above |
| Discontinuity | high-pass (> 8 kHz) sample-difference outlier detector (METRICS O11b) at chunk seams and globally; threshold 6× local median | seam: re-stitch with longer overlap; else Issues "possible click at m:ss" |
| Peak tracking | update running sample-peak and 4× true-peak (libebur128) | feeds V2/peak gate |

## 3. V2: render report (per generation)

| Check | Method | Issue raised when |
|---|---|---|
| Timing, engine (Path K proxy) | warp diagnostics: max \|Δr\|, ratio bounds; anchor shifts | any segment `|r/g − 1|` ≥ 8 % (warn) / ≥ 12 % (extreme) |
| Timing, end-to-end (Path R) | re-run the fast DSP onset tracker on the render (no ONNX) near anchors; e_k = detected − T_k | median \|e\| > 10 ms or any run of ≥ 4 beats with \|e\| > 25 ms (PROVISIONAL) → "Beats may be off near m:ss" |
| Residual drift | linear fit of e_k vs k (METRICS O3) | \|e_last\| > 20 ms (PROVISIONAL) |
| Stereo | ICC, M/S ratio, mono fold-down vs source at `W⁻¹` (400 ms blocks) ([STEREO_PHASE_PRESERVATION.md](STEREO_PHASE_PRESERVATION.md) §4) | \|ΔR_MS\| > 1 dB or fold-down loss > 0.5 LU |
| Loudness | integrated LUFS source vs render | \|Δ\| > 1 LU (info only; TSM typically −0.2 to −0.5 LU) |
| Peaks | sample peak / true peak | > 0 dBFS or > −1 dBTP → "Peaks over" (resolved at export gate) |

Path R uses the **DSP** tracker on purpose. It is independent of the model that made the anchors, so it does not share that model's biases (METRICS caveat). Its own error floor (≈ 5–10 ms) sets the thresholds.

## 4. Peak gate (pre-export)
```
peakDb   = samplePeak(gen);  tpDb = truePeak4x(gen)
limitHit = isLossy(fmt) ? tpDb > -1.0 : (isInteger(fmt) ? peakDb > 0.0 : false)
if isFloatWav(fmt) and peakDb > 0: warn("Peaks above 0 dBFS. Fine for float, may clip elsewhere")
if limitHit:
    choice = ask([LowerGain (default) | TruePeakLimiter (opt-in) | ExportAnyway])
    LowerGain:       g = (isLossy ? -1.0 - tpDb : -peakDb) - 0.05 dB margin ; gen *= dB(g)
    TruePeakLimiter: lookahead TP limiter, ceiling −1.0 dBTP (lossy) / −0.3 dBTP (PCM); report GR max
    ExportAnyway:    count clipped samples after quantisation; report in export summary
```
Batch mode (post-MVP) defaults to LowerGain. No path changes gain without a recorded choice (REQ-AUDIO-008).

## 5. V3: export validation
```
tmp = writeEncoded(...)
d   = decodeFresh(tmp)                      # same decoders as import, gapless on
check d.frames == expectedFrames            # ±0 (PCM/FLAC), ±0 after gapless trim (MP3)
check offset(d, quantised) == 0             # cross-correlation on 3 windows (start/mid/end)
check finite(d) and not unexpectedSilence(d)
check peak/TP within the gate decision
check FLAC: STREAMINFO MD5 == md5(quantised PCM)
check PCM: bit-exact vs quantised buffer (WAV/FLAC)
check MP3: SNR vs quantised > 25 dB (sanity, not quality)
if all ok: atomicRename(tmp, target) else delete(tmp); showFailure(reason)
```

## 6. What the user sees
- V1 failures: lane shows "Couldn't render 1:20–1:24" (Problem). Export is blocked until the span renders.
- V2 issues: quality chip `⚠ Review (n)` and an Issues list with "Go to" (MAIN_WINDOW §5). Wording states the observation and the likely cause, never jargon.
- V3: an export summary card (format, length, peak/TP, loudness delta, gate decision, "Validated ✓"). On failure: reason + [Retry] [Choose another format/location].

## Risks
- Path R false alarms on sparse or ambient material. Mitigation: only evaluate where source beat confidence ≥ Medium.
- The discontinuity detector can flag real hard transients. Mitigation: compare against the source at `W⁻¹`, and flag only *new* outliers.

## Tests
Fault injection per check (NaN chunk, truncated chunk, inserted click, +2 dB gain, 1-sample offset in the export, corrupted FLAC MD5). Each must raise exactly its own Issue, and V3 must refuse the rename.
