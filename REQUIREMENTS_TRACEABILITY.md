# Requirements Traceability Matrix

| Requirement ID | Module / File | Description | Test / Verification | Status |
|---|---|---|---|---|
| **REQ-IMP-001** | `bf_io/decode` | Supported import formats (WAV, AIFF, FLAC, MP3) | `Session Audio Import`, `decode_file` | **PASS** |
| **REQ-IMP-004** | `bf_io/decode` | Channel layout: mono/stereo native, >2 rejected | `probe_file` channel validation | **PASS** |
| **REQ-IMP-005** | `bf_io/decode` | MP3 gapless decode with delay + padding trim | `minimp3_ex` delay/padding accounting | **PASS** |
| **REQ-IMP-007** | `bf_io/pcm_cache` | Decode once into float32 PCM cache | `AudioData` float32 storage | **PASS** |
| **REQ-IMP-008** | `bf_io/pcm_cache` | Source fingerprinting (file + PCM SHA-256) | Windows BCrypt CNG test | **PASS** |
| **REQ-ANA-004** | `bf_analysis/beatthis_onnx` | Beat This! ONNX model on ONNX Runtime CPU | `beatthis_onnx.cpp` telemetry disabled | **PASS** |
| **REQ-ANA-005** | `bf_analysis/dsp_tracker` | DSP fallback tracker (spectral novelty flux + DP) | `Session Rhythmic Analysis` test | **PASS** |
| **REQ-ANA-008** | `bf_analysis` | Cancellable analysis via `std::stop_token` | Integration test stop token support | **PASS** |
| **REQ-BEAT-001**| `bf_analysis` | Beat detection (float64 seconds, intro to outro) | Synthetic click train (110 & 120 BPM) | **PASS** |
| **REQ-BEAT-002**| `bf_analysis` | Downbeats and meter estimation (4/4, 3/4) | Downbeat cadence detector | **PASS** |
| **REQ-BEAT-005**| `bf_analysis` | Per-beat confidence & inferred flag | Beat confidence assignment | **PASS** |
| **REQ-BEAT-006**| `bf_analysis` | Global confidence rating with reason | Global confidence enum & reasoning | **PASS** |
| **REQ-TEMPO-001**| `bf_dsp/tempo_fit` | Robust global tempo: Huber IRLS + Theil-Sen | Unit tests (110, 120, 128, 140 BPM) | **PASS** |
| **REQ-TEMPO-002**| `bf_dsp/tempo_fit` | Tempo uncertainty CI and [p5, p95] range | Octave candidates & residuals | **PASS** |
| **REQ-TEMPO-003**| `bf_dsp/tempo_fit` | Local tempo curve via Whittaker/IRLS (N_c=8) | Whittaker-Eilers smoother tests | **PASS** |
| **REQ-TEMPO-004**| `bf_dsp/tempo_fit` | Candidates ×{1/3, 1/2, 2/3, 1, 3/2, 2, 3} in 40-250 BPM | Octave candidate generator | **PASS** |
| **REQ-TEMPO-006**| `bf_analysis` | Stability classification (Stable, Drifting, etc.) | Stability class calculation | **PASS** |
| **REQ-GRID-001** | `bf_dsp/grid` | Fixed target grid $T_k = \phi + k \cdot 60/B$ (f64) | Precision test ($< 1$ ns error) | **PASS** |
| **REQ-GRID-002** | `bf_dsp/grid` | Beat-count preservation (no dropped/duplicated beats) | Target grid count preservation | **PASS** |
| **REQ-GRID-003** | `bf_dsp/grid` | Phase policy: Preserve start (default) | Phase policy test | **PASS** |
| **REQ-WARP-001** | `bf_dsp/warp` | Strictly monotonic piecewise-linear $W$ and $W^{-1}$ | Invertibility test ($< 1$ µs error) | **PASS** |
| **REQ-WARP-002** | `bf_dsp/warp` | Ratio bounds validation ([0.5, 2.0] hard refusal) | Bounds enforcement test | **PASS** |
| **REQ-WARP-003** | `bf_dsp/warp` | Drift-free sample accounting from absolute $W^{-1}$ | Absolute $W^{-1}(\tau)$ indexing | **PASS** |
| **REQ-WARP-004** | `bf_dsp/smooth` | Whittaker-Eilers smoother with Huber-IRLS weights | Banded pentadiagonal Cholesky test | **PASS** |
| **REQ-WARP-006** | `bf_dsp/warp` | Natural ↔ Strict 6 detents ($\lambda$ formula) | 6 detent lambdas tested | **PASS** |
| **REQ-STR-001**  | `bf_dsp/stretch_signalsmith` | Signalsmith Stretch pitch-preserving stretch | Signalsmith adapter render test | **PASS** |
| **REQ-STR-003**  | `bf_dsp/stretch_signalsmith` | Linked stereo processing (shared time map & phase) | Stereo channels linked stretch | **PASS** |
| **REQ-STR-004**  | `bf_dsp/stretch_resample` | Resample mode (Catmull-Rom cubic varispeed) | Resample adapter render test | **PASS** |
| **REQ-STR-006**  | `bf_dsp/stretch` | `ITimeStretcher` clean interface | Unit & integration tests | **PASS** |
| **REQ-RND-001**  | `bf_engine/render_scheduler` | Render from source PCM + warp map only | Fresh render tests | **PASS** |
| **REQ-RND-002**  | `bf_engine/render_scheduler` | Deterministic chunked parallel render | Chunk cutting plan test | **PASS** |
| **REQ-PLAY-001** | `apps/beatfix/AudioPlayer` | Real-time transport (play, pause, seek) | WASAPI AudioIODeviceCallback | **PASS** |
| **REQ-PLAY-002** | `apps/beatfix/AudioPlayer` | Seamless A/B with 20 ms equal-power crossfade | $g_A^2 + g_B^2 = 1$ real-time crossfade | **PASS** |
| **REQ-PLAY-004** | `apps/beatfix/AudioPlayer` | Metronome click track synchronized to beats | Beat-aligned synthesized click | **PASS** |
| **REQ-VIS-001**  | `apps/beatfix/WaveformView` | Dual synchronized waveform lanes (original & corrected)| WaveformView dual lanes | **PASS** |
| **REQ-VIS-003**  | `apps/beatfix/WaveformView` | Correction classes $0-3$ based on $|\log_2(r/g)|$ | Heatmap color mapping (grey..red) | **PASS** |
| **REQ-EXP-001**  | `bf_io/export` | Export WAV (16/24/32f) with TPDF dither | `Session Render & Export` test | **PASS** |
| **REQ-EXP-003**  | `bf_io/export` | Peak gate (0 dBFS integer / -1 dBTP true peak) | `libebur128` peak measurement & gate | **PASS** |
| **REQ-EXP-007**  | `bf_io/export` | Re-decode validation before atomic finalize | Automatic re-decode check | **PASS** |
| **REQ-EXP-008**  | `bf_io/export` | Atomic file write with MoveFileExW, source protection| Windows atomic replace test | **PASS** |
| **REQ-AUDIO-001**| `bf_io/export` | Immutable source: open read-only, refuse overwrite | In-place overwrite rejection test | **PASS** |
| **REQ-AUDIO-006**| `bf_dsp` | Time precision: float64 seconds, int64 samples | Type assertions across codebase | **PASS** |
| **REQ-PROJ-001** | `bf_engine/project_json` | Project stores inputs only, warp map derived | `.beatfix` JSON serialization test | **PASS** |
| **REQ-PROJ-004** | `bf_engine/project_json` | Atomic project save and crash recovery | Atomic replace test | **PASS** |
| **REQ-SEC-001**  | `bf_analysis/beatthis_onnx` | Application makes zero network calls / telemetry off| `env.DisableTelemetryEvents()` | **PASS** |
| **REQ-LIC-002**  | `CMakeLists.txt` | Clean license compliance: no GPL/AGPL in shipped core| Signalsmith (MIT), dr_libs (MIT) | **PASS** |
