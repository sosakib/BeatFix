# ADR-009: Codecs, Resampling and Export

**Status:** Accepted, 2026-10-08.

## Context
Beat alignment is destroyed by codec priming offsets: MP3 adds 1105 samples (≈ 25 ms) unless gapless info is honoured (measured, [AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md) F1). Time-stretching raises peaks (+1.25 dBFS measured on a −0.1 dBFS master). Repeated lossy encoding degrades audio. Licences: MP3 patents expired (2017). AAC is still licensed (Via LA). LAME and mpg123 are LGPL. libsamplerate tops out at 97 dB.

## Alternatives
| Area | Options | Chosen | Why |
|---|---|---|---|
| WAV/AIFF/FLAC/Ogg decode, WAV/FLAC encode | JUCE formats, dr_libs, libsndfile (LGPL), FFmpeg | **JUCE** | already a dependency, no extra licence |
| MP3 decode | minimp3_ex (CC0), dr_mp3, libmpg123 (LGPL), JUCE MP3 | **minimp3_ex**, gated by a 0-sample-offset test in Phase 0. Fallback libmpg123 DLL | explicit LAME/Xing delay/padding handling |
| MP3 encode | LAME (LGPL), FFmpeg | **LAME as a separate DLL** with LAME tag | standard, gapless tag, LGPL-compliant via dynamic linking |
| AAC/M4A, Opus | MF AAC, fdk-aac, libopus | **None in MVP** | licensing review. Suno delivers MP3/WAV |
| Resampler | r8brain (MIT), libsamplerate (BSD-2, 97 dB), soxr (LGPL, dormant), own polyphase | **r8brain** + contract wrapper (exact length, zero delay, ≥ 120 dB) | reuse. Own SRC is the fallback |
| Loudness / TP | libebur128 (MIT) | **libebur128** | BS.1770 reference implementation |

## Decision
Decode once (gapless) → cache float32 → render → peak gate (user choice, never silent) → SRC at most once → TPDF dither for integer outputs only (fixed seed) → encode once → tags (`BEATFIX_RENDER`, new BPM; stale timing tags stripped) → re-decode validation → atomic rename. Export defaults: WAV 24-bit, FLAC 24, MP3 CBR 320.

## Tradeoffs
No AAC/Opus for Apple- or streaming-oriented users in the MVP. LGPL DLL handling adds packaging work.

## Consequences
Click-track round-trip fixtures for every codec in CI. The LGPL relinking notice ships in NOTICE.

## References
[AUDIO_QUALITY_RESEARCH.md](../03_research/AUDIO_QUALITY_RESEARCH.md), [RESAMPLING.md](../04_algorithms/RESAMPLING.md), [DITHERING.md](../04_algorithms/DITHERING.md), [QUALITY_VALIDATION.md](../04_algorithms/QUALITY_VALIDATION.md), [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md), [AUDIO_PIPELINE.md](../05_architecture/AUDIO_PIPELINE.md).
