# Open Questions

IDs (O-, L-, M-) are referenced from IMPLEMENTATION_ROADMAP.md and MVP_DEFINITION.md. Deduplicated from every doc's "Open Questions" section (2026-10-10). Each item lists why it matters, which phase it blocks, and a **proposed default** so work can continue. Track docs keep their own lists. This file is the master.

## A. Resolved during synthesis (recorded so they are not reopened)

| Question (source) | Resolution |
|---|---|
| JUCE 8.0.15 vs 9.0.x (ARCH Q1) | **JUCE 9.0.x**, commercial tier; JUCE 9 EULA still goes to legal review (L-1) |
| Beat This in MVP or DSP-only first (ARCH Q2) | **Beat This in MVP**, DSP tracker as fallback/cross-check |
| C++20 vs C++23 `std::expected` (ARCH Q4) | **C++20**, own `Result<T>` |
| Cache budget/location (ARCH Q5) | **5 GB LRU** in `%LOCALAPPDATA%\BeatFix\cache` |
| Export from chunked or single-pass render (ARCH Q7) | **Chunked**. Single-pass is the test reference/fallback; the null test gates it |
| A/B default (ARCH Q8) | **Same musical position** via W/W⁻¹, 20 ms equal-power crossfade |
| UI stack JUCE vs WebView2 (UI Q4) | JUCE (ADR-001) |
| Per-region modes in MVP (UI Q6) | No (Post-MVP) |
| Does the engine process stereo jointly (METRICS) | Yes: Signalsmith links phase per bin (verified in source, TIME_STRETCH_RESEARCH) |
| Tail policy beyond last beat (METRICS O17, UI Q5) | Keep the tail. W extends with the edge ratio; output length = `W(end)` |
| Rust-stack items (Symphonia Opus, lofty licence) | Moot: C++ stack chosen |
| Stepped-region default (TEMPO Q5) | **Flag and ask** (keep r = 1 / flatten); never auto-smear |
| N_c unit after ×2 relabel (HALF_DOUBLE Q1) | N_c is in beats of the **chosen** level; detent labels are in bars, so musical meaning is kept |
| "Kept section" slot rule (TEMPO_STABILITY Q3) | Added to TARGET_BEAT_GRID §2.1 |
| Output origin when W(0) ≠ 0 (TSE Q3) | Added to TARGET_BEAT_GRID §5.1: output starts at source start; shift reported |

## B. Owner decisions

| ID | Question | Why it matters | Blocks | Proposed default |
|---|---|---|---|---|
| O-1 | Business model (free / one-time / subscription)? | JUCE tier; whether Rubber Band commercial (£590 with credit / £1,490) is worth it | Phase 8 (release) | one-time paid, JUCE Starter→Indie |
| O-2 | Legal entity and country | code-signing options (Artifact Signing is US/CA individuals only) | Phase 8 | buy an OV certificate |
| O-3 | Max supported duration | memory design, PERF-LONG tests | Phase 1 | ≤ 20 min guaranteed, ≤ 60 min supported, > 60 refused |
| O-4 | Reference laptop (REF-MID) model | perf targets | Phase 0 | 4C/8T AVX2, 16 GB, mid-2024 class |
| O-5 | Listening panel (who, paid formal panel?) | release gate | Phase 4/8 | owner + 5–10 collaborators; formal panel only if results are borderline |
| O-6 | Halftime genre default label (backbeat-slow vs producer-fast) | first impression on trap/hip-hop | Phase 3 | highest-scoring candidate (prior near 120 tends to the faster label), alternatives one click away |
| O-7 | DAW-ready start pads to bar 1 or bar 2 | DAW workflow | Phase 7 | next bar line ≥ warped pre-roll; setting |
| O-8 | Should "match source loudness" default on? | loudness after stretch (−0.2…−0.5 LU) | Phase 5 | off; report delta |
| O-9 | Beat This full (≈ 80 MB) vs small (≈ 8 MB) model | installer size vs accuracy | Phase 3 | full, unless the Phase 0 perf gate fails |
| O-10 | Is 6/8 / unusual meter support needed beyond a flag? | scope | Post-MVP | flag only |

## C. Legal review (not legal advice)

| ID | Question | Blocks |
|---|---|---|
| L-1 | JUCE 9 EULA: tiers, revenue/funding definition, seats; using a JUCE-free core | release |
| L-2 | Beat This! training-data caveat for commercial inference | release |
| L-3 | Fraunhofer US 9,230,558 B2 family vs HPSS hybrid / phase reset | Phase 1.5 |
| L-4 | LGPL compliance package (LAME, possibly libmpg123, TagLib if adopted) | release |
| L-5 | Suno ToS item 10: testing with users' Suno tracks; fitting calibration on Suno outputs; free-tier tracks | Phase 0 corpus, Phase 8 calibration |
| L-6 | Research datasets (Ballroom/Hainsworth NC, GTZAN/SMC unlicensed, RWC) for internal benchmarking | Phase 3 benchmarks (local only meanwhile) |
| L-7 | Rubber Band GPL as an internal-only reference (not distributed) | Phase 0 |
| L-8 | "BeatFix" trademark clearance; comparative marketing vs NewTime/Ableton | release |
| L-9 | AAC (Via LA / Media Foundation) if M4A import/export is ever added | Post-MVP |
| L-10 | Inno Setup commercial-use terms; WiX fee if switched | release |
| L-11 | Licences of the IEEE DataPort TSM dataset and zygurt/TSM / OMOQ code (internal research) | optional |
| L-12 | madmom-style DBN reimplementation patent claim (low confidence) | only if a DBN is reimplemented |

## D. Needs measurement (spike or phase answers it)

| ID | Question | Answered in | Default until then |
|---|---|---|---|
| M-1 | Signalsmith alignment with variable chunks / `outputSeek` with non-unity rate (< 0.5 ms?) | Phase 0 (TSE-1) | assume OK, block Phase 4 if not |
| M-2 | Signalsmith vs R3 vs Bungee audibility at ≤ 3 % on AI masters | Phase 0 listening | Signalsmith |
| M-3 | Chunk seam audibility, overlap O (50–100 ms) | Phase 0 / TSE-6 | 80 ms |
| M-4 | Bit-exact pass-through at r = 1? | Phase 0 | −90 dBFS null requirement |
| M-5 | minimp3_ex gapless 0-offset | Phase 0 | fallback libmpg123 |
| M-6 | Beat This CPU time on REF-MID; ORT run-to-run determinism | Phase 0 | persist activations |
| M-7 | Real Suno file specs (SR, bitrate, lossy-origin WAV) from 10–20 files | Phase 0 | 44.1/48 kHz MP3/WAV |
| M-8 | Suno drift statistics (ramps, steps, splices, by version/genre); integer-BPM hypothesis | Phase 3 (≥ 50 annotated songs) | Natural N_c = 8; round() target |
| M-9 | Beat This localisation error on clean electronic material (need for onset snap) | Phase 3 | snap enabled |
| M-10 | 3-seed activation averaging benefit | Post-MVP experiment | single seed |
| M-11 | HPSS parameters for AI masters | Phase 1.5 | DSP_RESEARCH defaults |
| M-12 | Audible ICC/IPD change thresholds on wide masters | Phase 4 | PROVISIONAL values |
| M-13 | Peak-growth distribution of the shipped engine on real material | Phase 5 | peak gate handles any value |
| M-14 | Is the SuperFlux μ > 1 (200 fps) needed for Suno vocals | Phase 3 | 100 fps |
| M-15 | JUCE 9 writers' tag coverage (FLAC/WAV), RF64 > 4 GiB, MMCSS / WASAPI rate handling | Phase 1/5 | TagLib DLL if needed |
| M-16 | Do DAWs/DJ apps honour the LAME gapless tag? | Phase 5 (manual test) | export card recommends WAV/FLAC for DAWs |
| M-17 | Does Suno Manual BPM change downloaded audio? Hidden tempo-locked WAV? | competitive check, any time | assume no |
| M-18 | SoX noise-shaping coefficients at 48 kHz | Phase 5 | shaping off (default) |
| M-19 | Calibration of confidence and margin weights (c_irr, c_meter, w_d, w_b, m_hi, m_lo, θ_kick) | Phase 8 | placeholders in config |

## E. Research and product gaps
- User research: DAW ownership (P1 vs P2), MP3 vs WAV from Suno, acceptable "Fix" time, price expectation. 5–8 interviews before Phase 7 UI polish.
- Neural TSM (2024–2026) not surveyed. Revisit after MVP.
- Competitor tools (Neural Analog, Wondera) not benchmarked. Run them on the C/U corpus when available.
- Stems demand (linked warp for stems, D8) for v1.x prioritisation.
