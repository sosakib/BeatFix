# ADR-001: Desktop Framework and Language

**Status:** Accepted, 2026-10-08. Open item: JUCE 9 EULA tier/price confirmation (**for legal review**).

## Context
BeatFix is a Windows-first, offline, single-track editor: decode → beat analysis (ONNX model + DSP) → warp map → offline multithreaded time-stretch → A/B preview → export. The realtime audio callback, the render pool and the UI must never block each other. Every quality time-stretch library (Signalsmith, Bungee, Rubber Band) and ONNX Runtime are C/C++. The brief (§17) requires that the beat detector, stretch engine, resampler and codec layer be replaceable, and (§39) forbids unnecessary frameworks. The owner has shipped both a JUCE plugin and a Tauri 2 app.

## Alternatives
Weighted scores out of 185 from [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §1 (judgement, not measurement):

| Option | Score | For | Against |
|---|---|---|---|
| **C++20 + JUCE 9** | **159** | One language from audio callback to stretch libs and ORT (no FFI on the render path); WASAPI devices, WAV/AIFF/FLAC/Ogg codecs, thread pool, D2D drawing built in; owner experience; CLI is a second target over the same core | Custom-drawn UI is slower to build; weaker accessibility than DOM/Qt; licence fee above US$20k revenue; AGPL otherwise |
| Dear ImGui (C++) | 144 | MIT, tiny, fast engineering views | No accessibility, no native dialogs, not a consumer UI |
| Qt 6.12 (C++) | 139 | Best C++ widget toolkit | LGPLv3 relink/installation-info duties or subscription; audio devices and codecs must be assembled by hand; no owner experience |
| Rust core + Tauri 2.12 | 137 | Fastest UI work, cheapest licences | Three languages (Rust + TS + C++ for stretch libs), FFI to C++ engines, `ort` still 2.0.0-rc.13, young stretch/beat crates, IPC for all edit state |
| .NET/WPF + NAudio | — | Easy on Windows | P/Invoke for every DSP lib; Windows lock-in |

Language sub-question: C++20 vs C++23 (`std::expected`). C++23 needs MSVC `/std:c++latest` with its churn.

## Decision
1. **C++20** throughout. Errors use a small `Result<T> = {T value; Error err;}` ([CODING_STANDARDS.md](../08_engineering/CODING_STANDARDS.md) §4). No C++23.
2. **JUCE 9.0.x** (pinned tag 9.0.3) for GUI, audio device I/O and WAV/AIFF/FLAC/Ogg, under the **JUCE commercial EULA** (Starter: free ≤ US$20k annual revenue/funding; then Indie). **Never the AGPL option.**
3. The engine (`bf_dsp`, `bf_analysis`) is plain C++20 + std with **no JUCE types**. `bf_io`/`bf_engine` may use JUCE privately in `.cpp` files only. A thin C-compatible API (`beatfix_c.h`) is the shell seam.
4. Fallback: **Tauri 2** shell over the same C API.

## Reasoning
- The hottest paths (render, analysis, audio callback) stay in one language with zero FFI.
- JUCE already contains most of what `bf_io` and playback need, which removes several dependencies (dr_libs, miniaudio, libsndfile).
- Keeping JUCE out of the engine's public headers turns the licence risk (revenue threshold, EULA changes) into a contained, known migration: swap the shell, not the DSP.
- `Result<T>` gives explicit error flow without exceptions across module boundaries and without waiting for C++23.

## Tradeoffs
- UI development is slower and more manual (custom `Component::paint`, `LookAndFeel`). Accepted for the lowest technical risk on the audio path.
- JUCE licence cost above $20k revenue (Indie $800 perpetual/seat [S]).
- `Result<T>` requires default-constructible `T` and is less ergonomic than `std::expected`.
- The Tauri fallback is not free: `bf_io`'s JUCE-backed readers/writers would need replacing (dr_libs + libFLAC).

## Consequences
- `app/` is the only target linking `juce_gui_*`. CI-checked rule: no JUCE include in `src/bf_*/**/*.h` public headers ([REPOSITORY_STRUCTURE.md](../08_engineering/REPOSITORY_STRUCTURE.md) §2).
- The licence tier is recorded in `third_party/manifest.json`; crossing $20k revenue/funding triggers the Indie purchase.
- Accessibility needs explicit work in JUCE (accessibility handlers, keyboard paths) per [WAVEFORM_VIEW.md](../06_ui/WAVEFORM_VIEW.md).
- A future CLI (Post-MVP) is a JUCE console target over `bf_engine` ([REPOSITORY_STRUCTURE.md](../08_engineering/REPOSITORY_STRUCTURE.md) §3).

## References
- [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §1 (option comparison and scoring), F2–F3, F15
- [LIBRARY_EVALUATION.md](../03_research/LIBRARY_EVALUATION.md) §A (frameworks), Decision Matrix
- [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) §2 (JUCE 9 licensing)
- [TECH_STACK.md](../08_engineering/TECH_STACK.md) §1, §3
- [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1
