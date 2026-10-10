# Library Evaluation — Frameworks, DSP, Analysis, Codecs, Resampling, Loudness

Agent C (Libraries / Technology Stack). Research date 2026-10-08. Companion: `LICENSING_RESEARCH.md` (licence obligations, traps, trademarks, patents). Extends `_baseline_research_log.md`; does not repeat its competitor/QA content.

Status tags: **[V]** verified this session against a primary source (repo/licence file/official doc/Context7) · **[S]** secondary source (forum, press, search summary) · **UNVERIFIED** = not confirmed, do not rely on it without checking.

---

## Objective

Choose, for each BeatFix subsystem (app shell, audio I/O, decode, analysis/ML, time-stretch, resampling, loudness, encode), a primary library plus fallback, with current versions, maintenance status, licence, commercial-redistribution implications, Windows-first build complexity, offline/real-time fit, API quality and known limits. Produce (a) a decision matrix, (b) a primary stack, (c) a fully permissive "engineering-safe shortlist" (closed-source, no paid licences), (d) an optional paid-upgrade path.

## Why It Matters

- Licensing is the binding constraint (baseline cross-reference). One GPL/AGPL/NC component in a shipped binary forces open-sourcing the app or removing the component.
- The warp engine must be swappable (brief §17: "replace beat detector, time-stretch engine, resampler, codec layer without rewriting"). Library APIs differ structurally (key-frame map vs. block ratio vs. per-grain source position), so the choice shapes the `IStretchEngine` interface.
- Windows-first: MSVC build, DLL hygiene, installer size, WebView2/ONNX runtime redistribution.

---

## Key Findings

1. **Versions are newer than the baseline assumed.** JUCE is at **9.0.3** (2026-09-28), not 8; JUCE 9 kept JUCE 8 tiers/prices and EULA unchanged [V repo LICENSE.md + S forum "JUCE 9: No pricing or EULA changes"]. Tauri stable is **2.12.1** (2026-09-30); Tauri **3.0.0-alpha.4** exists (2026-10-01) — do not start on v3 alpha [V GitHub releases]. ONNX Runtime **1.30.0** (2026-09-10) [V]. Qt **6.12** LTS (2026-09-30) [S endoflife.date]. FFmpeg **8.1.x** "Hoare" (8.1 released 2026-03-16) [S].
2. **Time-stretch:** Signalsmith Stretch (MIT, tag 1.4.0, active Sept 2026) and Bungee (MPL-2.0, v2.4.30, 2026-08-29) are the only high-quality permissive engines. Rubber Band remains GPL-2.0+/commercial at **£590 / £1,490 / £9,320** perpetual [V breakfastquay.com/technology/license.html]. Latest Rubber Band tag is still **v4.0.0** (last push 2025-03) — maintained but slow-moving [V].
3. **Audacity's StaffPad TSM is a GPL trap**: files in `au3/libraries/au3-time-and-pitch/StaffPad/` carry no per-file licence header, and Audacity's LICENSE.txt says files without another licence default to **GPLv2-or-later** (repo overall GPLv3) [V]. Not usable in closed source.
4. **Beat analysis:** Beat This! code **and weights are MIT** (v1.1.0, 2026-04-14) with an explicit caveat that some training data is copyrighted / limited-CC ("up to the user to assess") [V README]. ONNX path exists: `beat_this_cpp` (MIT; ONNX Runtime + PocketFFT + r8brain + miniaudio; ships `onnx/beat_this.onnx`; Windows + macOS) [V README] and Rust crate `beat-this` 1.1.0 (MIT, 2026-10-02, rten or ort) [V crates.io].
5. **madmom**: code BSD, **all model/data files CC BY-NC-SA 4.0** [V LICENSE]. **BeatNet** (CC BY 4.0) **depends on madmom ≥0.16.1** and librosa [V setup.py] — Python-only and drags madmom in. **Essentia AGPL-3.0**, **aubio GPL-3.0** [V GitHub licence API].
6. **ONNX Runtime on Windows:** DirectML EP is in **sustained engineering**; new GPU work moved to Windows ML [S onnxruntime.ai / learn.microsoft.com]. For a ~5 min song the model is small (8–78 MB); CPU EP is sufficient and avoids GPU driver variance.
7. **Codecs:** libsndfile **1.1.0+** reads/writes MP3 (mpg123 decode, LAME encode) [V CHANGELOG]; latest release still **1.2.2 (2023-08-13)** — slow release cadence [V]. `SFC_SET_COMPRESSION_LEVEL` is only implemented for FLAC and Ogg/Vorbis per docs [V command.html]; `SFC_SET_BITRATE_MODE` exists (format coverage not documented → treat MP3 VBR/CBR control as **UNVERIFIED**, test it).
8. **AAC is still patent-licensed** (Via LA pool; per-unit encoder/decoder royalty from $0.98/unit, $15k initial fee or $1k small entity) [V via-la.com]; patent expiry 2028 (baseline AAC) – 2031 (extensions) [S]. MP3 is patent-free (US last patent expired 2017-04-16; licensing programme ended 2017-04-23) [S multiple]. → **Do not ship AAC encode in v1**; MP3 encode via LAME is fine.
9. **Windows Media Foundation AAC encoder** exists (`CLSID_AACMFTEncoder`, Mfaacenc.dll): AAC-LC only, **16-bit PCM input only**, **44.1/48 kHz**, 1/2/6 ch, bitrates **96/128/160/192 kbps** only (12000–24000 bytes/s) [V learn.microsoft.com]. Whether an app invoking the OS MFT inherits Microsoft's patent coverage is **for legal review** (Via LA states no OS-vendor pass-through for Android; Windows not addressed) [V via-la.com].
10. **Resamplers:** r8brain-free-src MIT (tag version-6.5, active Sept 2026); libsamplerate BSD-2 (0.2.2, 2021; maintained but quiet); soxr LGPL-2.1+ (0.1.3, last push 2023 — effectively dormant); rubato (Rust) **MIT OR Apache-2.0** 5.0.1 (2026-10-01) [V].
11. **Loudness:** libebur128 MIT v1.2.6 (2021; last push 2023 — stable/dormant, small, well-known); Rust `ebur128` crate MIT 0.1.10 [V].

---

## Technical Details

### A. Application frameworks

| Framework | Version (2026-10) | Licence | Closed-source cost | Audio I/O | Waveform UI | Build on Windows | Notes |
|---|---|---|---|---|---|---|---|
| **JUCE 9** | 9.0.3 (2026-09-28) [V] | AGPLv3 **or** JUCE 9 EULA [V LICENSE.md] | Starter free ≤ $20k annual revenue/funding (no splash since JUCE 8); Indie ≤ $300k: $40/mo or $800 perpetual per user; Pro: $175/mo (12-mo min) or $3,500 perpetual [S forum, consistent across sources; juce.com fetch failed] | WASAPI/ASIO/DirectSound via `AudioDeviceManager`; `AudioFormatManager`, FLAC/Ogg/WAV/AIFF built-in, `MP3AudioFormat` (decode), `LAMEEncoderAudioFormat` (wraps external LAME exe), `WindowsMediaAudioFormat` [V Context7 JUCE docs] | Native C++ graphics, mature `AudioThumbnail` | CMake, MSVC, single language | Audio-native; interpolators incl. `WindowedSincInterpolator`, `LagrangeInterpolator` [V Context7]. AGPL = must open-source app. Third-party deps listed in `JUCE.spdx.json` [V]. |
| **Qt 6** | 6.12 LTS (2026-09-30) [S] | LGPLv3/GPL/commercial | LGPL: free if dynamic-linked + relinkable + Qt source + install info (anti-tivoization) [V qt.io]; some modules GPL-only. Qt for Small Business ADE ≈ €546/yr (revenue < €1M, ≤3 seats) [S] | Qt Multimedia (FFmpeg backend; uses Signalsmith Stretch for playback-rate per baseline) | QPainter/QML; good | Large SDK, moc, windeployqt; heavier installer | LGPLv3 relink obligation is real paperwork; LTS patches commercial-only since 6.8 [S]. |
| **Rust + Tauri 2** | tauri 2.12.1 [V]; cpal 0.18.2 (Apache-2.0); symphonia 0.6.1 (MPL-2.0); rubato 5.0.1 (MIT/Apache) [V crates.io] | MIT/Apache-2.0 (Tauri) | None | cpal (WASAPI/ASIO feature) | Web canvas/WebGL; stream peaks via `tauri::ipc::Channel` [V Context7] | Rust + TS front-end + (C++ DSP via FFI) = 3 toolchains; WebView2 evergreen on Win11 | Best permissive polished-UI option. Tauri 3 alpha → expect a migration within product life. DSP ecosystem is C++; Rust bindings for Signalsmith (`signalsmith-stretch` 0.1.3), Bungee (`bungee-rs` 0.1.1) are young [V]. |
| **Dear ImGui** | v1.92.9 (2026-07-31) [V] | MIT | None | none (pair with miniaudio) | Immediate-mode; ImPlot for waveforms | Trivial (sources in tree), needs D3D11/12 or GL backend | Fast to build tool UIs; weak accessibility, i18n, native dialogs (need nfd/OS APIs). Good for an internal "lab" build. |
| **wxWidgets** | 3.3.3 (2026-07-07) [V] | wxWindows Library Licence (LGPL + static-link exception) — GitHub API shows no SPDX id | None (static linking allowed) | none | Native widgets; custom draw | CMake, MSVC OK | Dated look; small audio community. |
| **.NET/WPF + NAudio** | NAudio 3.1.0 (2026-09-07) MIT [V] | MIT | None | WASAPI/ASIO via NAudio; MF readers | WPF/WinUI drawing OK | Easy on Windows; C++ DSP via P/Invoke | Windows-only lock-in; native interop for every DSP lib; GC pauses irrelevant offline but add complexity to preview. |

### B. Time-stretch / TSM engines

| Engine | Version / activity | Licence | Algorithm | Warp-map fit (BeatFix needs a monotonic source→target map) | Real-time | Offline | Limits |
|---|---|---|---|---|---|---|---|
| **Signalsmith Stretch** | tag 1.4.0; last commit 2026-09-25 [V] | MIT [V] | Phase vocoder + multi-prediction, C++11 header-only | `process(in, inputSamples, out, outputSamples)` — no max block, caller controls average in/out ratio per call [V Context7]; so a piecewise-linear warp map = per-segment block sizing. `exact()` for offline whole-buffer, `outputSeek()`, `flush()`, `inputLatency()/outputLatency()` [V Context7]. | Yes (`splitComputation`) | Yes | Best 0.75×–1.5× [V README] (BeatFix corrections are typically within ±5–10%, well inside). Formant correction "not as sharp as monophonic" [V]. Debug builds ~10× slower [V]. Needs `-O2`/`/O2` + fast-math check. |
| **Bungee** | v2.4.30 (2026-08-29), CI prebuilt for Win/macOS/Linux/Android/iOS [V] | MPL-2.0 [V] | Adaptive phase vocoder, granular (~100 grains/s) [V] | Granular API: each `Request` carries an input `position` (+ `speed`, `pitch`) per grain — maps one-to-one to a warp map (position = W⁻¹(t_out)) [Med; README field semantics recalled from prior fetch, re-verify]. Also `Stream` FIFO wrapper [V]. | Yes | Yes | Deps Eigen + PFFFT (+ cxxopts for CLI) via submodules [V]. MPL file-level copyleft — modifications to Bungee files must be published. |
| **Bungee Pro** | commercial | proprietary | "novel algorithms", SIMD, same API [V repo] | same | yes | yes | Price on request [S baseline]. |
| **Rubber Band v4 (R3 "finer")** | tag v4.0.0; last push 2025-03-03 [V] | GPL-2.0+ or commercial £590 (attribution) / £1,490 (no attribution, <10 staff) / £9,320 (no attribution) GBP ex-VAT, perpetual, no royalty, no formal support [V] | Phase vocoder; R3 multi-resolution | **Native**: `setKeyFrameMap(std::map<size_t,size_t>)` source→output frames, offline only, two-pass `study()` then `process()`/`retrieve()` [V Context7] — the exact BeatFix warp primitive. | Yes (`OptionProcessRealTime`) | Yes (`OptionProcessOffline`) | Transient/Detector/Phase options are R2-only [baseline]; R3 heavier CPU. Rust: only `rubberband-sys` 0.1.4 (GPL) [V]. |
| **SoundTouch** | — | LGPL-2.1 | WSOLA time-domain | Rate changes per block | Yes | Yes | Lower polyphonic quality, ~100 ms latency [baseline]. Fallback only. |
| **zplane élastique** | — | Commercial; royalty per sold unit, depends on products/platforms/market; pricing negotiated [S licensing.zplane.de] | Proprietary | Has time-map APIs **UNVERIFIED** | Yes | Yes | NDA/SDK cost; reference-quality (Ableton/FL/Cubase). Paid path only. |
| **Audacity/StaffPad TimeAndPitch** | in Audacity `au3/libraries/au3-time-and-pitch` [V] | **GPLv2+ by default** (no file header; Audacity licence rule) [V] | Phase vocoder (PFFFT) | `setTimeStretchAndPitchFactor` per block | Yes | Yes | **Excluded (GPL)**. Tested range 0.5–2.0 per its readme [V]. |

### C. Beat/tempo analysis and inference runtimes

| Candidate | Version / activity | Licence | Runtime | Notes |
|---|---|---|---|---|
| **Beat This!** | v1.1.0 (2026-04-14) [V] | MIT code **and** weights [V]; training-data caveat [V] | PyTorch ref; ONNX export | `final0-2` ≈78 MB, `small0-2` ≈8.1 MB [V]. Beats + downbeats; no DBN required. |
| **beat_this_cpp** | last push 2026-07-01, 15★, no releases [V] | MIT [V] | ONNX Runtime | Pipeline: miniaudio decode → mono → r8brain to 22050 Hz → 128-band mel (n_fft 1024, hop 441) via PocketFFT → chunked inference (1500 frames, 6-frame border) → peak-pick or "madmom-compatible" HMM/Viterbi (`--dbn`) [V README]. **Young, single maintainer** — treat as reference, vendor and test. The DBN is a reimplementation; the "madmom DBN patented" claim is still low-confidence (baseline) → legal review before shipping `--dbn`. |
| **beat-this (Rust)** | 1.1.0 (2026-10-02) [V] | MIT [V] | rten (pure Rust) or ort | Very young (baseline: ported with AI assistance); verify outputs against Python reference. |
| madmom | v0.16.1 (2018); repo active 2026-03 [V] | BSD code + **CC BY-NC-SA 4.0 models** [V] | Python/Cython | **Excluded** (NC models). |
| BeatNet | no releases; repo 2026-04 [V] | CC BY 4.0 [V] | Python; **requires madmom, librosa** [V] | Excluded for shipping (Python + madmom). Reference only. |
| Essentia | AGPL-3.0; last tag v2.1_beta5 [V] | AGPL / commercial (UPF) | C++/Python | **Excluded** unless commercial licence bought. |
| aubio | 0.4.9 (2019) [V] | GPL-3.0 [V] | C | **Excluded**. |
| librosa | 1.0.0 (2026-08-11) [V] | ISC [V] | Python only | Prototyping / ground-truth tooling only; algorithms (onset envelope, tempogram, DP beat tracker) are re-implementable from papers. |
| **ONNX Runtime** | 1.30.0 (2026-09-10) [V] | MIT [V] | C/C++ API; `Ort::Env`, `Ort::SessionOptions` (`SetIntraOpNumThreads`, `SetInterOpNumThreads`), `Ort::Session` [V Context7] | CPU EP default. DirectML EP sustained-engineering; Windows ML is the forward path [S]. Ship `onnxruntime.dll` side-by-side (size **UNVERIFIED**, typically tens of MB). |
| rten | v0.27.0 (2026-10-05) [V] | MIT OR Apache-2.0 [V crates.io] | Pure Rust | Good for Rust shell; op coverage must be checked per model. |
| tract (Sonos) | v0.23.8 (2026-09-21) [V] | MIT OR Apache-2.0 [V LICENSE] | Pure Rust | Alternative to rten. |

### D. Decode / encode / file I/O

| Library | Version | Licence | Role | Notes |
|---|---|---|---|---|
| **dr_libs** (dr_wav, dr_flac, dr_mp3) | dr_wav 0.14.5 tag; dr_mp3 0.7.4 header [V] | Public domain **or MIT-0** [V header] | Decode WAV/FLAC/MP3; write WAV | Single-header, zero deps; dr_mp3 based on minimp3 [V]. |
| minimp3 | repo active 2026-07 [V] | CC0-1.0 [V] | MP3 decode | Use via dr_mp3. |
| **miniaudio** | 0.11.25 (2026-03-03), active [V] | Public domain (Unlicense) **or MIT-0** [V LICENSE] | Playback (WASAPI), decode (`ma_decoder`), resample, WAV encode | `ma_decoder_init_file`, `ma_device` callback [V Context7]. Permissive audio I/O for non-JUCE shells. |
| **libFLAC** | 1.5.0 (2025-02-11) [V] | BSD-3 (Xiph) for libFLAC [V COPYING.Xiph]; repo also carries GPL/GFDL for tools/docs | FLAC encode/decode | Ship libFLAC only (not `flac` CLI). |
| **libsndfile** | 1.2.2 (2023-08-13) [V] | LGPL-2.1 [V] | Unified read/write WAV/AIFF/FLAC/Ogg/Opus/MP3 | MP3 since 1.1.0 via mpg123 + LAME [V]. `sf_open`, `sf_readf_float`, `sf_writef_float`, `sf_command` [V]. Must be dynamic-linked (DLL) for closed source. |
| **LAME** | 3.100 (2017) — **UNVERIFIED** this session, dormant | LGPL-2.0+ (well-established; confirm in source) | MP3 encode | Only realistic MP3 encoder; dormant but stable. DLL-link. |
| libmpg123 | version **UNVERIFIED** this session | LGPL-2.1 | MP3 decode | Only needed if using libsndfile MP3; else dr_mp3. |
| libopus | v1.6.1 tag [V] | BSD-3 [V] | Opus | Optional export (non-goal for v1). |
| libvorbis | v1.3.7 (2020) [V] | BSD-3 [V] | Vorbis | Optional. |
| **FFmpeg** | 8.1.x [S] | LGPL-2.1+ default; GPL if `--enable-gpl`; non-redistributable with `--enable-nonfree` [V ffmpeg.org/legal] | Universal decode (incl. M4A/AAC, ALAC) | Heavy (~tens of MB DLLs); LGPL compliance checklist in `LICENSING_RESEARCH.md`. Use only as optional "import anything" fallback. |
| fdk-aac | v2.0.3 tag [V] | Fraunhofer FDK licence (not OSI/GPL-compatible; patent licence not granted) [V NOTICE] | AAC enc | **Excluded.** |
| Windows Media Foundation | OS | OS component | AAC-LC encode (constrained, see Key Finding 9); AAC/M4A/ALAC decode | Zero redistribution; patent coverage → legal review. |

### E. Resampling

| Library | Version | Licence | Quality / use | Notes |
|---|---|---|---|---|
| **r8brain-free-src** | tag version-6.5; active 2026-09 [V] | MIT [V] | Very high (beat_this_cpp claims >144 dB SNR config) [V README] | C++ header-ish; offline & streaming; used by beat_this_cpp. |
| **libsamplerate** | 0.2.2 (2021); repo active 2026-08 [V] | BSD-2 [V] | `SRC_SINC_BEST_QUALITY`…`SRC_LINEAR`; `src_new/src_process/src_simple/src_callback_new` [V Context7] | Supports time-varying ratio (useful for varispeed preview). |
| libsoxr | 0.1.3; last push 2023-11 [V] | LGPL-2.1+ [V]; note optional pffft licence [V LICENCE] | Excellent | Dormant + LGPL → not needed given r8brain/libsamplerate. |
| JUCE interpolators | JUCE 9 | JUCE licence | `WindowedSincInterpolator`, `LagrangeInterpolator`, `CatmullRom`, `Linear` [V Context7] | Adequate for preview; use r8brain for export SRC. |
| **rubato** | 5.0.1 (2026-10-01) [V] | MIT OR Apache-2.0 [V Cargo.toml] | Sinc (async/sync), FFT-based | Rust-shell choice. |

### F. Loudness / true-peak

| Library | Version | Licence | Notes |
|---|---|---|---|
| **libebur128** | v1.2.6 (2021), last push 2023 [V] | MIT [V] | EBU R128 integrated/short-term/momentary, LRA, sample & true peak. Small, stable; dormant is acceptable for a spec-frozen algorithm. Vendor it. |
| Rust `ebur128` | 0.1.10 (2024-10) [V] | MIT [V] | Port of libebur128. |

---

## Decision Matrix

Legend: Q = output quality, P = performance, API = maturity. Ratings H/M/L are engineering judgement from evidence above.

| Purpose | Candidate | Alternatives | Quality | Perf | API maturity | Licence | Commercial (closed-source) use | Platform | Maintenance | Build complexity | Recommendation | Fallback |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Engine language | C++20 core (framework-agnostic, C ABI) | Rust core | — | H | — | — | — | Win/mac/Linux | — | M | **Adopt**: all top TSM libs are C++ | Rust core w/ FFI |
| App shell / UI | **JUCE 9** (commercial EULA: Starter→Indie) | Tauri 2.12, Qt 6.12 LGPL, Dear ImGui, wxWidgets, WPF+NAudio | H | H | H | AGPLv3 / commercial | Yes **only** under JUCE EULA (Starter free ≤$20k; Indie $800 perp.); AGPL = no | Win/mac/Linux | H (monthly patch releases) | L–M (single C++ toolchain) | **Primary (paid-tier aware)** | **Tauri 2** (permissive) |
| App shell (permissive) | **Tauri 2.12** + C++ engine via FFI | Dear ImGui, Qt LGPL | H (web UI) | M–H | H | MIT/Apache-2.0 | Yes | Win (WebView2)/mac/Linux | H | M–H (Rust+TS+C++) | **Permissive-shortlist primary** | Dear ImGui |
| Audio device I/O | JUCE `AudioDeviceManager` | **miniaudio**, cpal | H | H | H | JUCE / PD-MIT0 / Apache-2.0 | Yes | Win WASAPI | H | L | JUCE if JUCE shell; **miniaudio** otherwise | cpal (Rust) |
| Decode WAV/FLAC/MP3 | **dr_libs** (+ libFLAC) | libsndfile (LGPL DLL), JUCE formats, symphonia (MPL) | H | H | H | PD/MIT-0, BSD | Yes | All | M–H | L (headers) | **Primary** | libsndfile DLL |
| Decode M4A/AAC/ALAC/other | Windows Media Foundation (OS) | FFmpeg LGPL DLL | H | H | M (COM) | OS | Yes; AAC patent coverage → legal review | Windows only | H | M | **v1: MF on Windows, optional** | FFmpeg LGPL build |
| Beat/downbeat model | **Beat This! (ONNX)** | own DSP tracker; Beat This small; (madmom/Essentia/aubio excluded) | H (SOTA 2024) | H on CPU | M (ports young) | MIT (+data caveat) | Yes (legal review of training-data caveat) | All | M (v1.1.0 Apr 2026) | M (export + mel front-end parity) | **Primary** | DSP onset+tempogram tracker (own code) |
| Inference runtime | **ONNX Runtime 1.30 CPU EP** | rten, tract; Windows ML | H | H | H | MIT | Yes | All | H | M (DLL redistribution) | **Primary** | rten (Rust) |
| Time-stretch | **Signalsmith Stretch** | Bungee (MPL), Rubber Band R3 (paid), SoundTouch (LGPL), élastique (paid) | H (≈RB per OpenMPT eval, baseline) | H | M–H (small API, Context7 docs) | MIT | Yes | All | H (Sept 2026) | L (header-only) | **Primary** | **Bungee** (MPL) |
| Time-stretch (premium) | **Rubber Band R3** commercial | élastique, Bungee Pro | H (reference OSS quality) | M | H (`setKeyFrameMap`) | GPL / £590+ | Only with commercial licence | All | M (v4.0.0 2024, slow) | M | **Paid upgrade** | — |
| Resampling (export SRC) | **r8brain-free-src** | libsamplerate (BSD-2), rubato, soxr (LGPL) | H | H | M–H | MIT | Yes | All | H | L | **Primary** | libsamplerate |
| Loudness / true-peak | **libebur128** | Rust ebur128; own impl | H | H | H | MIT | Yes | All | L (stable/dormant) | L | **Primary** | own BS.1770 impl |
| Encode WAV | **dr_wav** (or own RIFF/RF64 writer) | libsndfile, JUCE | H | H | H | PD/MIT-0 | Yes | All | H | L | **Primary** (must support RF64 >4 GB? check dr_wav W64/RF64 — **UNVERIFIED**) | libsndfile |
| Encode FLAC | **libFLAC** | libsndfile | H | H | H | BSD-3 | Yes | All | H | L–M | **Primary** | libsndfile |
| Encode MP3 | **LAME (DLL)** | libsndfile→LAME, MF MP3 encoder (**UNVERIFIED** existence) | H | H | M (old API) | LGPL | Yes, dynamic link + notices | All | L (dormant) | M | **Primary** | libsndfile DLL |
| Encode AAC | — (deferred) | MF AAC-LC encoder; fdk-aac (excluded) | M (MF limited to 16-bit, 96–192 kbps) | H | M | OS / patent pool | Legal review required | Windows | — | M | **Defer from v1** | MF AAC after legal sign-off |
| Universal import | — | FFmpeg 8.1 LGPL DLL | H | H | M (complex API) | LGPL-2.1+ | Yes with full LGPL compliance | All | H | H | **Optional, post-v1** | — |

---

## Alternatives Considered

- **Python backend (librosa/madmom/PyTorch) + any UI**: fastest prototyping, worst distribution (Python runtime, 500 MB+ torch), and madmom NC models. Keep Python only for offline research/validation tooling (ground truth, mir_eval).
- **FFmpeg as the single codec layer**: one dependency for everything, but LGPL compliance overhead, large DLLs, and still no clean AAC encoder (native `aac` encoder is LGPL but patent licence is separate). Rejected for v1 core; optional plugin later.
- **SoundTouch as primary stretch**: licence-friendly (LGPL) but audibly worse on dense AI-generated mixes (WSOLA). Fallback only.
- **Qt instead of JUCE**: comparable capability; LGPLv3 relink/anti-tivoization obligations plus large deployment; less audio-specific. Viable third choice.
- **Pure-Rust stack (Tauri+cpal+symphonia+rubato+rten+signalsmith-stretch-rs)**: fully permissive and single backend language, but Rust bindings for the stretchers are 0.1.x and the reference implementations are C++. Viable as the permissive shortlist; engine still best kept in C++ and called over FFI.

## Evidence / Sources

- JUCE repo licence + releases: https://github.com/juce-framework/JUCE (LICENSE.md, releases 9.0.0–9.0.3) [V]
- JUCE 9 pricing unchanged: https://forum.juce.com/t/juce-9-no-pricing-or-eula-changes/69000 ; JUCE 8 tiers: https://forum.juce.com/t/important-changes-to-the-juce-end-user-licence-agreement-for-juce-8/61265 [S]
- Rubber Band licence/prices: https://breakfastquay.com/technology/license.html [V]; API: Context7 `/breakfastquay/rubberband` [V]
- Signalsmith Stretch: https://github.com/Signalsmith-Audio/signalsmith-stretch ; Context7 `/signalsmith-audio/signalsmith-stretch` [V]
- Bungee: https://github.com/bungee-audio-stretch/bungee [V]
- Audacity licence + StaffPad sources: https://github.com/audacity/audacity (LICENSE.txt; au3/libraries/au3-time-and-pitch/StaffPad) [V]
- Beat This!: https://github.com/CPJKU/beat_this [V]; beat_this_cpp: https://github.com/mosynthkey/beat_this_cpp [V]; Rust crate: https://crates.io/crates/beat-this [V]
- madmom LICENSE: https://github.com/CPJKU/madmom [V]; BeatNet setup.py/LICENSE: https://github.com/mjhydri/BeatNet [V]
- ONNX Runtime: https://github.com/microsoft/onnxruntime (v1.30.0) [V]; DirectML status: https://onnxruntime.ai/docs/execution-providers/DirectML-ExecutionProvider.html [S]
- libsndfile CHANGELOG/API/command docs: https://github.com/libsndfile/libsndfile , https://libsndfile.github.io/libsndfile/api.html , /command.html [V]
- FFmpeg legal: https://ffmpeg.org/legal.html [V]; FFmpeg 8.1 release: https://gigazine.net/news/20260317-ffmpeg-8-1-hoare/ [S]
- MF AAC encoder: https://learn.microsoft.com/en-us/windows/win32/medfound/aac-encoder [V]
- Via LA AAC: https://www.via-la.com/licensing-2/aac/ [V]
- Qt LGPL obligations: https://www.qt.io/licensing/open-source-lgpl-obligations [V]; Qt versions: https://endoflife.date/qt [S]; Qt Small Business: https://www.qt.io/pricing/qt-for-small-business [S]
- zplane licensing: https://licensing.zplane.de/licensing [S]
- GitHub API licence/activity snapshot for all repos listed (queried 2026-10-08) [V]; crates.io API (queried 2026-10-08) [V]

## Recommendation

### Primary stack ("BeatFix Core + JUCE shell")

```
App shell / UI / device I/O : JUCE 9.0.x under JUCE commercial EULA (Starter, free ≤$20k; upgrade to Indie $800 perpetual)
Engine (no JUCE types in API): C++20 static lib "beatfix_core", C ABI facade
  Decode        : dr_wav / dr_flac / dr_mp3  (+ libFLAC); MF reader for M4A on Windows (optional, flag)
  Analysis      : own mel front-end (match Beat This preprocessing exactly) + ONNX Runtime 1.30 CPU + Beat This (final or small)
                  + own tempo/drift/downbeat post-processing (no madmom code/models)
  Warp render   : IStretchEngine { SignalsmithEngine (default), BungeeEngine (A/B), RubberBandEngine (build flag, licence-gated) }
  SRC           : r8brain-free-src
  Loudness/TP   : libebur128
  Encode        : dr_wav (WAV 16/24/32f), libFLAC, LAME DLL (MP3)
```

Confidence: **Medium-High** for the engine libraries; **Medium** for JUCE-vs-Tauri (product/business call — see risks).

Rationale: every quality TSM option is C++; JUCE gives one toolchain, WASAPI/ASIO, waveform thumbnails, and file dialogs. Keeping the engine JUCE-free means the shell can be swapped to Tauri without touching DSP if JUCE licensing or revenue thresholds become a problem.

### Engineering-safe shortlist (fully permissive, closed-source, zero paid licences)

| Layer | Pick | Licence |
|---|---|---|
| UI shell | **Tauri 2.12** (fallback: Dear ImGui 1.92) | MIT/Apache-2.0 (MIT) |
| Audio out | **miniaudio** 0.11.25 (Rust shell: cpal 0.18) | PD/MIT-0 (Apache-2.0) |
| Decode | dr_libs, libFLAC; (Rust: symphonia 0.6 MPL-2.0, unmodified) | PD/MIT-0, BSD-3 |
| ML runtime | ONNX Runtime 1.30 (Rust: ort 2.0-rc / rten 0.27) | MIT (MIT/Apache) |
| Beat model | Beat This! weights | MIT (data caveat → legal review) |
| Stretch | Signalsmith Stretch; Bungee | MIT; MPL-2.0 (unmodified or publish changed files) |
| SRC | r8brain-free-src; libsamplerate | MIT; BSD-2 |
| Loudness | libebur128 | MIT |
| Encode | dr_wav, libFLAC, **LAME as separate DLL** | PD/MIT-0, BSD-3, LGPL (dynamic) |

### Optional paid-upgrade path

1. **JUCE commercial** (if JUCE shell chosen and revenue > $20k): Indie $800 perpetual/user (≤$300k) → Pro $3,500 perpetual/user [S].
2. **Rubber Band commercial**: £590 with prominent credit, or £1,490 non-attribution (<10 employees) [V]. Unlocks R3 quality + native `setKeyFrameMap` warp map. Ship as a build-flagged engine; never in the default open build.
3. **Bungee Pro / zplane élastique**: pricing on request; only if listening tests show Signalsmith/Bungee/RB inadequate on AI-generated material.

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Accidental GPL/AGPL/NC linkage (Rubber Band GPL build, JUCE AGPL, StaffPad, aubio, Essentia, madmom models) | M | Critical | CI licence gate (SPDX scan of vendored code + DLL list); deny-list in repo; see `LICENSING_RESEARCH.md`. |
| JUCE revenue threshold crossed silently | M | M | Track revenue/funding; budget Indie; engine stays JUCE-free. |
| Beat This ONNX front-end mismatch (mel params) → degraded beats | M | H | Golden-file test vs Python reference on N tracks; tolerance on activations. |
| beat_this_cpp / beat-this-rs immaturity | H | M | Vendor + own tests; treat as reference not dependency. |
| Signalsmith quality on dense mixes at larger ratios | L–M | M | Ratios mostly ±10%; A/B against Bungee and RB R3 in QA corpus. |
| libsndfile/LAME dormancy (security fixes slow) | M | L–M | Prefer dr_libs for decode (fuzz untrusted inputs); LAME only for encode of our own PCM. |
| ONNX Runtime GPU path churn (DirectML → Windows ML) | M | L | CPU EP only in v1. |
| Tauri 3 migration | M | M | Pin 2.12.x; thin shell. |

## Assumptions

- BeatFix ships closed-source and commercially (or may later); Windows 10/11 x64 first.
- Typical corrections are small (±10% local ratio), so engines tuned for 0.75–1.5× are sufficient.
- Inputs are mostly MP3/WAV (Suno exports); AAC/M4A is secondary.
- Offline render is the quality path; real-time preview may use a cheaper engine setting.

## Open Questions

1. Revenue/business model (free, paid, subscription)? Determines JUCE tier and whether Rubber Band attribution (£590) is acceptable.
2. Is M4A/AAC import required in v1 (Suno exports MP3/WAV; other AI tools may give M4A)? If yes, MF decode on Windows needs legal sign-off.
3. Is AAC/M4A **export** a requirement at all? (Recommend no for v1.)
4. Beat This `final` (78 MB) vs `small` (8 MB) — installer size vs accuracy; benchmark on Suno corpus.
5. dr_wav RF64/W64 write support for >4 GB 32-bit float renders — **UNVERIFIED**; otherwise write RF64 ourselves.
6. Bungee `Request.position` semantics and Bungee 2.x API stability — re-verify against current README before ADR.
7. Does the Windows MF stack expose an MP3 encoder MFT usable for export (would remove LAME)? **UNVERIFIED**.

## Implementation Implications

- Define `IStretchEngine` around a **warp map**, not a ratio: `render(const WarpMap& W, AudioView src, AudioSink& dst, const StretchParams&)`. Adapters:
  - Signalsmith: iterate warp segments; per block call `process(in, Δsrc, out, Δdst)` where Δsrc/Δdst come from the segment slope; prime with `outputSeek()`; finish with `flush()` after `inputLatency()` silence [V Context7]. Keep cumulative rounding error < 1 sample (carry fractional remainder).
  - Bungee: per grain set `Request.position = W⁻¹(t_out_grain)` (Med confidence on field semantics).
  - Rubber Band: build `std::map<size_t,size_t>` from W, `setTimeRatio(total_out/total_in)` then `setKeyFrameMap`, `study()` full pass, `process()`/`retrieve()` [V Context7].
- All engines must process **stereo channels together** (Signalsmith multi-channel instance; RB `OptionChannelsTogether`) to keep image/phase coherence.
- Render-from-source: decode once to 32-bit float in memory (or memory-mapped temp), never re-decode lossy output.
- ONNX: one `Ort::Env` per process; analysis on worker thread; `SetIntraOpNumThreads(n_cores-1)`.
- Ship third-party notices + LGPL DLLs (LAME, optional libsndfile/FFmpeg) unmodified-or-with-source; see `LICENSING_RESEARCH.md` §Attribution.
- Vendor all permissive libs as source under `third_party/<name>/` with their LICENSE files; generate an SPDX SBOM at build time (JUCE already ships `JUCE.spdx.json`).
