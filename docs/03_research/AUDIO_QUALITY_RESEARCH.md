# Audio Quality, Import & Export Research (Agent E)

Status: research, 2026-10-08. Scope: formats, codec delay/gapless, sample rate, precision, dithering, true peak, clipping, loudness, generation loss, source preservation, metadata, filenames, output validation, optional enhancement stage.
Companion algorithm docs: `docs/04_algorithms/RESAMPLING.md`, `docs/04_algorithms/DITHERING.md`.
Confidence labels: **H**igh / **M**ed / **L**ow. **UNVERIFIED** = not confirmed from a primary source in this pass. **MEASURED** = reproduced locally (ffmpeg 8.1.2 full build + numpy; script `docs/03_research/_evidence/agentE_codec_tsm_generation_exp.py`, raw run-1 JSON alongside (its E2 block is the non-band-limited variant; the band-limited E2 in §4.12 was a follow-up run: 16 kHz low-pass before the limiter, drive 12 dB); results summarized in §4.12).

---

## Objective

Specify how BeatFix gets audio in and out without degrading it more than the time-warp itself needs to, without hiding sample-level timing offsets that break beat alignment, and without claiming quality it cannot deliver.

## Why It Matters

- BeatFix's core promise is **sample-accurate beat alignment**. Codec priming/padding, an undecoded Xing frame, or a resampler's group delay each shift audio by 1–50 ms. That is enough to put a corrected track audibly off the DAW grid (a 25 ms flam on drums is clearly audible), and it can wreck the product's main claim without any error being reported.
- Most inputs are **already lossy** (Suno MP3/M4A), and outputs are often lossy again. Each extra generation adds coding noise and peak overshoot (MEASURED, §4.12). The pipeline must keep this to the single unavoidable re-encode.
- Phase-vocoder time-stretching **raises sample peaks** on already-limited masters (MEASURED +0.1…+1.35 dB). Loud AI masters sitting at −0.1 dBFS will clip on integer export unless clipping is detected and handled explicitly.
- Trust: the app must never imply that lossless export, upsampling or "HD" output restores information the source never had.

## Key Findings

| # | Claim | Evidence | Conf. | Implication / Recommendation |
|---|---|---|---|---|
| F1 | LAME MP3: encoder delay 576 samples + decoder delay 529 = **1105 samples** of leading offset. Padding at the end varies, so the frame count rounds up to 1152-sample frames. LAME writes `enc_delay`/`enc_padding` (12 bits each) into the LAME tag inside the first (Xing/Info) frame. | LAME tech FAQ (lame.sourceforge.io/tech-FAQ.txt; older text says 528/1057 for ISO encoders); HA forum/OpenMPT bug 725 (576+529=1105); **MEASURED**: untrimmed decode offset exactly 1105 at 44.1 k and 48 k for CBR320, V0, V2 | H | Decode MP3 with gapless trimming (LAME tag) **on**. Export MP3 **with** LAME tag. Without trimming the offset is 25.06 ms @44.1 k / 23.02 ms @48 k. |
| F2 | If the Xing/Info frame is decoded as audio instead of skipped, the decoder outputs an extra 1152 samples of silence (26.1 ms @44.1 k). | LAME tag design (the tag lives in a valid, silent MPEG frame); behavior of non-tag-aware decoders. UNVERIFIED for specific decoders. | M | Decoder must recognize Xing/Info/VBRI. Add a golden test (click-track MP3 → first click at the exact sample). |
| F3 | AAC priming: Apple uses a fixed **2112** samples; AAC packets are 1024 samples; the remainder is under 1024. Delay is signalled by the MP4 edit list or by iTunSMPB. | Apple TN2258; FFmpeg ML; **MEASURED**: ffmpeg native AAC primes 1024. Even with trimming, the decoded m4a was **+616 (44.1 k) / +768 (48 k) samples longer** at the end (end padding not trimmed). | H (2112/1024) / H (measured tail) | AAC import: honor edit list or iTunSMPB. AAC export validation must allow a tail of less than 1024 samples, or must write correct end trim. AAC stays post-MVP. |
| F4 | Opus: pre-skip in OpusHead is counted at 48 kHz. libopus/opusenc typically writes **312** (6.5 ms lookahead). Decoders must skip pre-skip and trim to the final granule position. RFC 7845 recommends ≥3840 samples (80 ms) of pre-roll when seeking. Opus always decodes at 48 kHz; the "input sample rate" field is informational only. | RFC 7845 §4–5; **MEASURED**: untrimmed offset 312 @48 k, 287 @44.1 k (= 312·44.1/48) | H | Opus export means resampling 44.1→48 k. Pre-skip trimming is mandatory on import. |
| F5 | Ogg Vorbis and FLAC are sample-exact when granule/STREAMINFO is honored. FLAC STREAMINFO carries an **MD5 of the unencoded PCM**. | RFC 9639; **MEASURED** 0 offset / 0 length diff (Vorbis untrimmed raw was +128) | H | Use the FLAC MD5 for export verification. |
| F6 | FLAC (RFC 9639, Dec 2024): 1–8 ch, 1–1,048,575 Hz, **4–32-bit integer** (32-bit encode since libFLAC 1.4.0). **No float.** | RFC 9639; xiph FLAC 1.4.0 release notes | H | FLAC export: 16/24-bit (dithered). A float source exported to FLAC must be quantized. |
| F7 | WAV: 16-bit stereo → `WAVE_FORMAT_PCM` (0x0001). >16-bit or >2 ch → `WAVE_FORMAT_EXTENSIBLE` (0xFFFE). Float → `WAVE_FORMAT_IEEE_FLOAT` (0x0003) + `fact` chunk. | Microsoft WAVEFORMATEXTENSIBLE guidance; **MEASURED** ffmpeg writes exactly these tags | H | Writer follows the same rules. Reader accepts all three plus the extensible float subtype. |
| F8 | RIFF sizes are 32-bit, so files cap at 4 GiB (some readers treat sizes as signed: 2 GiB). RF64 (EBU Tech 3306) / BW64 (ITU-R BS.2088) use `ds64` 64-bit sizes. Stereo f32 at 96 kHz hits 4 GiB at ~93 min. Stereo 24-bit at 48 kHz hits it at ~4.1 h. | EBU Tech 3306; arithmetic | H | Auto-switch to RF64 only when needed. Always read RF64/BW64. |
| F9 | MP3 (MPEG-1 L3) supports 32/44.1/48 kHz only (MPEG-2/2.5 add the lower rates), max 2 ch. | ISO 11172-3; ffmpeg libmp3lame "Supported sample rates" listing | H | A 88.2/96 kHz source exported to MP3 must be resampled to 44.1/48 k. Suggest 48 k for 96 k sources and 44.1 k for 88.2 k ones (simpler ratio). |
| F10 | True-peak (ITU-R BS.1770, Annex 2): oversample ×4 (at 48 kHz → 192 kHz) with a 48-tap, 4-phase FIR, take abs, report dBTP. Higher ratios are preferred. A 4× meter can under-read by up to ~0.69 dB near Nyquist. BS.1770-5 (11/2023) is current. | ITU rec page R-REC-BS.1770-5-202311; secondary summary (Fora Soft 2026) for the 48-tap/0.688 dB numbers | H (method) / M (numbers, secondary) | Use a BS.1770-compliant meter (libebur128 / Rust `ebur128`, both MIT). Consider 8× for the export-gate check. |
| F11 | Lossy-delivery ceiling **−1 dBTP**. AES TD1008 says lower bitrates need more headroom and that codec and SRC overshoots add up. Spotify uses −1 dBTP (−2 if louder than −14 LUFS). EBU R128 sets max −1 dBTP for broadcast. | AES TD1008 & Spotify via secondary; EBU R128 | M | Default true-peak gate: −1.0 dBTP for MP3/AAC/Opus/Vorbis exports, 0.0 dBFS sample peak for integer lossless. |
| F12 | **MEASURED**: phase-vocoder TSM (Rubber Band R2 via ffmpeg `rubberband`) on a band-limited, sample-peak-limited (−0.1 dBFS, −10.5 LUFS) master raised the **sample peak to +0.03…+1.25 dBFS** and true peak by up to +0.5 dB. Integrated loudness fell 0.2–0.5 LU. WSOLA (`atempo`) left the sample peak unchanged. | §4.12 E2 | M (synthetic signal, one engine) | TSM output must be peak-checked **after** render. Assume ≥1.5 dB of possible overs on loud masters. Re-run this test with the chosen engine (Signalsmith/Bungee/RB R3) on real Suno material. |
| F13 | **MEASURED**: repeated lossy generations degrade steadily. Over 10 generations, MP3 V2 waveform-SNR vs. original fell 15.9→11.4 dB, MP3 320 fell 18.4→16.1 dB, and AAC 256 (ffmpeg native) fell 16.8→9.2 dB. Decoded true peaks grew from +1.0 to +4.2 dBTP (MP3 V2, no clipping between generations). This matches the ITU-R 1993 MPEG-1 tandem tests and EBU Tech Review 304 on cascading. | §4.12 E3; AudioLabs AES coding tutorial "Tandem coding"; EBU trev_304 | H (direction) / L (absolute SNR values: waveform SNR is not perceptual) | Render from source every time. Never re-encode intermediates. Warn when a BeatFix-produced lossy file is re-imported. |
| F14 | **Suno output format: UNVERIFIED.** Only secondary sources exist: free tier gets MP3 (+ M4A video), Pro/Premier add WAV. Sample rate is claimed as 44.1 or 48 kHz. No official spec found. Even the secondary source advises inspecting each file. | musicmake.ai (explicitly disclaims), muapi.ai summaries | L | Make no assumptions. Probe every import (codec, SR, bit depth, bitrate, effective bandwidth) and report it. Treat WAVs from AI generators as possibly lossy-origin (a spectral cutoff may reveal this). |
| F15 | Symphonia (MPL-2.0, pure Rust) 0.6.x: gapless is **on by default** (`AudioDecoderOptions::gapless`). The 0.5.x API used `FormatOptions::enable_gapless` (default false). Formats include AAC-LC, ALAC, FLAC, MP3, Vorbis, WAV, AIFF, CAF, MP4, OGG, MKV. **Opus decode in Symphonia: UNVERIFIED (historically absent).** | Context7 /pdeljanov/symphonia GETTING_STARTED; docs.rs symphonia-core 0.6.1 | M | If Rust: Symphonia 0.6 for decode. Add libopus (BSD-3) for Opus. Check gapless in CI with golden click-track files. |
| F16 | Resampler libraries: libsamplerate (BSD-2) SINC_BEST is only **97 dB SNR / 97 % bandwidth**, float I/O only. libsoxr is LGPL-2.1+. r8brain-free-src is **MIT** (C++, double). rubato is **MIT** (Rust, f32/f64, FFT-synchronous + async sinc). | libsamplerate docs (Context7); soxr/r8brain repos; rubato Context7 | H | See RESAMPLING.md. Rust: rubato `Fft<f64>`. C++: r8brain. libsamplerate is not enough for 24-bit work. |

---

## Technical Details

### 4.1 Quality vocabulary (normative for UI copy and docs)

| Term | Meaning in BeatFix | Allowed claim | Forbidden claim |
|---|---|---|---|
| **Preserving quality** | Output carries the rendered signal with no loss beyond quantization (f32 WAV, or 24-bit with TPDF dither) | "Lossless export of the corrected audio" | "Original quality restored" |
| **Preventing degradation** | Avoiding needless lossy steps: decode once, float pipeline, one SRC, one dither, one final encode, render from source | "No intermediate re-encoding" | — |
| **Perceived enhancement** | EQ/excitation/loudness/stereo widening: changes the signal and sounds "better" subjectively | "Optional enhancement (alters audio)" | "Improves fidelity / recovers detail" |
| **Actual reconstruction** | Model-based estimation of samples destroyed by a *known* degradation (de-clip interpolation, de-click). Still an estimate. | "Repairs clipped peaks (estimated)" | "Recovers the original waveform" |
| **Upsampling** | Changes the sample grid only. Adds **zero** information; content above the source Nyquist is empty. | "Converted to 96 kHz for compatibility" | "Hi-Res", "HD", "higher quality" |
| **AI restoration / bandwidth extension** | A generative model synthesizes plausible content that was never in the source | "AI-generated high-frequency content (synthetic)" | "Restored", "recovered", "lossless" |

Rule: the UI shows **source effective bandwidth** (e.g. "content ends at ~16 kHz, likely lossy origin") next to export format choices. Exporting FLAC from an MP3 source shows: "Lossless container; source was lossy (MP3 320). Quality cannot exceed the source."

### 4.2 Pipeline (render/export chain)

```
source file (immutable, hashed)
 → decode ONCE (gapless-trimmed) → cached PCM f32, native SR, native channels   [project cache, keyed by PCM hash]
 → analysis branch (separate low-SR mono copy; never feeds render)
 → render: warp/TSM at native SR (f32 buffers, f64 accumulators/time)
 → [optional Enhancement stage — opt-in, post-MVP, separate toggle, bypass = bit-identical]
 → SRC once, only if export SR ≠ native SR (RESAMPLING.md)
 → gain stage (user gain / clip-policy gain / optional loudness match)
 → optional true-peak limiter (opt-in)
 → MEASURE: sample peak, true peak (BS.1770 ≥4×), LUFS-I, LRA, NaN/Inf, silence
 → export gate (clip policy decision, §4.7)
 → quantize: int target → TPDF dither (+ optional noise shaping 16-bit) ; float target → none ; lossy → feed float to encoder (no dither)
 → encode + metadata → temp file in target dir → VALIDATE (re-decode, §4.11) → atomic rename
 → export report (shown in UI; optional JSON sidecar)
```

This needs two passes (render, then measure, then quantize/encode), which is fine offline. A 10-min stereo 48 kHz render is 230 MB as f32 (600 × 48000 × 2 × 4 B): hold it in memory, or spill to a temp f32 file above a threshold.

### 4.3 Format matrix

**Import (MVP: all of these).** Decoding is cheap, and refusing a user's Suno file is the worst failure.

| Format | Variants to accept | Gapless / timing requirement | Candidate decoder (licence) |
|---|---|---|---|
| WAV | PCM u8/s16/s24/s32, float32/64, EXTENSIBLE, RF64/BW64 | sample-exact | Symphonia (MPL-2.0) / dr_wav (PD/MIT-0) / libsndfile (LGPL-2.1) |
| AIFF / AIFF-C | big-endian PCM, `sowt`, `fl32`/`fl64` | sample-exact; SR is an 80-bit extended float | Symphonia / libsndfile |
| FLAC | 4–32-bit, any SR | sample-exact; verify MD5 if present | Symphonia / libFLAC (BSD-3) |
| MP3 | MPEG-1/2/2.5 L3, CBR/VBR/ABR | **must** skip Xing/Info frame; trim `enc_delay+529` and `enc_padding−529` from LAME tag; iTunSMPB fallback | Symphonia / mpg123 (LGPL-2.1) / minimp3 (CC0) |
| M4A/AAC | AAC-LC (HE-AAC: UNVERIFIED support) | edit list (`elst`) or iTunSMPB; else assume nothing and flag "timing unverified" | Symphonia (AAC-LC) / Windows Media Foundation (OS) |
| Ogg Vorbis | — | granule-position trimming | Symphonia / libvorbis (BSD-3) |
| Opus | Ogg Opus | pre-skip + end trim; output 48 kHz | libopus + libopusfile (BSD-3) |

**MP3 without LAME tag** (non-LAME encoders): no reliable delay info. Do not trim. Mark the import "encoder delay unknown (typically ~1–2 granules, i.e. up to a few tens of ms; UNVERIFIED per encoder)". This affects only cross-app alignment, not internal consistency, because analysis and render share one decoded buffer.

**Export**

| Format | MVP? | Options | Default | Notes |
|---|---|---|---|---|
| WAV | **MVP** | 16-bit PCM, 24-bit PCM, 32-bit float | **24-bit** + TPDF | float: no dither, overs preserved but warned; auto-RF64 >4 GiB |
| FLAC | **MVP** | 16 / 24-bit, compression level 0–8 | 24-bit, level 5 (UNVERIFIED that 5 is libFLAC's default; level only affects size/speed) | lossless; writes MD5; Vorbis comments + PICTURE |
| MP3 | **MVP** | CBR 320, VBR V0, V2, CBR 256/192 | **CBR 320** | LAME tag always written. CBR chosen for exact seeking in DJ/DAW tools (VBR seeks through a 100-entry Xing TOC; DJ-app impact UNVERIFIED). Float input, no dither. Gate at −1 dBTP. LAME algorithm quality `-q 2` (= `-h`). |
| AIFF | post-MVP (cheap) | 16/24 PCM | 24 | ID3 chunk for tags |
| Opus | post-MVP | 96–256 kbps VBR | 192 | 48 kHz only; libopus BSD-3; R128 header gain = 0 |
| AAC/M4A | post-MVP | 192–256 kbps | 256 | encoder choice and patent licensing **for legal review** (see Risks) |
| Ogg Vorbis | post-MVP / maybe never | q6–q8 | — | low demand |

Channel handling: keep the source channel count (mono stays mono; MP3 supports at most 2 ch). Never upmix silently.

### 4.4 Sample-rate policy

- **Keep native SR** end to end: decode → TSM → export. The default export SR equals the source SR.
- Resample **only** when (a) the user picks a different export SR, or (b) the target codec forces it (MP3 >48 k source; Opus always 48 k). Resample **once**, after TSM and before measurement, dither and encode (ISPs become real sample peaks after SRC, so measure after it).
- The analysis branch (e.g. 22.05 kHz mono for Beat This!) uses its own resample. Its group delay must be compensated so that beat times in seconds are identical in both domains (RESAMPLING.md, test R-A1).
- No internal upsampling "for quality". It brings no TSM benefit worth the cost (Assumption, M).
- The UI never labels an upsampled export as higher resolution (§4.1).

### 4.5 Internal precision

| Quantity | Type | Why |
|---|---|---|
| Sample buffers (cache, render) | **f32** | 24-bit mantissa ≈ 144 dB dynamic range is above any DAC. Halves memory vs f64 (230 MB vs 461 MB for 10 min stereo 48 k). |
| Accumulators: overlap-add sums, filter MACs (SRC, true-peak, K-weighting), loudness/energy integration, RMS over a whole song | **f64** | Avoids error growth over 10⁷–10⁸ terms. BS.1770 K-weighting biquads are sensitive to low-frequency coefficient precision. |
| **Time / positions / warp map** | **f64 seconds or i64 samples + f64 fraction** | **f32 cannot represent integer sample indices above 2²⁴ = 16,777,216**. That is only **349.5 s at 48 kHz** (380 s at 44.1 kHz). A 6-minute song would get ±1–2-sample beat-position jitter, rising with length. This is critical for beat alignment. |
| Dither RNG, quantizer | f64 math → int | see DITHERING.md |

Decoders that output integers (FLAC/WAV) convert with scale 2^-(N-1). Int→float→int is then bit-exact (DITHERING.md §bypass).

### 4.6 Dithering policy (details in DITHERING.md)

- Dither **only** when reducing word length to an integer format: f32 render → 16/24-bit WAV/FLAC/AIFF. Use TPDF ±1 LSB (error RMS = Δ/2: −96.33 dBFS at 16-bit, −144.49 dBFS at 24-bit; MEASURED in numpy).
- **Never** dither float outputs, and never dither before a lossy encoder (feed float).
- Noise shaping: optional, 16-bit only, 44.1/48 kHz only. Default off (flat TPDF).
- Bypass: if every sample is already on the target integer grid (e.g. an identity render of a 16-bit source), write it bit-exact with no dither.
- Deterministic seeded PRNG, so the same project and settings produce byte-identical exports (golden tests).

### 4.7 Clipping & peak policy — "no silent limiting"

Internal float never clips. The export gate runs on the measured pre-quantization render:

| Target | Gate | If exceeded |
|---|---|---|
| Integer lossless (16/24) | sample peak > 0 dBFS (−1 LSB) | modal choice (below) |
| Float WAV | sample peak > 0 dBFS | **warn only** (values preserved). Message: "many players will clip". |
| Lossy (MP3/AAC/Opus/Vorbis) | true peak > −1.0 dBTP (configurable −0.1…−3) | modal choice |

Choices, each with a numeric preview:
1. **Lower gain by X dB** (X = overshoot + 0.1 dB margin). Linear, transparent, changes loudness by −X dB. *Recommended default; also the default for headless/batch.*
2. **Apply true-peak limiter** (opt-in). Report max gain reduction, number of limited regions, and total limited duration.
3. **Export anyway (clip)**. Count clipped samples and report them in the export report. Hard clip for integer; lossy encoders receive the overs.

The chosen action is recorded in the export report and in the project. Nothing happens silently. Rationale: F12 shows TSM routinely creates +0.1–1.3 dB overs on −0.1 dBFS masters, so this dialog will appear for most loud AI masters. The default must be safe and one click.

**True-peak limiter (opt-in) sketch.** Lookahead L = 1.5–5 ms, ceiling C (dBTP), release τr = 50–200 ms:
```
p[n]  = max over 4x(8x)-oversampled phases of |x| around n            // BS.1770-style interpolator
g_t[n] = min(1, C_lin / p[n])                                          // target gain
g_m[n] = min(g_t[n .. n+L])                                            // sliding-min over lookahead (monotonic deque)
g[n]   = attack: linear ramp reaching g_m before the peak; release: g[n] = min(g_m[n], g[n-1] + (1-g[n-1])*(1-exp(-1/(τr·fs))))
y[n]   = x[n-L] * g[n]   (latency L compensated → output aligned to input; verify 0-sample offset in tests)
verify: re-measure true peak ≤ C (+0.05 dB tolerance), else iterate ceiling −0.1 dB
```
Same gain applied to all channels (stereo link) to keep the image. Latency must be compensated exactly (beat alignment).

### 4.8 Inter-sample peaks after time-stretching — why

- A brick-wall-limited master holds its *samples* at ≤ ceiling, but its band-limited reconstruction already exceeds it (true peak > sample peak). MEASURED: the source had −0.1 dBFS sample peak and +1.0 dBTP.
- A **phase vocoder** resynthesizes each partial with re-propagated phases. The phase relationships the limiter exploited (peaks shaved where partials align) are lost and re-randomized. Transient phase resets (Bonada-style) re-cohere energy at onsets. Nothing bounds the output sample peak by the input's. Result: sample overs (MEASURED +0.13…+1.35 dB rise) and small true-peak changes (≤ +0.5 dB).
- **WSOLA/OLA** with non-negative crossfade weights that sum to 1 satisfies |w·a + (1−w)·b| ≤ max(|a|,|b|), so the sample peak cannot rise (MEASURED: unchanged). Equal-power crossfades (w_a² + w_b² = 1) break this bound.
- **SRC after TSM** turns inter-sample peaks into sample peaks (e.g. 48→44.1 k).
- Loudness after PV-TSM fell 0.2–0.5 LU (MEASURED), consistent with transient/phase smoothing. This is below the ~1 LU commonly treated as a just-noticeable difference (Assumption, M).

### 4.9 Loudness

- Measure BS.1770 / EBU R128: K-weighting, 400 ms blocks with 75 % overlap, absolute gate −70 LUFS, relative gate −10 LU. Also LRA (EBU Tech 3342), true peak and sample peak, for **source and output**. Show the deltas.
- **Default: preserve source loudness, i.e. unity gain, no normalization to any target** (−23 broadcast / −14 streaming targets are delivery-platform jobs, not BeatFix's).
- If |ΔLUFS-I| > 0.5 LU after render, show an info chip. Offer "Match source loudness" (gain-only, combined with the clip gate). Default **off** in MVP. Assumption: users expect "same track, fixed tempo", and TSM's −0.2…−0.5 LU drift is near inaudible.
- Library: `ebur128` crate (MIT, Rust port of libebur128) or libebur128 (MIT).
- Do not write ReplayGain/R128 tags in MVP. Strip stale ones from carried-over metadata (§4.10).

### 4.10 Source preservation, fingerprinting, metadata, filenames

**Immutability.** BeatFix never writes to the source path. It refuses an export whose canonical path equals the source path. On Windows, also compare the volume serial + file index (`GetFileInformationByHandle`) to catch hard links and aliases. The project stores an absolute path, a relative path, and the hashes.

**Fingerprints** (stored in the project):
- `file_sha256` (or BLAKE3, CC0/Apache-2.0, faster) over the file bytes. Detects *any* change, including tag edits.
- `pcm_hash` over the decoded, gapless-trimmed PCM as canonical interleaved little-endian i32 (lossless sources: native integer samples, decoder-independent) or f32 (lossy sources). Store with the `decoder name+version`, because lossy decoders may differ in LSBs across versions.
- Plus `sr`, `channels`, `frames`, `codec`, `bitrate`, `encoder_delay_trimmed`, `effective_bandwidth_hz`.
- Reopen logic: file hash equal → OK. File hash differs but PCM hash equal → "metadata changed, audio identical", keep analysis. PCM hash differs → invalidate the analysis cache and warn. File missing → relink by PCM hash (search the folder). Chromaprint (LGPL-2.1) acoustic fingerprint for fuzzy relink: post-MVP.
- **Re-import guard**: exports carry a tag `BEATFIX_RENDER=<project-id>;<pcm-hash-of-render>` (Vorbis comment / ID3 TXXX / RIFF INFO ICMT). Importing a BeatFix lossy export shows: "This is a BeatFix render. Open the original project to avoid another lossy generation."

**Metadata carry-over.** Read ID3v1/v2.2–2.4, APEv2, Vorbis comments, MP4 `ilst`, RIFF LIST/INFO, `id3 ` chunk in WAV/AIFF, and bext/iXML (read-only). Normalize to an internal set: title, artist, album, album artist, track/disc, year/date, genre, comment, lyrics (optional), ISRC, artwork (front cover).

| Target | Write as |
|---|---|
| MP3 | ID3v2.3 (widest compatibility; UTF-16) or v2.4 (UTF-8) option; APIC front cover; `TBPM` = target BPM |
| FLAC / Ogg / Opus | Vorbis comments (`BPM`), `METADATA_BLOCK_PICTURE` (base64 in Ogg) |
| WAV | LIST/INFO (INAM, IART, ICMT, …) + optional `id3 ` chunk (de-facto, widely read) |
| AIFF | `ID3 ` chunk |
| M4A | `ilst` incl. `covr`, `tmpo` |

**Strip** stale technical tags: ReplayGain/R128 gains, iTunNORM, iTunSMPB, LAME/encoder-settings tags, TLEN, any old gapless info, Serato/Traktor/rekordbox beatgrid blobs (GEOB etc.), because the timing changed. **Keep** provenance and comment fields (AI-generation notices, URLs) by default (ethical/transparency reason). Whether Suno embeds provenance tags or C2PA manifests is UNVERIFIED. Libraries: `lofty` (Rust; licence believed MIT OR Apache-2.0, UNVERIFIED) or TagLib (C++, LGPL-2.1/MPL-1.1).

**Filenames.** Default `{stem} [BeatFix {bpm} BPM].{ext}` (template editable). Windows rules:
- Strip `<>:"/\|?*` and control chars. Reject reserved base names (CON, PRN, AUX, NUL, COM1–9, LPT1–9, case-insensitive, with any extension). Strip trailing dots/spaces.
- NFC-normalize. Use wide (UTF-16) APIs only, since Suno titles often contain emoji/CJK.
- Long paths: `\\?\` prefix or `longPathAware` manifest.
- On collision append ` (2)`… unless overwrite is confirmed. Never overwrite the source.
- Write `name.ext.beatfix-tmp` in the same directory → validate → `MoveFileExW(MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)` (or `ReplaceFileW`). Delete the temp on cancel or failure.

### 4.11 Output validation (every export, automatic, ~1× real-time decode cost)

Re-open the written temp file through the **import path** (the same decoder users will hit) and check:

| Check | Lossless (WAV/FLAC/AIFF) | Lossy |
|---|---|---|
| container/codec/SR/ch/bit-depth match request | exact | exact |
| length vs expected | **exact** | exact for MP3 (LAME tag), Opus, Vorbis; AAC: 0 ≤ tail < 1024 (F3) |
| content identity | decoded ints == quantized buffer (hash compare); FLAC: STREAMINFO MD5 equals that hash | **alignment offset = 0 samples** by cross-correlation of the first 10 s and last 10 s vs the pre-encode buffer (catches priming bugs); correlation > 0.98 (assumption, tune) |
| NaN/Inf | none (float WAV) | none |
| peak | sample peak ≤ gate (or matches "clip anyway" count) | true peak ≤ gate + 0.5 dB codec allowance, else warn |
| silence | RMS > −90 dBFS unless the source was silent | same |
| loudness | |ΔLUFS| vs pre-encode buffer < 0.05 LU | < 0.3 LU |
| RF64 | `ds64` present iff data > 4 GiB − header | n/a |

On failure: keep the temp file only in debug mode, show a precise error, never rename it into place. Export progress shows phases (render → measure → encode → validate) and is cancellable at chunk granularity. Cancel removes the temp file.

### 4.12 Local experiments (evidence)

Setup: ffmpeg 8.1.2 (libmp3lame, native aac, libvorbis, libopus, flac, rubberband filter, ebur128 true-peak meter), numpy. Synthetic signals (click track; 120 BPM kick/snare/hats/saw-bass/pad mix, low-passed at 16 kHz, then limited with `alimiter` to −0.1 dBFS).

**E1 codec offset** (first-click position, samples; "untrimmed" = `-flags2 +skip_manual`):

| codec | 44.1 k trimmed / untrimmed | 48 k trimmed / untrimmed | length diff trimmed (44.1/48) |
|---|---|---|---|
| MP3 CBR320 / V0 / V2 | 0 / **1105** | 0 / **1105** | 0 / 0 |
| AAC 256 (ffmpeg native, m4a) | 0 / 1024 | 0 / 1024 | **+616 / +768** |
| Vorbis q6 | 0 / 128 | 0 / 128 | 0 / 0 |
| Opus 160 | 0 / 287 | 0 / **312** | 0 / 0 |
| FLAC | 0 / 0 | 0 / 0 | 0 / 0 |

**E2 TSM peaks** (source: −0.1 dBFS sample, +1.0 dBTP, −10.5 LUFS):

| engine | ratio range 0.90–1.10 | sample peak (dBFS) | true peak (dBTP) | LUFS |
|---|---|---|---|---|
| rubberband (R2, crisp, channels apart) | 0.90, 0.97, 0.99, 1.01, 1.03, 1.10 | +0.34 … **+1.25** | +1.0 … +1.5 | −10.7 … −11.0 |
| rubberband channels=together | same | +0.03 … +0.68 | +0.8 … +1.1 | −10.7 … −11.0 |
| atempo (WSOLA) | same | −0.1 (unchanged) | 0.9 … 1.0 | −10.5 |

A first run on a non-band-limited signal (true peak +2.4 dBTP) gave similar sample-peak rises (+0.1…+0.9 dB).

**E3 generations** (waveform SNR vs original, dB; peak not clipped between generations):

| gen | MP3 V2 | MP3 320 | AAC 256 (native) |
|---|---|---|---|
| 1 | 15.9 | 18.4 | 16.8 |
| 2 | 15.1 | 17.8 | 14.6 |
| 5 | 12.3 | 16.9 | 12.0 |
| 10 | 11.4 | 16.1 | 9.2 |
| true peak g1 → g10 | +1.0 → +4.2 dBTP | +1.5 → +2.6 | +2.7 → +3.5 |

Caveats: synthetic material. Waveform SNR is not a perceptual metric (PEAQ/ViSQOL/listening are needed for magnitude claims). One TSM engine (R2 via ffmpeg, not the engine BeatFix will ship). These numbers support the *direction* of the decisions, not specific thresholds.

### 4.13 Optional future Enhancement stage (NOT MVP)

Position: `Beat Sync → [Enhancement, opt-in, per-module toggles, A/B, bypass = bit-identical] → Export`.
- It never changes the warp map or timing. Any module with latency must be latency-compensated to 0 samples (tested).
- Every output is labelled with the §4.1 vocabulary.
- Runs only on the render, never on the analysis input.

| Module | Candidate (licence) | Fit for music | Notes |
|---|---|---|---|
| Denoise | RNNoise (BSD-3, xiph) | low: speech-trained, 48 kHz | for spoken intros only |
| Denoise | DeepFilterNet 1–3 (MIT OR Apache-2.0) | low–med: speech enhancement, full-band 48 kHz | Rust crate `deep_filter`; ONNX exports exist |
| Speech enhance/denoise | Resemble Enhance (MIT) | low: speech | — |
| Bandwidth extension | AudioSR (code MIT; weights licence UNVERIFIED) | med: music-capable, outputs 48 kHz with 24 kHz bandwidth, diffusion, heavy GPU, **generative = synthetic** | label "AI-generated HF" |
| Lossy-artifact restoration | Apollo (JusperLee; README badge **CC BY-SA 4.0**, GitHub reports NOASSERTION) | med: trained for MP3-compressed music | ShareAlike plus model licence: **for legal review**; likely unsuitable for proprietary bundling |
| Source separation (enabler for per-stem treatment) | Demucs (MIT) | high quality | large models; not enhancement per se |
| De-click / de-clip | Classical DSP: AR interpolation (Godsill/Rayner), sparse/consistent declipping (A-SPADE family) | med | implement in-house from papers; no dependency needed |
| Loudness tools | in-house gain + limiter (§4.7) | — | already partly needed for the clip gate |

---

## Alternatives Considered

| Decision | Chosen | Alternatives | Why not |
|---|---|---|---|
| Internal format | f32 buffers + f64 accumulation/time | all-f64; all-f32 | all-f64 doubles RAM for no audible gain; all-f32 breaks sample positions after 349 s |
| Default WAV bit depth | 24-bit + TPDF | 16 (matches most sources); 32f | 16 adds −96 dBFS of noise to an already-processed signal; 32f is less compatible and lets overs pass |
| Default MP3 | CBR 320 | V0 (smaller, similar quality) | beat-critical seeking/cue accuracy in DJ tools (Assumption, M). V0 offered. |
| Clip default | gain-reduce on prompt | auto-limit; auto-clip; normalize to target | the brief forbids silent limiting; normalization changes loudness intent |
| Loudness default | unity gain, report Δ | auto-match source LUFS | auto-match adds gain that could interact with the clip gate. Kept opt-in. |
| WAV >4 GiB | auto RF64 | always RF64; refuse | always-RF64 hurts compatibility |
| Resampler | rubato (Rust) / r8brain (C++) | libsamplerate, libsoxr, Speex, ffmpeg swr | libsamplerate: 97 dB. soxr/swr: LGPL (acceptable via dynamic linking but adds obligations). Speex: lower quality. |
| AAC encoder | post-MVP: Windows Media Foundation (OS component) | FDK-AAC; ffmpeg native; Apple via qaac | FDK licence is non-free plus patents; ffmpeg native has the end-trim issue (F3) and is weaker at low rates; Apple needs iTunes libs. MF encoder bitrate options UNVERIFIED (believed ≤192 kbps). |

## Evidence / Sources

- LAME tech FAQ (delay/padding): https://lame.sourceforge.io/tech-FAQ.txt
- 576+529=1105 (LAME delay in decoders): https://bugs.openmpt.org/view.php?id=725 ; https://hydrogenaudio.org/index.php/topic,61917.0.html ; https://libzplay.sourceforge.net/LAMETAG.html
- LAME/Xing tag spec (12-bit enc_delay/padding fields): http://gabriel.mp3-tech.org/mp3infotag.html (fetch failed this session; field widths from prior knowledge, M)
- Apple TN2258 AAC encoder delay (2112, 1024/packet, remainder < 1024): https://developer.apple.com/library/archive/technotes/tn2258/_index.html
- iTunSMPB discussion: https://github.com/uupaa/AAC.js/wiki/EncoderDelay ; https://lists.ffmpeg.org/pipermail/ffmpeg-devel/2012-July/127834.html
- RFC 7845 Ogg Opus (pre-skip, 80 ms preroll, output gain, R128 tags): https://www.rfc-editor.org/rfc/rfc7845.html
- opusenc pre-skip 312: https://code.videolan.org/videolan/vlc/-/issues/19242 ; ffmpeg libopusenc source
- RFC 9639 FLAC: https://www.rfc-editor.org/rfc/rfc9639.html ; FLAC 1.4.0 (32-bit int): https://xiph.org/flac/2022/09/09/flac-1-4-0-released.html
- ITU-R BS.1770-5 (11/2023): https://www.itu.int/rec/R-REC-BS.1770-5-202311-I ; true-peak/TD1008/Spotify summary (secondary): https://www.forasoft.com/learn/audio-for-video/articles-audio/true-peak-dbtp-inter-sample
- Tandem coding: https://audiolabs-erlangen.de/content/resources/aesCodingTutorial/tandem.html ; EBU Tech Review 304 cascading: https://tech.ebu.ch/files/live/sites/tech/files/shared/techreview/trev_304-cascading.pdf
- libsamplerate converter specs: Context7 /libsndfile/libsamplerate (docs/api_misc.md)
- rubato API: Context7 /henquist/rubato ; Symphonia API: Context7 /pdeljanov/symphonia ; docs.rs symphonia-core 0.6.1
- Licences (GitHub API, 2026-10-08): rnnoise BSD-3; demucs MIT; AudioSR MIT; r8brain MIT; libebur128 MIT; ebur128 crate MIT; Symphonia MPL-2.0; libvorbis BSD-3; libsndfile LGPL-2.1; DeepFilterNet MIT/Apache-2.0 (README); Apollo CC BY-SA 4.0 (README badge)
- Suno formats (secondary, low confidence): https://musicmake.ai/blog/suno-v5-audio-quality-khz-2026 ; https://muapi.ai/playground/suno-convert-to-wav
- Local experiments: ffmpeg 8.1.2 + numpy, `_evidence/agentE_*.py` (port to `tests/` as golden tests)

## Recommendation

1. **Decode once, gapless-correct, cache f32 PCM** keyed by PCM hash. Analysis and render share that one buffer. (H)
2. **MP3 import must honor the LAME/Xing tag and skip the Xing frame. MP3 export must write a LAME tag.** Golden click-track tests per codec assert a 0-sample offset. (H)
3. **f64 for every time/position/warp quantity**; f32 sample buffers; f64 accumulators. (H)
4. **Keep native SR; resample once at export only when required**; linear-phase, ≥120 dB stopband, delay-compensated (RESAMPLING.md). (H)
5. **TPDF dither only on float→int; never on float or lossy outputs; deterministic seed; bypass for already-quantized data** (DITHERING.md). (H)
6. **Measure after render+SRC: sample peak, BS.1770 true peak, LUFS-I, LRA. Gate: 0 dBFS (int), −1 dBTP (lossy). Explicit user choice (gain-reduce recommended / opt-in TP limiter / clip anyway); never silent.** (H)
7. **Preserve source loudness (unity gain) by default**; report ΔLUFS; matching is opt-in. (M)
8. **MVP export: WAV 16/24/32f, FLAC 16/24, MP3 CBR320/V0/V2.** MVP import: WAV/RF64, AIFF, FLAC, MP3, M4A-AAC-LC, Vorbis, Opus. (H)
9. **Validate every export by re-decoding** (length, alignment, hash/MD5, NaN, peak, silence, loudness) before the atomic rename. (H)
10. **Enhancement = separate opt-in post-stage, post-MVP**, honest labelling; Apollo/AudioSR weights **for legal review**. (H)

## Risks

- **Decoder delay mismatch** across the analysis/render/validation decoders → beat offsets. Mitigation: a single decode path plus golden tests per codec/encoder (LAME, FhG, iTunes MP3, Apple/FDK/ffmpeg AAC).
- **TSM overs on every loud AI master** → frequent clip dialogs (UX fatigue). Mitigation: remember the choice per project or user; batch default gain-reduce.
- **AAC/MP4 patent and encoder licensing** (Via LA pool; FDK licence): **for legal review**. MP3 patents are generally reported expired (2017): **for legal review**.
- **LGPL components** (LAME, mpg123, libsoxr, libsndfile, TagLib) need dynamic linking and relinking rights: **for legal review**. Generative model weights (Apollo CC BY-SA, AudioSR weights unknown): **for legal review**.
- Synthetic-only evidence for TSM peak growth and generation loss. Must re-measure with the shipping engine on real Suno files.
- Suno format assumptions are unverified. Formats may change; the probe-and-report design absorbs this.
- Float WAV with overs may clip in downstream tools (users blame BeatFix). Mitigation: a clear warning.

## Open Questions

1. Actual Suno download specs (MP3 bitrate/SR, WAV SR/bit depth, whether the WAV is lossy-origin with a ~16 kHz cutoff). Needs 10–20 real files probed (ffprobe + spectrum). **Owner: QA.**
2. Do target DAWs and DJ apps (Ableton, FL Studio, Logic, Serato, rekordbox) honor the LAME gapless tag? This decides whether to warn "MP3 may be offset ~25 ms in app X". UNVERIFIED.
3. The final TSM engine's peak-growth distribution on real material (sets the default gain-reduction expectations).
4. Does Symphonia 0.6 decode Opus, and how accurate is its AAC edit-list handling? (Test with golden files.)
5. Windows Media Foundation AAC encoder bitrates/quality and its licensing position for AAC export.
6. Should "match source loudness" become default-on after user testing?
7. `lofty` crate licence confirmation; TagLib vs lofty depending on the stack decision (C++/JUCE vs Rust/Tauri, not yet made).

## Implementation Implications

- Modules: `decode` (probe + gapless + hashing), `pcm_cache`, `resample` (offline, delay-compensated), `meter` (peak, true peak, R128, LRA, bandwidth estimate), `export_gate`, `quantize` (dither), `encode` (WAV/RF64, FLAC, MP3), `tags` (read/normalize/write/strip), `validate`, `fs_safe` (filename sanitize, temp+atomic rename, source-protection).
- Project file stores: source path(s), file hash, PCM hash plus decoder version, codec/delay info, effective bandwidth, export history (settings, gate decision, measured stats, output hash).
- Test assets to create: click tracks at 44.1/48/96 k encoded by LAME CBR/VBR, non-LAME MP3, AAC (ffmpeg/FDK/Apple if available), Vorbis, Opus, FLAC 16/24/32, WAV variants incl. EXTENSIBLE, float, RF64 (sparse >4 GiB file), AIFF-C. Plus a loud limited master for peak-gate tests.
- CI assertions: 0-sample offset per codec; exact lossless round-trip; dither statistics; SRC specs; export validation catches injected faults (truncated file, NaN, 1105-sample shift).
