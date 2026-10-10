# Web Research Orchestration: Beat-sync / tempo-correction desktop audio editor
Timestamp: 20261007-235940
Started: 2026-10-07 23:59:40
Thoroughness: thorough
Wave Policy: continue while critical gaps

## Research Question
Planning inputs for a desktop app that detects beats/tempo drift in audio and corrects tempo (beat-sync / tempo-correction / warp).

## Facets
1. Time-stretch DSP + libraries & licensing (Rubber Band, SoundTouch, Signalsmith Stretch, Bungee, elastique)
2. Beat tracking / tempo estimation (madmom, BeatNet, Beat This!, essentia, aubio, librosa, BTrack) + licensing
3. Competitors (Ableton warp, Logic Smart Tempo, Melodyne, Serato, Mixed In Key, Studio One, Reaper, Audacity, tempo-correction tools)
4. Audio quality eval / QA (artifacts, metrics, test datasets)
5. UI patterns + desktop architecture (JUCE, Tauri/Rust, Python; offline render pipeline)

## Collected Findings
(filled below)

### Wave 1 findings (inline research)
- Rubber Band v4.0.0 (2024-10-25): GPL or paid commercial licence; R3 "finer" engine higher quality (mixes, vocals, bass), more CPU than R2. breakfastquay.com/rubberband
- Signalsmith Stretch: MIT, C++11, realtime+offline, formant ctrl, seek(); best 0.75x-1.5x; bindings Py/JS/Rust; used by Qt Multimedia FFmpeg backend; OpenMPT eval says comparable to Rubber Band. github signalsmith-audio
- Bungee: MPL-2.0 open source (adaptive phase vocoder, any ratio incl reverse); Bungee Pro proprietary, same API. Compare page tests 12 impls (Audacity/StaffPad, Bungee, Bungee Pro, elastique 3.4.5, PaulStretch, RubberBand R3, Serato, Signalsmith, Sonic, SoundTouch, Superpowered, browser) w/ 16 samples, 0.25-1.5x.
- SoundTouch: LGPL-2.1, WSOLA time-domain, ~100ms latency, lower quality on polyphonic.
- elastique (zplane): commercial SDK, large upfront + per-unit fee; used in Ableton (Complex), FL Studio, Cubase, etc.
- Audacity 3.4: Musical view (beats & measures), clip stretch via StaffPad-derived lib-time-and-pitch (license unverified).
- Beat This! (ISMIR 2024, CPJKU): MIT code+weights, pip beat-this, PyTorch, 78MB final / 8MB small, beats+downbeats, no DBN needed, 22kHz mono spectrogram.
- madmom: BSD code but models CC BY-NC-SA 4.0; commercial -> contact Widmer (JKU). "DBN patented" claim = low-confidence single source.
- BeatNet: CC BY 4.0, online CRNN + particle filter, beats/downbeats/meter.
- Essentia: AGPLv3 or commercial (MTG-UPF); RhythmExtractor2013; TempoCNN models AGPL.
- Competitors: Logic Smart Tempo (adapt vs keep tempo, constant vs variable, multitrack downmix analysis); Melodyne 5 Tempo Editor (assign/edit, make tempo constant, transfer tempo between recordings); Ableton warp (auto-warp markers near not on transients, long-sample issues); Pro Tools Elastic Audio/Beat Detective; Studio One Audio Bend; REAPER dynamic split -> stretch markers + SWS tempo tools; Rekordbox dynamic beatgrid; Serato beatgrid 128-marker limit.
- QA: Roberts & Paliwal TSM dataset 42,529 ratings; existing objective measures poor; their NN measure r=0.865. Beat eval: mir_eval F1/CMLt/AMLt, 70ms; datasets Ballroom, Hainsworth, SMC, GTZAN, Harmonix (912 songs).
- Artifacts: transient smearing, phasiness; fixes: phase locking (Puckette), transient phase reset (Bonada), multi-resolution.

### Wave 2 findings (gap-fill)
- Rubber Band commercial: Standard GBP590 (prominent attribution), Non-attribution small (<10 staff) GBP1,490, Non-attribution GBP9,320; perpetual, no royalty, no updates/support included.
- RB API: setKeyFrameMap (offline only, source->target sample map, after setTimeRatio, before process); offline = study() then process(); R3 = OptionEngineFiner; Transients/Detector/Phase options R2-only; WindowShort for R3 speed.
- Beat This ports: ONNX exports on HF (musetric/beat-this-onnx WebGPU; ashudesai), C++ port beat_this_cpp (mosynthkey), Rust crate beat-this 1.1.0 (2026-10-02, MIT, rten pure-Rust or ort; 4.5-min MP3 ~4.6s on M4; ~94MB/hr streaming; port authored by Claude -> young, verify).
- SMC Blind Spot (arXiv 2605.12287): SOTA DNN trackers fail on expressive music: octave, continuity, total failure; confident-but-wrong activations; madmom DBN min 55 BPM forces double tempo on 21% SMC tracks -> recommend multi-hypothesis tempo.
- aubio GPL-3.0; librosa ISC (DP beat tracker, older/weaker).
- JUCE 8: AGPLv3 or commercial; Starter free <=$20k revenue (no splash), Indie <=$300k $40/mo or $800 perpetual, Pro $175/mo or $3,500 perpetual (forum/HISE sources; official page fetch failed).
- Rust stack: cpal (I/O), symphonia (decode), Tauri; Signalsmith Rust crates ssstretch/signalsmith-stretch; no mature Rubber Band Rust crate found (Py: pylibrb).
- Bungee Pro: pricing on request, native SDK all desktop+mobile + Web SDK.
- Competitor UX: Logic Smart Tempo Editor has Hint mode vs Beat Marker mode, half/double shortcuts, apply region tempo to project; Melodyne Assign vs Edit mode (Editor/Studio only, Studio $699); manual correction nearly always needed (SOS). Logic $199.99 or Creator Studio $12.99/mo; Live Suite $749.
- Multitrack: per-track stretching breaks inter-mic phase (drums); need group/linked stretch with identical time map; elastique Pro claims inter-channel phase coherence; Logic/Melodyne analyse downmix.

## Cross-Reference
- Agreement: licensing is the binding constraint: commercial-safe stack = Beat This (MIT) + Signalsmith (MIT) or Bungee (MPL) or Rubber Band (paid GBP590+). madmom models, Essentia, aubio, BeatNet(CC BY ok) vary.
- Agreement (Logic, Melodyne, SOS, SMC paper): auto detection errs on half/double tempo and downbeats; UI must make correction fast.
- Conflict: Bungee compare page labels Signalsmith "Commercial"; repo says MIT -> trust repo (primary source).
- Low confidence: "madmom DBN patented" (single HF model card).

## Gap Evaluation
- Critical: none. Minor: Audacity StaffPad lib license unverified; Bungee Pro price; official JUCE page; no head-to-head numeric TSM quality scores for libraries (only listening pages).
- Decision: SATISFICED (thorough) - all facets covered medium+.
