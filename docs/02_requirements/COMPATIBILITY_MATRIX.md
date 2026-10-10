# Compatibility Matrix

Sources: [AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md), [LIBRARY_EVALUATION.md](../03_research/LIBRARY_EVALUATION.md), [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md). "Tested" means a fixture runs in CI. "Supported" means it should work but has no fixture.

## 1. Platform

| Item | MVP | Notes |
|---|---|---|
| Windows 11 x64 | **Tested** | primary |
| Windows 10 22H2 x64 | Supported | Windows 10 mainstream support has ended; it is kept while JUCE 9 and ORT support it. **UNVERIFIED** minimum for JUCE 9, check in Phase 0 |
| Windows on ARM64 | Not supported | x64 emulation may work (**UNVERIFIED**). Native ARM64 is post-MVP (ORT and Signalsmith build for ARM64, JUCE supports it) |
| macOS / Linux | Not supported | architecture keeps the core portable (no JUCE types in `bf_dsp`/`bf_analysis`) |
| CPU | x64 with SSE4.2 required. AVX2 used when present | ORT CPU EP dispatches by ISA. The SIMD baseline of the release build is set in BUILD_AND_DEPENDENCIES. AVX2-only builds are **not** required |
| RAM | 8 GB minimum, 16 GB reference | REQ-PERF-007 |
| Display | 1024×640 minimum, 100–200 % scaling | per-monitor DPI v2 |

## 2. Audio devices

| Driver | MVP | Notes |
|---|---|---|
| WASAPI shared | **Tested** | default |
| WASAPI exclusive | Supported | setting |
| DirectSound | Supported via JUCE | fallback only |
| ASIO | Post-MVP | Steinberg ASIO SDK licence terms need review. Not needed for offline editing |

## 3. Import formats

| Format | Variants | Rates (kHz) | Bit depth | Channels | MVP |
|---|---|---|---|---|---|
| WAV / RF64 / BW64 | PCM u8/s16/s24/s32, float32/64, EXTENSIBLE | 44.1, 48, 88.2, 96, 192 tested. Others up to 192 supported | as listed | 1–2 | **Tested** |
| AIFF / AIFF-C | PCM, float (`fl32`) | same | 8–32 | 1–2 | **Tested** |
| FLAC | all bit depths | same | 16, 24 tested | 1–2 | **Tested** |
| MP3 | MPEG-1/2/2.5 L-III, CBR/VBR/ABR, LAME/Xing gapless | 32–48 | — | 1–2 | **Tested** (gapless 0-offset gate) |
| Ogg Vorbis | — | up to 192 | — | 1–2 | Supported |
| AAC / M4A / ALAC | — | — | — | — | **Rejected** (licensing review; post-MVP) |
| Opus | — | — | — | — | **Rejected** (post-MVP) |
| > 2 channels | any | — | — | 3+ | **Rejected** with message |
| Duration | — | — | — | — | ≤ 20 min guaranteed, ≤ 60 min supported, > 60 min refused (PROVISIONAL, owner decision) |

## 4. Export formats

| Format | Options | Default | Notes |
|---|---|---|---|
| WAV | 16 / 24 / 32-float, source rate or chosen rate | 24-bit, TPDF dither | RF64 automatically above 4 GiB (**UNVERIFIED** in JUCE 9 writer, so test it) |
| FLAC | 16 / 24, level 5 | 24 | MD5 checked on validation |
| MP3 | CBR 320, VBR V0, V2 | CBR 320 | 32/44.1/48 kHz only. Higher sources are resampled once (r8brain). LAME tag written |
| AIFF, Ogg, Opus, AAC | — | — | Post-MVP |

## 5. Interoperability (consumers of exports)

| App | Concern | Status |
|---|---|---|
| Ableton, FL Studio, Logic, Cubase, Studio One, REAPER | Do they honour the LAME gapless tag on MP3 import? An ignored tag offsets audio by about 25 ms | **UNVERIFIED**. Recommend WAV/FLAC for DAW use (EXPORT_UI hint) |
| Serato, rekordbox, Traktor | Old beatgrid tags are stripped on export, so DJ apps re-analyse | by design |
| DAW-ready start | The first downbeat on a bar line from t = 0 at tempo B | REQ-GRID-004 |
| BPM tag | ID3 `TBPM` / Vorbis `BPM` written as an integer when B is an integer, otherwise a decimal (**UNVERIFIED** app support for decimals) | — |
