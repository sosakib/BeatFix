# BeatFix

> **Precision Tempo Stabilization & Rhythmic Timing Correction for Windows**

BeatFix is a high-performance standalone audio application and CLI toolkit designed to analyze a track's rhythmic timing, detect tempo drift, and correct timing inconsistencies using state-of-the-art time-stretching with complete non-destructive fidelity.

---

## Key Features

- **Automated Rhythmic Analysis**
  - Dual-band transient onset envelope detector with dynamic programming beat tracking.
  - Automatic tempo estimation, section-by-section drift detection, and beat grid alignment.
  - Downbeat recognition and confidence scoring.

- **Non-Destructive Time-Stretching**
  - High-fidelity **Signalsmith Stretch** engine with phase-coherent stereo processing.
  - Multi-bar detent correction options: Strict (0 bars), 1 bar, 2 bars (Natural), 4 bars, 8 bars, or Global constant ratio.
  - Zero heap allocation churn in the real-time audio pipeline.

- **Universal Audio Format Support**
  - Native support for **WAV** (16-bit, 24-bit, 32-bit float), **FLAC**, **MP3**, and **AIFF**.
  - Gapless decode pipeline with full **Win32 Unicode (UTF-8 / UTF-16)** filename handling.
  - Explicit detection and clean rejection of unsupported lossy containers per engineering spec.

- **Elegant Modern Interface**
  - Native **drag-and-drop** audio loading with glowing drop overlay.
  - 60 FPS dual-waveform visualizer with synchronous A/B preview (Original vs. Corrected).
  - Beat markers, downbeat flags, warp boundary lines, and live playhead scrubbing.
  - Built-in synchronized metronome click generator.

- **Mastering-Grade Export Pipeline**
  - Atomic file commits with Windows transactional semantics (`MoveFileExW`).
  - Automatic peak gate headroom protection (-0.1 dBFS ceiling with automated gain reduction).
  - TPDF dithering for 16-bit and 24-bit output, unquantized 32-bit float.
  - Mandatory post-export re-decode validation and NaN/Inf sanitization.

- **CLI Automation (`bftool`)**
  - Fast headless batch processing: `probe`, `analyze`, and `fix`.

---

## Architecture Overview

```
BeatFix/
├── apps/
│   ├── beatfix/          # Standalone JUCE GUI application
│   │   ├── MainComponent # Transport, controls, and drag-and-drop target
│   │   ├── WaveformView  # 60Hz multi-layer peak pyramid visualizer
│   │   ├── AudioPlayer   # Real-time lock-free audio playback & metronome
│   │   └── ExportModal   # Export configuration and report view
│   └── beatfix-cli/      # Headless CLI toolkit (bftool)
├── src/
│   ├── bf_dsp/           # DSP algorithms, stretch engine, dither, units
│   ├── bf_analysis/      # Onset detection, tempo tracking, beat analyzer
│   ├── bf_engine/        # Session state, undo/redo history, warp planner
│   └── bf_io/            # Audio decoders (dr_wav, dr_flac, minimp3) & export
├── tests/                # Unit test suite (bf_tests)
└── third_party/          # dr_libs, minimp3, Catch2, Signalsmith Stretch, JUCE
```

---

## Getting Started

### Prerequisites

- **Windows 10 / 11 (x64)**
- **Visual Studio 2022** (MSVC v143 with C++20 support)
- **CMake 3.22+**
- **Ninja** build system

### Building

Open the Visual Studio x64 Developer PowerShell:

```powershell
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build all targets (GUI, CLI, Tests)
ninja -C build
```

Compiled binaries:
- `build/BeatFix_artefacts/Release/BeatFix.exe` — Standalone GUI Application
- `build/bftool.exe` — Headless Command-Line Tool
- `build/bf_tests.exe` — DSP & Audio Engine Unit Tests

---

## CLI Usage (`bftool`)

```powershell
# Inspect audio metadata and SHA-256
bftool probe "song.mp3"

# Analyze rhythmic tempo, confidence, and detected beats
bftool analyze "track.flac"

# Fix timing with target BPM and detent
bftool fix "input.wav" "output_fixed.wav" --bpm 124 --detent 3 --bitdepth 24
```

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **Space** | Play / Pause |
| **A** | Listen to Original Audio |
| **B** | Listen to Corrected Audio |
| **K** | Toggle Metronome Click |
| **Home** | Seek to Beginning |
| **End** | Seek to End |
| **Ctrl + Z** | Undo Timing Adjustment |
| **Ctrl + Y** | Redo Timing Adjustment |
| **Ctrl + O** | Open File Dialog |
| **Ctrl + S** | Save Project File |
| **Ctrl + E** | Open Export Dialog |

---

## License

Proprietary / MIT License. All DSP algorithms and audio engine code developed according to the BeatFix Engineering Specification.
