# Product Scope

Status: 2026-10-08, v1.0. Authority: [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §4–5 (MVP and not-MVP lists). This file classifies **every** feature named in brief §2 (workflow), §9 (stretch modes), §14 (enhancement), §15 (import/export) and §40 (extensibility), plus the competitor-derived opportunities D1–D12 ([COMPETITIVE_ANALYSIS.md](COMPETITIVE_ANALYSIS.md)). MVP features get an `F-xxx` ID in [FEATURE_SPECIFICATION.md](FEATURE_SPECIFICATION.md).

## Classification

| Class | Meaning | Gate to start |
|---|---|---|
| **MVP** | Ships in v1. DECISION_SUMMARY §5 | — |
| **Phase 1.5** | First increment after MVP. Already designed, gated on a measured result | Named test passes |
| **Phase 2** | Planned post-MVP product work | MVP shipped and user feedback |
| **Phase 3** | Planned later, or blocked on a licence/legal decision | Named decision |
| **Experimental** | Research needed. May never ship | Named experiment |
| **Future** | Architecture keeps the seam open. No plan | — |
| **Never** | Out of product scope by principle ([PRODUCT_VISION.md](PRODUCT_VISION.md) §5) | — |

These are **release** phases. They are not the implementation phases 0–9 of [IMPLEMENTATION_ROADMAP.md](../10_implementation/IMPLEMENTATION_ROADMAP.md).

Scope rule: a feature moves up a class only through an ADR or a DECISION_SUMMARY update. "It's small" is not a reason (brief §34).

---

## 1. Core workflow (brief §2)

| # | Brief step | Class | MVP form / note | F-ID |
|---|---|---|---|---|
| 1 | Import an audio file | MVP | One file per window. Drag-drop or Open | F-001 |
| 2 | Analyse the complete song | MVP | Beat This! ONNX + owned decoder. DSP fallback | F-003 |
| 3 | Detect BPM / beats / transients / confidence | MVP | Beats, downbeats, per-beat + global confidence. Transient strength is an analysis feature, not a UI feature | F-003, F-004, F-006 |
| 4 | Detect where tempo becomes inconsistent | MVP | Stability classes + change-point / splice flags | F-007 |
| 5 | Show analysis results | MVP | Control strip, tempo graph, Issues list | F-006, F-007, F-019 |
| 6 | Choose / type target BPM | MVP | One-click suggestion = nearest integer, exact value shown | F-009 |
| 7 | Generate fixed-BPM target grid | MVP | Single target BPM per song | F-010 |
| 8 | Compute timing correction | MVP | Monotone piecewise-linear W, Natural↔Strict | F-010, F-011 |
| 9 | Render corrected version | MVP | Signalsmith "Smooth" default. "Resample" for constant-ratio material | F-013, F-014 |
| 10 | Show original + corrected waveforms | MVP | Dual lanes linked through W | F-015 |
| 11 | Indicate stretched / compressed / unchanged regions | MVP | Class on `|log2(r/g)|`, signed strip, hover numbers | F-016 |
| 12 | Preview + A/B | MVP | Same musical position, 20 ms equal-power crossfade | F-017 |
| 13 | Change BPM/mode without reprocessing an encoded output | MVP | Render from decoded source every time. Chunk cache | F-013, F-022 |
| 14 | Export WAV, FLAC, MP3 | MVP | See §4 | F-020 |
| 14b | "…and other practical formats" | Phase 2 | AIFF, Opus export. See §4 | — |
| 15 | Preserve maximum source quality | MVP | Native SR, one SRC max, one encode, validation | F-020, F-021 |

## 2. Analysis and correction features

| Feature | Class | Note / reason |
|---|---|---|
| Ranked tempo hypotheses ×{1/3,1/2,2/3,1,3/2,2,3}, 40–250 BPM (D3) | MVP | DECISION_SUMMARY §6. Octave errors are common (TEMPO_ESTIMATION_RESEARCH T2) |
| Beat-level edits: ×2/÷2, downbeat shift / set bar 1, meter 3/4 vs 4/4 | MVP | DECISION_SUMMARY §4 allows beat-level edits only |
| Accept / reject detected change points | MVP | DECISION_SUMMARY §4 |
| Intentional tempo change: flag + ask "flatten" or "keep section unwarped (r = 1)" (D9 basic) | MVP | DECISION_SUMMARY §6. Never auto-smear |
| Natural↔Strict, 6 detents (Global, 32, 16, **8**, 4, Strict beats) (D4) | MVP | Brief §40 "quantization strength". DECISION_SUMMARY §6 |
| Click / metronome overlay at detected beats (A) or grid slots (B) | MVP | Main verification tool for P1 ([PLAYBACK_AND_AB.md](../06_ui/PLAYBACK_AND_AB.md) §2). Never exported |
| Phase policy: Preserve start / DAW-ready start (first downbeat on a bar line) (D10 part) | MVP | [TARGET_BEAT_GRID.md](../04_algorithms/TARGET_BEAT_GRID.md) §3 |
| Manual warp-marker dragging / anchor editing | Phase 2 | DECISION_SUMMARY §4. Keyboard shortcuts reserved ([KEYBOARD_SHORTCUTS.md](../06_ui/KEYBOARD_SHORTCUTS.md) "Post-MVP") |
| User-drawn exclusion region ("leave as is") outside detected change points | Phase 2 | Generalises the MVP flag/ask. Needs region UI |
| Per-section target BPM ("own tempo") | Phase 2 | DECISION_SUMMARY §3.6, §4 |
| Tap tempo / manual BPM for beatless material | Phase 2 | MVP shows "No steady beat" and disables export ([UX_BEHAVIOR.md](../06_ui/UX_BEHAVIOR.md) §4.4) |
| Drum stem as analysis source, warp the mix with the same map | Phase 2 | Cheap, likely precision gain. Magnitude UNVERIFIED (BEAT_TRACKING_RESEARCH §8) |
| Target tempo ramps (accelerando to a target curve) | Future | Brief §40. W supports it mathematically. No user demand evidence |
| Groove templates / swing | Future | DECISION_SUMMARY §4. Natural already keeps source groove |
| In-app source separation (Demucs) for analysis | Experimental | ≤ 1–2 F1 gain for high cost. Weight licence **for legal review** (BEAT_TRACKING_RESEARCH F6) |
| Section boundaries (All-In-One-style labels) to explain tempo changes | Experimental | Dependency weight, weights licence UNVERIFIED |
| 3-seed activation ensemble | Experimental | 3× cost, benefit UNVERIFIED (BEAT_TRACKING_RESEARCH OQ3) |
| Beat This **small** model | MVP (fallback) | Used only if the Phase 3 perf gate fails (DECISION_SUMMARY §1) |
| Pure-DSP tracker | MVP | Fallback + cross-check + refinement (ADR-003) |

## 3. Stretch modes (brief §9)

Original names. No FL/Ableton mode names (LICENSING_RESEARCH §8).

| Brief concept | BeatFix name | Class | Reason |
|---|---|---|---|
| Smooth / Elastic | **Smooth** (Signalsmith Stretch 1.4.0, linked stereo) | MVP (default) | MIT, native variable ratio, IPD preserved (ADR-006) |
| Resample (varispeed) | **Resample** | MVP | Constant-ratio material only. Pitch shift shown in cents (≈ 17.2 cents per 1 %) |
| Transient Preserve | **Transient** (HPSS hybrid: harmonic→Smooth, percussive→short OLA, same W⁻¹) | Phase 1.5 | Gated on an A/B listening test on drum-heavy corpus items (TIME_STRETCH_RESEARCH rec. 4) |
| Drum / Slice | **Slice** | Phase 3 | Fraunhofer US 9,230,558 B2 (expected expiry 2029-02-17) covers transient cut/stretch/reinsert. **For legal review** before any work |
| Granular | **Texture** | Future | Not needed for ±5 % corrections. Useful only at extreme ratios |
| Auto, per-bin soft hybrid inside one engine (no seams) | Auto | Phase 2 (Experimental until tested) | The principled form of Auto (TIME_STRETCH_RESEARCH TD-7a) |
| Auto, region-level Smooth/Transient switching | Auto (regional) | Experimental | Only if per-bin Auto proves insufficient. ≥ 4-bar hysteresis. Never auto-selects Resample or Slice (TD-7b). Design in [HYBRID_AUTO_ENGINE.md](../04_algorithms/HYBRID_AUTO_ENGINE.md) |
| Passthrough for unchanged regions (|r−1| < 1e-4 for ≥ 2 bars) | internal | Experimental | Seam audibility unverified. Behind a setting only |
| Rubber Band R3 engine | — | dev/test only, never distributed. Premium engine = Phase 3 | GPL. Commercial licence (£590+) only if listening tests justify it (ADR-006) |
| Bungee | — | Fallback engine (not shipped by default) | MPL-2.0. Stereo linking UNVERIFIED |
| Preview quality Fast / High | — | MVP | Export always renders at the highest quality |

## 4. Import / export (brief §15)

| Item | Class | Spec |
|---|---|---|
| Import WAV (PCM/float/EXTENSIBLE), RF64/BW64 | MVP | JUCE readers |
| Import AIFF / AIFF-C | MVP | JUCE readers |
| Import FLAC | MVP | JUCE readers. Verify STREAMINFO MD5 if present |
| Import MP3 | MVP | minimp3_ex, LAME/Xing gapless trim. 0-sample offset gate (ADR-009) |
| Import Ogg Vorbis | MVP | JUCE readers |
| Import AAC/M4A | Phase 3 | Patent pool (Via LA) and MF licence coverage **for legal review** (LICENSING_RESEARCH §7) |
| Import Opus | Phase 2 | libopus/opusfile BSD-3. Pre-skip trim. Deferred only to keep MVP small (DECISION_SUMMARY §3.7) |
| Export WAV 16 / 24 / 32f | MVP | Default **24-bit, TPDF on**. Float never dithered. Auto-RF64 > 4 GiB |
| Export FLAC 16 / 24 | MVP | Default 24. Writes MD5 |
| Export MP3 | MVP | LAME DLL (LGPL), LAME tag written. Default **CBR 320**. V0/V2 offered. Gate −1 dBTP |
| Export AIFF | Phase 2 | Cheap. Low demand |
| Export Opus | Phase 2 | 48 kHz only (forces SRC from 44.1 k) |
| Export Ogg Vorbis | Future | Low demand |
| Export AAC/M4A | Phase 3 | Same legal gate as import. MF encoder limits: 16-bit input, 96–192 kbps |
| Sample-rate handling | MVP | Native rate. Export SR selectable (44.1/48/88.2/96). One r8brain SRC max |
| Bit depth / float PCM | MVP | As above. f32 buffers, f64 time (ADR-002) |
| MP3 bitrate / VBR | MVP | CBR 320 / 256, VBR V0 / V2 |
| Metadata carry-over (text tags) + BPM tag | MVP | Strip stale tech tags (ReplayGain, iTunSMPB, DJ beatgrid blobs). Keep provenance fields |
| Artwork carry-over | Phase 2 | Tag-library choice open (TagLib LGPL vs own writer) |
| Re-import guard tag (`BEATFIX_RENDER=…`) | MVP | Prevents an extra lossy generation (AUDIO_QUALITY_RESEARCH §4.10) |
| Filename template + Windows sanitisation + collision handling | MVP | Never overwrite the source |
| Clipping behaviour: peak gate (0 dBFS int / −1 dBTP lossy), user picks lower gain (default) / TP limiter / export anyway | MVP | DECISION_SUMMARY §6. Nothing silent |
| Export progress (render → encode → validate) + cancel | MVP | Partial file deleted on cancel |
| Output validation (re-decode: length, offset, hash/MD5, NaN, peak, loudness) | MVP | Before the atomic rename |
| Loudness report (ΔLUFS-I, TP, LRA source vs output) | MVP | libebur128. Report only |
| "Match source loudness" gain | Phase 2 | Opt-in in research. Not in DECISION_SUMMARY MVP list |
| Export report JSON sidecar | Phase 2 | MVP shows the report in the result card |
| Click-track / tempo-map / MIDI export (D10) | Phase 2 | DECISION_SUMMARY §4 excludes MIDI/tempo-map from MVP |
| Ableton-style warp-marker interchange | Phase 3 | Format/trademark review. WARP_MAP.md "Interchange" |

## 5. Optional enhancement (brief §14)

Placement if ever built: `Beat Sync → [Enhancement, opt-in, per-module, A/B, bypass = bit-identical] → Export`. It must never change W or timing (AUDIO_QUALITY_RESEARCH §4.13).

| Module | Class | Reason |
|---|---|---|
| Gentle denoise | Experimental | Available models are speech-trained (RNNoise, DeepFilterNet). Low fit for music |
| De-click | Phase 3 | Classical AR interpolation from papers. No dependency |
| De-clip | Phase 3 | Same (A-SPADE family). Label "estimated" |
| Spectral repair | Future | Large UI and DSP surface |
| Tonal cleanup | Future | Undefined requirement |
| High-frequency reconstruction (AudioSR, Apollo) | Experimental | Generative = synthetic content. Weights licence UNVERIFIED / CC BY-SA. **For legal review** |
| Stereo optimisation / widening | Never | Alters the mix. Contradicts fidelity-first |
| Loudness tools | Partly MVP | Measurement, peak gate and opt-in TP limiter are MVP. Normalisation to platform targets: Never (a delivery job) |

## 6. Extensibility targets (brief §40)

| Feature | Class | Seam kept open in MVP architecture |
|---|---|---|
| Multiple tracks | Phase 2 (as linked stems only) | Warp map is independent of the audio it's applied to |
| Stem processing (linked mix + stems, one map) (D8) | Phase 2 | Same map, sample-length equality check (USER_PERSONAS J4) |
| Vocal-specific modes | Future | `ITimeStretcher` interface (ADR-006) |
| Tempo ramps | Future | W is a general monotone map |
| Groove templates | Future | Natural keeps source groove. No template model |
| Manual warp markers | Phase 2 | `manual anchors` field reserved in WARP_MAP.md |
| Quantization strength | **MVP** | = Natural↔Strict |
| Swing | Future | — |
| MIDI beat export | Phase 2 | Beat list + tempo curve already persisted |
| Ableton-style warp markers | Phase 3 | Interchange section of WARP_MAP.md |
| Plugin integration / hosting | Future | Engine core has a C API, no JUCE types (ADR-001) |
| Batch processing (D10, P3) | Phase 2 | Analysis/render are headless. Confidence gate decides auto vs "needs review" (USER_PERSONAS J6) |
| Command-line mode | Phase 2 | Second JUCE target over the same core |
| Spectral repair | Future | Enhancement stage slot |
| AI-assisted correction | Experimental | `IBeatAnalyzer` is swappable. Watch-list models (masked-diffusion, AlignBeat, BeatFM) have no verified licence (BEAT_TRACKING_RESEARCH F14) |

## 7. Platform, distribution, product

| Item | Class |
|---|---|
| Windows 10/11 x64, Inno Setup per-user installer + portable zip, code-signed | MVP |
| Microsoft Store MSIX | Phase 2 |
| macOS / Linux | Future (engine is portable C++20. JUCE shell supports both) |
| Telemetry, cloud, accounts, online updates inside the app | Never (MVP makes no network calls) |
| Project file `.beatfix` (inputs only), autosave, lock file, undo/redo | MVP |
| Split L/R waveform lanes. "Absolute time" lane alignment toggle | Phase 2 |
| Loudness-matched A/B, blind A/B | Phase 2 |
| Session/QA report for P4 ("what did you do to my audio") | Phase 2 (MVP has the export result card + Issues list) |
| Localisation | Future |

## 8. Competitor-derived opportunities (D1–D12)

| ID | Opportunity | Class |
|---|---|---|
| D1 | One-click fix with smart target | MVP |
| D2 | Confidence-aware automation | MVP |
| D3 | Ranked tempo hypotheses | MVP |
| D4 | Natural vs Strict | MVP |
| D5 | Correction heatmap | MVP |
| D6 | Analysis ↔ correction separation | MVP |
| D7 | Non-destructive render-from-source | MVP |
| D8 | Linked stems | Phase 2 |
| D9 | Section-aware exceptions | MVP (flag + keep/flatten). Phase 2 (user regions, own tempo) |
| D10 | DAW/DJ handoff | MVP (BPM tag, filename, DAW-ready start). Phase 2 (click/MIDI/tempo map) |
| D11 | Offline, private, Windows-first | MVP |
| D12 | Quality transparency (warn outside safe ratio band) | MVP (warnings). Per-region mode suggestion: Experimental (needs Auto) |

## Open scope questions
1. Should Opus import move into MVP? It's cheap and BSD, but DECISION_SUMMARY §3.7 excludes it. Decide after the Suno format probe (AUDIO_QUALITY_RESEARCH OQ1).
2. Is the "match source loudness" option worth MVP UI space? Decide after measuring the shipping engine's ΔLUFS on real material (research: −0.2…−0.5 LU, below a ~1 LU JND).
3. What share of P2 users need stems in v1 (COMPETITOR_RESEARCH OQ5)? This sets whether D8 moves ahead of batch.
