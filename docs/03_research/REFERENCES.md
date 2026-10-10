# References

Status: 2026-10-08. Deduplicated bibliography harvested from every document in `docs/03_research/`, plus the URL-bearing sources cited in `docs/04_algorithms/` and `docs/07_quality/`. Grouped by topic. Confidence and verification status of each claim stay with the citing document. Inclusion here means the source was cited, not that it is authoritative.

**Cited-in codes**: BT = [BEAT_TRACKING_RESEARCH](BEAT_TRACKING_RESEARCH.md) · TE = [TEMPO_ESTIMATION_RESEARCH](TEMPO_ESTIMATION_RESEARCH.md) · TS = [TIME_STRETCH_RESEARCH](TIME_STRETCH_RESEARCH.md) · DSP = [DSP_RESEARCH](DSP_RESEARCH.md) · AQ = [AUDIO_QUALITY_RESEARCH](AUDIO_QUALITY_RESEARCH.md) · LE = [LIBRARY_EVALUATION](LIBRARY_EVALUATION.md) · LIC = [LICENSING_RESEARCH](LICENSING_RESEARCH.md) · CR = [COMPETITOR_RESEARCH](COMPETITOR_RESEARCH.md) · UI = [UI_UX_RESEARCH](UI_UX_RESEARCH.md) · QA = [QA_RESEARCH](QA_RESEARCH.md) · AR = [ARCHITECTURE_RESEARCH](ARCHITECTURE_RESEARCH.md) · BL = [_baseline_research_log](_baseline_research_log.md) · ALG = `04_algorithms/*` · Q7 = `07_quality/*`.

"(not re-fetched)" = cited from established literature without fetching in this pass (Med confidence on exact parameters).

---

## 1. Beat tracking, onset detection, tempo estimation

| Reference | URL | Cited in |
|---|---|---|
| Foscarin, Schlüter, Widmer. "Beat this! Accurate beat tracking without DBN postprocessing." ISMIR 2024, arXiv 2407.21658 | https://arxiv.org/abs/2407.21658 | BT, QA, Q7 |
| Böck, Davies. "Deconstruct, analyse, reconstruct: how to improve tempo, beat, and downbeat estimation." ISMIR 2020 | http://archives.ismir.net/ismir2020/paper/000223.pdf | BT, TE |
| Kim, Nam. "All-In-One Metrical and Functional Structure Analysis with Neighborhood Attentions on Demixed Audio." WASPAA 2023, arXiv 2307.16425 | https://arxiv.org/abs/2307.16425 | BT |
| Ahn, Hwang, Jung. "The SMC Blind Spot." arXiv 2605.12287 (2026) | https://arxiv.org/abs/2605.12287 | BT, TE, QA, BL |
| Bretz, Soydaner, Plaat. "Silent Metronome." arXiv 2609.07688 (2026) | — | BT |
| Foscarin, Korzeniowski, Vogl. Masked-diffusion beat tracking, arXiv 2608.04624 | — | BT (watch-list) |
| AlignBeat, arXiv 2510.14391. BeatFM arXiv 2508.09790. HingeNet arXiv 2508.09788. arXiv 2510.25560 | — | BT (watch-list) |
| Ellis. "Beat tracking by dynamic programming." JNMR 36(1), 2007 (not re-fetched) | — | BT |
| Davies, Plumbley. "Context-dependent beat tracking of musical audio." IEEE TASLP 15(3), 2007 (not re-fetched) | — | BT |
| Whiteley, Cemgil, Godsill. Bar-pointer model, ISMIR 2006 (not re-fetched) | — | BT |
| Krebs, Böck, Widmer. "An efficient state-space model for joint tempo and meter tracking." ISMIR 2015 (not re-fetched) | — | BT |
| Böck, Krebs, Widmer. "Accurate tempo estimation based on RNNs and resonating comb filters." ISMIR 2015 (not re-fetched) | — | TE |
| Böck, Krebs, Schedl. Log-filtered spectral flux, ISMIR 2012 (not re-fetched) | — | BT |
| Böck, Widmer. "Maximum Filter Vibrato Suppression for Onset Detection" (SuperFlux). DAFx-13 (not re-fetched) | — | BT, DSP |
| Davies, Böck. TCN beat tracking, EUSIPCO 2019. Böck, Davies, Knees, ISMIR 2019 (not re-fetched) | — | BT |
| Dixon. "Onset detection revisited." DAFx 2006 (not re-fetched) | — | BT |
| Bello et al. "A Tutorial on Onset Detection in Music Signals." IEEE TSAP 2005 (not re-fetched) | — | BT, DSP |
| Duxbury, Bello et al. Complex-domain onset detection, 2003–2004 (not re-fetched) | — | BT, DSP |
| Scheirer. "Tempo and beat analysis of acoustic musical signals." JASA 1998 (not re-fetched) | — | BT, TE |
| Klapuri, Eronen, Astola. Meter analysis, IEEE TASLP 2006 (not re-fetched) | — | BT |
| Goto. Beat tracking with drum patterns, JNMR 2001 (not re-fetched) | — | BT |
| Fitzgerald. "Harmonic/percussive separation using median filtering." DAFx-10 | — | BT, TS, DSP |
| Grosche, Müller. PLP / Fourier tempogram, IEEE TASLP 2011 (not re-fetched) | — | TE |
| Peeters. Tempo template/combination, EURASIP JASP 2007 (not re-fetched) | — | TE |
| Percival, Tzanetakis. Streamlined tempo estimation, IEEE/ACM TASLP 2014 (not re-fetched) | — | TE |
| Schreiber, Müller. CNN tempo estimation, ISMIR 2018 (not re-fetched) | — | TE |
| Holzapfel, Davies, Zapata, Oliveira, Gouyon. "Selective sampling for beat tracking evaluation." IEEE TASLP 2012 (not re-fetched) | — | BT |
| Zapata, Davies, Gómez. "Multi-feature beat tracking." IEEE/ACM TASLP 2014 (not re-fetched) | — | BT |
| Killick et al. PELT change-point detection, JASA 2012 (not re-fetched) | — | TE |
| Gouyon et al. Tempo induction evaluation, 2006. ISMIR 2004 tempo contest (Acc1/Acc2) | https://mtg.upf.edu/ismir2004/contest/tempoContest/node7.html | QA, Q7 |

## 2. Time-scale modification and DSP

| Reference | URL | Cited in |
|---|---|---|
| Driedger, Müller. "A Review of Time-Scale Modification of Music Signals." Appl. Sci. 6(2):57, 2016 | https://www.mdpi.com/2076-3417/6/2/57 (text: https://www.cs.bu.edu/fac/snyder/cs583/Literature%20and%20Resources/AReviewOfTimeScaleModification.pdf) | TS, DSP |
| Driedger, Müller. "TSM Toolbox: MATLAB Implementations of TSM Algorithms." DAFx-14 (GPL, reference only) | https://dafx14.fau.de/papers/dafx14_jonathan_driedger_tsm_toolbox_matlab_imple.pdf | TS, DSP |
| Driedger, Müller, Ewert. "Improving TSM of Music Signals Using Harmonic-Percussive Separation." IEEE SPL 21(1):105–109, 2014 | https://audiolabs-erlangen.com/resources/2014-SPL-HPTSM/ | TS |
| Průša, Holighaus. "Phase Vocoder Done Right." EUSIPCO 2017, arXiv 2202.07382 | https://ar5iv.arxiv.org/html/2202.07382 | TS, DSP |
| Röbel. "A new approach to transient processing in the phase vocoder." DAFx-03 | https://hal.archives-ouvertes.fr/hal-01161124 | TS |
| Damskägg, Välimäki. "Audio Time Stretching Using Fuzzy Classification of Spectral Bins." Appl. Sci. 7(12):1293, 2017 | https://www.mdpi.com/2076-3417/7/12/1293 | TS, DSP |
| Laroche, Dolson. "Improved phase vocoder time-scale modification of audio." IEEE TSAP 7(3):323–332, 1999 (via Driedger & Müller) | — | TS, DSP |
| Puckette 1995 (phase locking). Bonada 2000. Duxbury et al. 2002 AES (phase reset) (via BL/TS) | — | TS, BL |
| Verhelst, Roelands. WSOLA, 1993. Roucos, Wilgus. SOLA, 1985. Moulines, Charpentier. TD-PSOLA, 1990. Moinet, Dutoit. PVSOLA, 2011 | — | TS |
| Signalsmith Audio. "Four Ways To Write A Pitch-Shifter." ADC22 talk | https://www.youtube.com/watch?v=fJUmmcGKZMI | TS |
| Harris. "On the Use of Windows for Harmonic Analysis with the DFT." Proc. IEEE 1978 (not re-fetched) | — | DSP |

## 3. Quality evaluation, listening tests, metrics

| Reference | URL | Cited in |
|---|---|---|
| Roberts, Paliwal. "A time-scale modification dataset with subjective quality labels." JASA 148(1):201–210, 2020 | https://arxiv.org/abs/2006.00848 | TS, DSP, QA, Q7 |
| TSM subjective-quality dataset (IEEE DataPort, licence not stated) | https://ieee-dataport.org/open-access/time-scale-modification-dataset-subjective-quality-labels | TS, QA |
| Roberts, Paliwal. Double-ended objective TSM measure, arXiv 2006.06153 | https://arxiv.org/abs/2006.06153 | QA, Q7 |
| Roberts, Paliwal. Single-ended TSM measure, arXiv 2009.02940 | https://arxiv.org/abs/2009.02940 | QA, Q7 |
| zygurt/TSM code (no licence: all rights reserved) | https://github.com/zygurt/TSM | QA, Q7 |
| Fierro, Välimäki. Objective TSM evaluation / enhanced 3-way decomposition | https://research.aalto.fi/fi/publications/towards-objective-evaluation-of-audio-time-scale-modification-met/ ; https://arxiv.org/abs/2210.14041 | TS, DSP |
| Variable-rate HPS-TSM pairwise evaluation (24 participants), arXiv 2609.18999 | https://arxiv.org/abs/2609.18999 | QA, Q7 |
| SELEBI (percussion smearing), arXiv 2602.16421 | https://arxiv.org/abs/2602.16421 | QA |
| ITU-R BS.1534-3 (MUSHRA) | https://www.itu.int/rec/R-REC-BS.1534 | QA, Q7 |
| ITU-R BS.1116-3 (small impairments) | https://www.itu.int/rec/R-REC-BS.1116-3-201502-I | QA, Q7 |
| MUSHRA overview (Wikipedia). TestDevLab MUSHRA methodology (secondary) | https://en.wikipedia.org/wiki/MUSHRA ; https://www.testdevlab.com/blog/how-we-conduct-listening-tests-mushra-methodology-from-itu-r-recommendations | QA, Q7 |
| webMUSHRA (AudioLabs, custom free licence, no patent grant) | https://github.com/audiolabs/webMUSHRA | QA, Q7 |
| ViSQOL (Apache-2.0) | https://github.com/google/visqol | Q7 |
| mir_eval docs (beat, tempo), source, PyPI 0.8.2 (MIT) | https://mir-eval.readthedocs.io/latest/api/beat.html ; https://mir-eval.readthedocs.io/latest/api/tempo.html ; https://github.com/mir-evaluation/mir_eval ; https://github.com/mir-evaluation/mir_eval/blob/main/mir_eval/beat.py ; https://pypi.org/project/mir_eval/ | QA, Q7 |
| Pitch JND (Wikipedia, secondary) | https://en.wikipedia.org/wiki/Just-noticeable_difference | Q7 |

## 4. Timing perception and visualisation

| Reference | URL | Cited in |
|---|---|---|
| Drake, Botte. "Tempo sensitivity in auditory sequences: Evidence for a multiple-look model." Perception & Psychophysics, 1993 | https://link.springer.com/article/10.3758/BF03205262 | UI |
| Friberg, Sundberg. Displacement JND, ASA abstract. JASA 98:2524–2531, 1995 | https://auditory.org/asamtgs/asa93dnv/4pPP/4pPP2.html | UI |
| Dahl, Granqvist. Tempo-drift JND, Ann. N.Y. Acad. Sci. 999:161–165, 2003 | https://vbn.aau.dk/ws/portalfiles/portal/65338357/DahlGranqvistVenezia_proc3subm_openacc.pdf | UI |
| Repp on Weber's law breakdown below 200–300 ms (auditory list) | https://auditory.org/postings/2007/341.html | UI |
| McKinney, Moelants. "Ambiguity in tempo perception." RPPW / Music Perception 2006 | https://cspeech.ucd.ie/rppw/rppw10/mckinney/abstract.html | TE |
| van Noorden, Moelants 1999 (resonance model). Moelants 2002 (preferred tempo) (via search summary) | — | TE |
| Okabe-Ito colour-universal palette | https://conceptviz.app/blog/okabe-ito-palette-hex-codes-complete-reference ; https://siegal.bio.nyu.edu/color-palette/ | UI |
| W3C. What's New in WCAG 2.2 | https://www.w3.org/WAI/standards-guidelines/wcag/new-in-22/ | UI |
| Chromium `animation_win.cc` (reduced motion on Windows). Vispero note | https://chromium.googlesource.com/chromium/src.git/+/HEAD/ui/gfx/animation/animation_win.cc ; https://vispero.com/resources/short-note-on-prefers-reduced-motion-and-puzzled-windows-users/ | UI |
| BBC audiowaveform data format. peaks.js (Context7 `/bbc/peaks.js`) | https://github.com/bbc/audiowaveform/blob/master/doc/DataFormat.md | UI |
| Audacity waveform summaries (doxygen) | https://doxy.audacityteam.org/_get_wave_display_8cpp_source.html ; https://doxy.audacityteam.org/class_sqlite_sample_block.html | UI |
| REAPER `.reapeaks` format | https://www.reaper.fm/sdk/reapeaks.txt | UI |

## 5. Datasets and annotations

| Reference | URL | Licence note (per QA) | Cited in |
|---|---|---|---|
| Beat This annotations (16 datasets) | https://github.com/CPJKU/beat_this_annotations | MIT, no audio | QA |
| Harmonix Set | https://github.com/urinieto/harmonixset | Annotations MIT. Audio not distributed | QA |
| mirdata quick reference, Ballroom/Hainsworth loaders | https://mirdata.readthedocs.io/en/latest/source/quick_reference.html ; https://mirdata.readthedocs.io/en/latest/_modules/mirdata/datasets/ballroom.html | Disagreement → treat as NC | QA |
| Sturm. GTZAN faults, arXiv 1306.1461 | https://arxiv.org/abs/1306.1461 | No licence, commercial audio | QA |
| SMC dataset | https://www.csd.uoc.gr/~hannover/Datasets.html | On request, licence unpublished | QA |
| RWC Music Database | https://staff.aist.go.jp/m.goto/RWC-MDB/ | Research-use pledge | QA |
| GiantSteps Tempo | https://github.com/GiantSteps/giantsteps-tempo-dataset | Annotations CC BY-SA 4.0 | QA |
| Groove MIDI Dataset / E-GMD (Google Magenta) | https://magenta.tensorflow.org/datasets/groove ; https://magenta.tensorflow.org/datasets/e-gmd | CC BY 4.0 | QA |
| FMA | https://github.com/mdeff/fma | Per-track licences | QA |
| MTG-Jamendo | https://github.com/MTG/mtg-jamendo-dataset | Packaging NC | QA |
| ccMixter (Wikipedia) | https://en.wikipedia.org/wiki/CcMixter | Per-upload CC | QA |
| Isophonics Beatles annotations, Hainsworth, Ballroom (via mirdata) | — | Commercial audio | QA |

## 6. Libraries, runtimes, models

| Reference | URL | Licence | Cited in |
|---|---|---|---|
| Beat This! repo (code + weights) | https://github.com/CPJKU/beat_this ; licence section https://github.com/CPJKU/beat_this#license | MIT + data caveat | BT, LE, LIC |
| Beat This ONNX exports | https://huggingface.co/musetric/beat-this-onnx ; https://huggingface.co/ashudesai/songbird-models | — | BT, BL |
| beat_this_cpp | https://github.com/mosynthkey/beat_this_cpp | MIT | LE |
| beat-this Rust crate 1.1.0 | https://crates.io/crates/beat-this ; https://docs.rs/crate/beat-this/latest | MIT | BT, LE |
| madmom (code BSD, models CC BY-NC-SA) | https://github.com/CPJKU/madmom ; https://github.com/CPJKU/madmom/blob/main/LICENSE | — | BT, LE, LIC |
| BeatNet | https://github.com/mjhydri/BeatNet | CC BY 4.0 | BT, LE, LIC |
| All-In-One | https://github.com/mir-aidj/all-in-one | MIT (weights UNVERIFIED) | BT |
| aislice 0.1.0 | https://pypi.org/project/aislice/ | MIT | BT, TE |
| ONNX Runtime 1.30 (repo, releases, privacy/telemetry, DirectML status) | https://github.com/microsoft/onnxruntime ; https://github.com/microsoft/onnxruntime/releases ; https://raw.githubusercontent.com/Microsoft/onnxruntime/HEAD/docs/Privacy.md ; https://onnxruntime.ai/docs/execution-providers/DirectML-ExecutionProvider.html | MIT | LE, AR |
| Signalsmith Stretch | https://github.com/Signalsmith-Audio/signalsmith-stretch | MIT | TS, LE, AR |
| Bungee | https://github.com/bungee-audio-stretch/bungee | MPL-2.0 | TS, LE |
| Rubber Band Library | https://github.com/breakfastquay/rubberband ; https://breakfastquay.com/rubberband/ | GPL-2.0+ / commercial | TS, LE |
| Audacity (StaffPad TimeAndPitch, GPLv2+ default) | https://github.com/audacity/audacity ; https://github.com/audacity/audacity/blob/master/LICENSE.txt | GPL | LE, LIC |
| JUCE (repo, releases, LICENSE) | https://github.com/juce-framework/JUCE ; https://api.github.com/repos/juce-framework/JUCE/releases ; https://github.com/juce-framework/JUCE/blob/master/LICENSE.md | AGPLv3 / JUCE 9 EULA | LE, LIC, AR |
| r8brain-free-src | https://github.com/avaneev/r8brain-free-src ; https://github.com/avaneev/r8brain-free-src/blob/master/LICENSE | MIT | LE, AR, ALG |
| libsamplerate API notes | https://github.com/libsndfile/libsamplerate/blob/master/docs/api_misc.md | BSD-2 | AQ, ALG |
| libsndfile (repo, API) | https://github.com/libsndfile/libsndfile ; https://libsndfile.github.io/libsndfile/api.html | LGPL-2.1 | LE |
| FLAC 1.4.0 release notes | https://xiph.org/flac/2022/09/09/flac-1-4-0-released.html | BSD-3 | AQ |
| fdk-aac NOTICE (non-free) | https://github.com/mstorsjo/fdk-aac/blob/master/NOTICE | Excluded | LE, LIC |
| cpal 0.18.2 DeviceTrait | https://docs.rs/cpal/0.18.2/cpal/traits/trait.DeviceTrait.html | Apache-2.0 | AR |
| Velopack | https://github.com/velopack/velopack | MIT | AR |
| vcpkg ports | https://github.com/microsoft/vcpkg/tree/master/ports | — | AR |
| Qt versions (endoflife.date) | https://endoflife.date/qt | — | LE |
| FFmpeg 8.1 release (secondary) | https://gigazine.net/news/20260317-ffmpeg-8-1-hoare/ | — | LE |

Context7 library IDs used: `/cpjku/madmom`, `/websites/librosa_doc`, `/signalsmith-audio/signalsmith-stretch`, `/colinmarc/signalsmith-stretch-rs`, `/breakfastquay/rubberband`, `/pdeljanov/symphonia`, `/henquist/rubato`, `/libsndfile/libsamplerate`, `/websites/juce_master`, `/websites/onnxruntime_ai_api_c`, `/pykeio/ort`, `/websites/tauri_app`, `/bbc/peaks.js`.

## 7. Licences, patents, legal

| Reference | URL | Cited in |
|---|---|---|
| JUCE 8 EULA tier announcements. JUCE 9 "no pricing or EULA changes". JUCE 9 licence page (did not render) | https://forum.juce.com/t/important-changes-to-the-juce-end-user-licence-agreement-for-juce-8/61265 ; https://forum.juce.com/t/amendments-to-the-juce-end-user-licence-agreement-for-juce-8/61265 ; https://forum.juce.com/t/juce-9-no-pricing-or-eula-changes/69000 ; https://juce.com/legal/juce-9-licence/ | LE, LIC, AR |
| Rubber Band licence and prices | https://breakfastquay.com/technology/license.html ; https://breakfastquay.com/rubberband/license.html | LE, LIC |
| FFmpeg legal (LGPL checklist) | https://ffmpeg.org/legal.html | LE, LIC |
| Qt LGPL obligations, licensing, Small Business terms/pricing | https://www.qt.io/licensing/open-source-lgpl-obligations ; https://doc.qt.io/qt-6/licensing.html ; https://www.qt.io/terms-conditions/qt-dev-framework/exhibit-small-business-2026-01 ; https://www.qt.io/pricing/qt-for-small-business | LE, LIC, AR |
| zplane élastique licensing | https://licensing.zplane.de/licensing | LE, LIC |
| Via LA AAC patent pool | https://www.via-la.com/licensing-2/aac/ | LE, LIC |
| AAC patent expiry estimates (WorldDAB) | https://www.worlddab.org/resources/dab-plus-patent-information | LIC |
| MP3 patent expiry (secondary) | https://i-programmer.info/news/181-algorithms/10750-mp3-free-at-last.html ; https://www.digitalmusicnews.com/2017/05/12/the-death-of-mp3/ | LIC |
| Fraunhofer US 9,230,558 B2 (related US 9,236,062, US 9,275,652) | https://patents.google.com/patent/US9230558B2/en | TS |
| Windows Media Foundation AAC encoder | https://learn.microsoft.com/en-us/windows/win32/medfound/aac-encoder | LE, LIC |
| Suno Terms of Service (rev. 2026-08-10) | https://suno.com/terms | QA |
| Image-Line NewTime getting-started news (trademark context) | https://www.image-line.com/fl-studio-news/newtime-getting-started | LIC |

## 8. Codecs, formats, standards

| Reference | URL | Cited in |
|---|---|---|
| LAME tech FAQ (encoder delay/padding) | https://lame.sourceforge.io/tech-FAQ.txt | AQ |
| LAME/Xing info tag spec (fetch failed this session) | http://gabriel.mp3-tech.org/mp3infotag.html | AQ |
| LAME tag description (libzplay) | https://libzplay.sourceforge.net/LAMETAG.html | AQ |
| OpenMPT bug 725 (576 + 529 = 1105). Hydrogenaudio thread | https://bugs.openmpt.org/view.php?id=725 ; https://hydrogenaudio.org/index.php/topic,61917.0.html | AQ |
| Apple TN2258 (AAC encoder delay) | https://developer.apple.com/library/archive/technotes/tn2258/_index.html | AQ |
| iTunSMPB / encoder delay discussions | https://github.com/uupaa/AAC.js/wiki/EncoderDelay ; https://lists.ffmpeg.org/pipermail/ffmpeg-devel/2012-July/127834.html | AQ |
| RFC 7845 Ogg Opus | https://www.rfc-editor.org/rfc/rfc7845.html | AQ |
| opusenc pre-skip 312 (VLC issue) | https://code.videolan.org/videolan/vlc/-/issues/19242 | AQ |
| RFC 9639 FLAC | https://www.rfc-editor.org/rfc/rfc9639.html | AQ |
| ISO/IEC 11172-3 (MPEG-1 Layer III) | — | AQ |
| EBU Tech 3306 (RF64). ITU-R BS.2088 (BW64). Microsoft WAVEFORMATEXTENSIBLE guidance | — | AQ |
| ITU-R BS.1770-5 (loudness, true peak) | https://www.itu.int/rec/R-REC-BS.1770-5-202311-I | AQ, DSP, QA, Q7 |
| EBU R128. EBU Tech 3342 (LRA). AES TD1008 (via secondary) | — | AQ |
| True-peak / TD1008 / Spotify summary (secondary) | https://www.forasoft.com/learn/audio-for-video/articles-audio/true-peak-dbtp-inter-sample | AQ |
| Tandem coding (AudioLabs AES tutorial). EBU Tech Review 304 (cascading) | https://audiolabs-erlangen.de/content/resources/aesCodingTutorial/tandem.html ; https://tech.ebu.ch/files/live/sites/tech/files/shared/techreview/trev_304-cascading.pdf | AQ |

## 9. Resampling and dither theory

| Reference | URL | Cited in |
|---|---|---|
| J.O. Smith. Digital Audio Resampling Home Page, CCRMA | https://ccrma.stanford.edu/~jos/resample/ | ALG (RESAMPLING) |
| Oppenheim, Schafer. *Discrete-Time Signal Processing* §7.6 (Kaiser design) | — | ALG (RESAMPLING) |
| Lipshitz, Wannamaker, Vanderkooy. "Quantization and Dither: A Theoretical Survey." JAES 40(5), 1992 | — | ALG (DITHERING) |
| Wannamaker. "Psychoacoustically Optimal Noise Shaping." JAES 40(7/8), 1992. Lipshitz, Vanderkooy, Wannamaker. "Minimally Audible Noise Shaping." JAES 39(11), 1991 | — | ALG (DITHERING) |
| SoX `dither` effect source (coefficients to verify) | — | ALG (DITHERING) |
| Whittaker–Eilers smoother. Huber IRLS. Theil–Sen. Fritsch–Carlson PCHIP (standard methods) | — | ALG (WARP_FUNCTION, BPM_ESTIMATION) |

## 10. Competitors and products (official docs, reviews)

| Reference | URL | Cited in |
|---|---|---|
| FL Studio NewTime manual | https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Newtime.htm | CR, UI, LIC |
| FL Studio edition comparison | https://www.image-line.com/flstudio-feature-comparison/index.php | CR |
| Ableton Live manual: Audio Clips, Tempo, and Warping | https://www.ableton.com/en/manual/audio-clips-tempo-and-warping/ | CR, UI |
| Logic Smart Tempo (Sound On Sound, two parts). Apple Flex Time algorithms | https://www.soundonsound.com/techniques/logics-smart-tempo-part-1 ; https://www.soundonsound.com/techniques/logic-pro-using-smart-tempo ; https://help.apple.com/logicpro/mac/10.1/en.lproj/lgcpa77a4a3f.html | CR |
| Cubase Tempo Detection panel. SOS Timing Correction. MusicTech Cubase 14 | https://archive.steinberg.help/cubase_pro/v12/en/cubase_nuendo/topics/editing_tempo_and_signature/editing_tempo_tempo_detection_panel_r.html ; https://www.soundonsound.com/techniques/timing-correction ; https://musictech.com/news/gear/cubase-14-new-features-pricing-availability/ | CR |
| Studio One / Fender Studio Pro | https://ask.audio/articles/presonus-studio-one-time-correcting-audio ; https://www.soundonsound.com/news/studio-one-becomes-fender-studio-pro ; https://synthanatomy.com/2026/01/fender-studio-pro-the-new-studio-one-daw-with-presonus.html | CR |
| Celemony Melodyne 5: Assignment vs Editing | https://helpcenter.celemony.com/M5/doc/melodyneStudio5Training/en/HC2-Training_R1T2_Zuweisung_vs_Bearbeitung_M5 | CR |
| Pro Tools Elastic Audio (Avid guide, Dummies) | https://www.avid.com/pro-tools/user-guide/elastic-audio ; https://www.dummies.com/art-center/music/pro-tools-plug-in-fixing-your-timing-with-elastic-audio/ | CR |
| REAPER purchase. SOS "Stretch Time" | https://www.reaper.fm/purchase.php ; https://www.soundonsound.com/techniques/stretch-time | CR, UI |
| Serato DJ 5.0 beta review. rekordbox. DJ.Studio grid modes. Mixed In Key 11 | https://www.digitaldjtips.com/serato-fixes-its-beatgrids-in-dj-5-0-beta/ ; https://rekordbox.com/en/connect/pointblank/dj-programmes/ ; https://help.dj.studio/en/articles/8867579-fixing-beat-grid-issues ; https://wearecrossfader.co.uk/blog/mixed-in-key-11/ | CR |
| Moises. RipX DAW 8 | https://aipure.ai/products/moises ; https://www.soundonsound.com/news/hitnmix-ripx-daw-8-now-available | CR |
| Web fixers: Neural Analog, Wondera (403, UNVERIFIED) | https://neuralanalog.com/fix-suno-tempo ; https://www.wondera.ai/tools/en/ai-align-tempo-120-bpm-tools | CR |

## 11. Suno and AI-music sources

| Reference | URL | Conf. | Cited in |
|---|---|---|---|
| Suno help: Fixing Tempo Drift | https://help.suno.com/en/articles/8363457 | High | CR, BT, TE |
| Suno help: Exporting from Studio | https://help.suno.com/en/articles/8128193 | High | BT |
| Suno help: Download file types | https://help.suno.com/en/articles/13926081 | High | CR |
| Suno blog: Studio 1.2 | https://about.suno.com/blog/studio1_2 | High | CR |
| Suno Premier price (secondary) | https://gptprompts.ai/suno-pricing | Med | CR |
| Suno output format claims (secondary, self-disclaimed) | https://musicmake.ai/blog/suno-v5-audio-quality-khz-2026 ; https://muapi.ai/playground/suno-convert-to-wav | Low | AQ |
| Udio downloads disabled | https://routenote.com/blog/udio-stops-user-downloads-after-umg-deal-heres-why/ ; https://www.digitalmusicnews.com/2025/11/20/will-udio-still-exist-in-2026/ | Med | CR |
| Neural Analog FAQ on Suno drift | https://neuralanalog.com/faq/why-does-a-suno-song-speed-up-or-slow-down | Low | BT |

## 12. Build, packaging, distribution, signing

| Reference | URL | Cited in |
|---|---|---|
| Tauri 2 updater and Windows installer docs | https://v2.tauri.app/plugin/updater/ ; https://v2.tauri.app/distribute/windows-installer/ | AR |
| Microsoft Artifact Signing quickstart and pricing | https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart ; https://azure.microsoft.com/pricing/details/artifact-signing/ | AR |
| EV certificates no longer grant instant SmartScreen reputation | https://www.todesktop.com/blog/posts/windows-apps-psa-ev-certs-do-not-grant-immediate-reputation-anymore ; https://learn.microsoft.com/en-us/answers/questions/1850140/ | AR |
| 460-day code-signing certificate validity (CA/B CSC-31) | https://www.digicert.com/blog/understanding-the-new-code-signing-certificate-validity-change ; https://knowledge.digicert.com/alerts/code-signing-certificates-459-day-validity | AR |
| Microsoft Store free registration (individuals 2025-09, companies 2026-05) | https://blogs.windows.com/windowsdeveloper/2025/09/10/free-developer-registration-for-individual-developers-on-microsoft-store/ ; https://blogs.windows.com/windowsdeveloper/2026/05/07/publish-to-microsoft-store-as-a-company-now-with-free-registration-and-faster-onboarding/ | AR |
| Inno Setup licensing | https://jrsoftware.org/isorder.php ; https://jrsoftware.org/ishelp/topic_purchase.htm | AR |
| WiX Open Source Maintenance Fee | https://docs.firegiant.com/wix/osmf/ | AR |

## 13. Anecdotal / low-confidence sources

Used only as evidence that a pain or workflow exists, never for numbers.

| Reference | URL | Cited in |
|---|---|---|
| Image-Line forum NewTime threads (summarised via search snippets, pages returned 403) | https://forum.image-line.com/viewtopic.php?p=1488249 ; https://forum.image-line.com/viewtopic.php?p=1998516 ; https://forum.image-line.com/viewtopic.php?p=2060231 | CR |
| Ableton forum warp complaints | https://forum.ableton.com/viewtopic.php?p=1699409 | CR |
| Suno user reports (Reddit snapshots, 2026-03) | https://reddit.sentinel-team.org/posts/1ro2exx/snapshots/2026-03-14T01%3A32%3A40.93927Z ; https://reddit.sentinel-team.org/posts/1rom7tx/snapshots/2026-03-14T01%3A32%3A40.93927Z ; https://reddit.sentinel-team.org/posts/1rqy3l7/snapshots/2026-03-14T01%3A32%3A40.93927Z | CR |
| Suno workflow blogs | https://www.jgbeatslab.com/ai-music-lab-blog/suno-studio-fix-tempo-drift-daw-sync ; https://jackrighteous.com/en-us/blogs/guides-using-suno-ai-music-creation/fix-timing-warp-quantize-suno-studio-1-2 ; https://audiosupport.co.uk/suno-ai-logic-pro-sync-fix-audio-drift-tempo-issues/ | CR, BT |
| genxnotes "107.553 BPM" case study (page 403, cited via search) | — | CR, TE |

## 14. Local evidence (this repository)

| Item | Path | Cited in |
|---|---|---|
| Codec offset / TSM peak / generation-loss experiment (ffmpeg 8.1.2 + numpy) | `_evidence/agentE_codec_tsm_generation_exp.py`, `_evidence/agentE_results_run1.json` | AQ |
| TPDF dither statistics check | `_evidence/agentE_dither_check.py` | AQ, ALG (DITHERING) |
