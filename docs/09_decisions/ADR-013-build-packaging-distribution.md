# ADR-013: Build, Packaging and Distribution

**Status:** Accepted, 2026-10-08.

## Context
Windows-first, offline, small team. Dependencies: JUCE 9, Signalsmith, ONNX Runtime 1.30 (prebuilt), r8brain, minimp3, libebur128, LAME (LGPL DLL), Catch2. vcpkg lags upstream for JUCE (8.0.7), ORT (1.23.2) and Signalsmith (1.1.0). Code-signing has changed: EV certificates no longer bypass SmartScreen, and new certificates are capped at 460 days (since 2026-03-01).

## Alternatives
| Area | Options | Chosen | Why |
|---|---|---|---|
| Build | CMake + vcpkg for all; CMake + FetchContent; Conan | **CMake ≥ 3.28 + FetchContent pinned by tag + SHA256; vcpkg only for mp3lame** | current versions without waiting on vcpkg ports |
| CI | GitHub Actions `windows-latest`; self-hosted | **GitHub Actions** (+ ASan job, licence check, perf smoke) | standard, free tier |
| Installer | Inno Setup, WiX v6 (maintenance fee > $10k revenue), NSIS, MSIX | **Inno Setup per-user + portable zip** (MVP); **MSIX via Microsoft Store** (Phase 2: free signing and updates) | simplest signed installer. MSIX later |
| Updates | auto-updater; Store; manual | **No in-app updater** (offline-first, REQ-SEC-001). Release notes page; Store updates in Phase 2 | no network code |

## Decision
See [BUILD_AND_DEPENDENCIES.md](../08_engineering/BUILD_AND_DEPENDENCIES.md), [CI_CD.md](../08_engineering/CI_CD.md), [RELEASE_STRATEGY.md](../08_engineering/RELEASE_STRATEGY.md). Binaries and installer are code-signed (entity/country decides the options, **owner decision**). The ONNX model is hash-pinned and installed beside the exe.

## Tradeoffs
FetchContent pins mean manual version bumps. Inno Setup asks commercial users to buy a licence (check terms, **for legal review**).

## References
[ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §11–14, [LIBRARY_EVALUATION.md](../03_research/LIBRARY_EVALUATION.md).
