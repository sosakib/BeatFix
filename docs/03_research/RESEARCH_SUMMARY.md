# Research Summary

Status: 2026-10-08, v1.0 (synthesis of research tracks A–H). Decisions derived from this research are final in [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md). Where research and DECISION_SUMMARY differ, DECISION_SUMMARY wins (§3 below lists every such case). Bibliography: [REFERENCES.md](REFERENCES.md).

## Objective
Give one place that states what the research established, how sure it is, where sources disagreed and how that was settled, and the explicit answers to brief §35 (20 research questions) and §36 (AI/ML policy). It also states realistic quality limits. Detail stays in the track documents. This file links to them.

## Why It Matters
An implementation agent needs to know which numbers are measured, which are cited, and which are guesses. Most of the risk in BeatFix sits in a small set of unverified items: Suno drift statistics, engine quality on AI masters, performance on the reference laptop, and legal questions. These are collected in §6–7.

Confidence labels follow the track docs: **High** (primary source or derivation), **Med** (secondary or partial), **Low** (anecdotal/single source), **UNVERIFIED**, **MEASURED** (reproduced locally, AUDIO_QUALITY_RESEARCH §4.12), **for legal review**.

---

## 1. Tracks and documents

| Track | Agent | Documents |
|---|---|---|
| A. DSP / time-stretch | A | [TIME_STRETCH_RESEARCH.md](TIME_STRETCH_RESEARCH.md), [DSP_RESEARCH.md](DSP_RESEARCH.md) |
| B. Beat tracking / tempo | B | [BEAT_TRACKING_RESEARCH.md](BEAT_TRACKING_RESEARCH.md), [TEMPO_ESTIMATION_RESEARCH.md](TEMPO_ESTIMATION_RESEARCH.md) |
| C. Libraries / licensing | C | [LIBRARY_EVALUATION.md](LIBRARY_EVALUATION.md), [LICENSING_RESEARCH.md](LICENSING_RESEARCH.md) |
| D. Product / competitors | D | [COMPETITOR_RESEARCH.md](COMPETITOR_RESEARCH.md), [USER_PERSONAS.md](../01_product/USER_PERSONAS.md) |
| E. Audio quality / IO | E | [AUDIO_QUALITY_RESEARCH.md](AUDIO_QUALITY_RESEARCH.md) (+ `_evidence/` scripts and JSON) |
| F. UI/UX / visualisation | F | [UI_UX_RESEARCH.md](UI_UX_RESEARCH.md) |
| G. QA / benchmarking | G | [QA_RESEARCH.md](QA_RESEARCH.md) |
| H. Architecture / build | H | [ARCHITECTURE_RESEARCH.md](ARCHITECTURE_RESEARCH.md) |
| Baseline | orchestrator | [_baseline_research_log.md](_baseline_research_log.md) |

## 2. Key findings per track

### A. DSP / time-stretch
| Finding | Conf. | Consequence |
|---|---|---|
| Variable-ratio TSM = fixed synthesis hop, analysis positions from W⁻¹ (Driedger & Müller 2016 eq. 34–35) | High | All engines are driven by W⁻¹ in f64 (TS F1) |
| Any α ≠ 1 breaks vertical phase coherence: phasiness and smear appear even at α = 0.98. Worked example: 440 Hz drifts 1.19 rad/frame at 108→110 BPM | High (derivation) | "Small ratio" ≠ "no artifacts". Phase-locked/phase-gradient PV required (TS F2, TD-10) |
| Signalsmith Stretch (MIT 1.4.0) takes per-call in/out counts (native variable ratio) and links stereo per bin (input IPD preserved) | High (source read) | Default "Smooth" engine (ADR-006) |
| HPSS hybrid (PV on harmonic + short OLA on percussive) preserves transients without explicit detection | High | "Transient" mode, Phase 1.5 |
| Transient cut/stretch/reinsert is claimed by Fraunhofer US 9,230,558 B2 (expected expiry 2029-02-17) | Med | Slice is blocked pending **legal review** |
| Region-level Auto: little gain at ±5 %, sources overlap in time in a mix, seams are audible, misclassification is silent | Med | No region switching in MVP. Long-term Auto = soft per-bin inside one engine (TS TD-7) |
| Objective TSM measures correlate poorly with listening (Roberts & Paliwal 2020, 42,529 ratings) | High | Listening tests gate quality. Metrics are regression guards only |
| f32 sample positions fail above 2²⁴ samples (349.5 s at 48 kHz) | High | f64/i64 for all time (ADR-002) |

### B. Beat tracking / tempo
| Finding | Conf. | Consequence |
|---|---|---|
| Beat This! (ISMIR 2024): MIT code **and** weights, beats + downbeats, 50 fps. GTZAN (unseen) F1 89.1 / CMLt 79.8. Pop-like sets F1 95–97. SMC 62.7 | High | Primary activation model (ADR-003). Training-data caveat → **legal review** |
| Beat This continuity is weaker (non-periodic beats on complex pieces). The minimal post-processing has no continuity model | High | Own DP decoder with ±40 % corridor + repair |
| 50 fps frame quantisation alone gives sd ≈ 5.8 ms. BeatFix needs anchor jitter ≲ 5 ms | High | Parabolic sub-frame + transient snap with global offset |
| DSP finds the period up to octave almost as well as ML (Acc2 0.92–0.97), but octave-correct Acc1 is 0.66 vs 0.83 on GTZAN | High | ML for level, DSP for cross-check/refinement/fallback |
| SOTA Acc1 ≈ 83–87 %: octave ambiguity in roughly 1 song in 6–8 on generic data | High | One-click ×2/÷2 and ranked candidates with margin |
| Fixed tempo floors cause errors (55 BPM floor → double tempo on 21 % of SMC) | Med-High | Candidate range 40–250 BPM |
| A drift of ε = 0.1 % over 400 beats leaves a 17 ms residual vs σ ≈ 5–10 ms detector noise | High (derivation) | *Detecting* drift is easy. *Localising* it needs windows ≥ 16 beats for 0.5 % precision |
| Source separation adds only +0.7 beat / +1.7 downbeat F1 (All-In-One, Harmonix) | High | No Demucs in MVP |
| ORT CPU is not bit-exact across machines/threads | Med | Persist activations + model hash. Re-analysis is explicit |
| Suno drift exists (Suno help). Magnitudes and shapes are anecdotal | High / Low | Corpus of ≥ 50 annotated Suno songs before freezing thresholds |

### C. Libraries / licensing
| Finding | Conf. | Consequence |
|---|---|---|
| Permissive quality TSM: only Signalsmith (MIT) and Bungee (MPL-2.0). Rubber Band is GPL or £590 / £1,490 / £9,320 | High | Signalsmith default. Bungee fallback. RB dev/test only |
| Traps: JUCE-AGPL, Rubber Band GPL, Audacity StaffPad (GPLv2+ by default), aubio GPL, Essentia AGPL, madmom models CC BY-NC-SA, fdk-aac, GPL/nonfree FFmpeg | High | CI licence gate from the first commit (LICENSING_RESEARCH §Technical Details) |
| JUCE 9.0.3 current. Tiers reported unchanged from JUCE 8 (Starter free ≤ $20k, Indie $800 perpetual) | High (version) / Med (prices, forum) | JUCE 9.0.x commercial, never AGPL (ADR-001). Re-verify prices at purchase |
| AAC is still patent-pooled (Via LA, from $0.98/unit). MP3 patents expired 2017 | High / Med | No AAC in MVP. MP3 via LAME DLL |
| r8brain (MIT), libebur128 (MIT), libFLAC (BSD-3), minimp3 (CC0) | High | Adopted (ADR-009) |

### D. Product / competitors
| Finding | Conf. | Consequence |
|---|---|---|
| Suno Studio 1.2 has Manual BPM + warp markers + quantize, Premier-only | High | Suno is a direct competitor. Win on local, transparency, fidelity |
| No tool shows confidence or correction intensity | Med | Heatmap + confidence are the differentiators |
| Octave/meter fixes are manual buttons everywhere | High | Ranked hypotheses |
| NewTime: one BPM per sample, manual markers | Med (inferred from manual) | Whole-song variable tempo is a real step up |
| User pain for AI music is mostly blog/Reddit evidence | Low | Validate with interviews |
Product synthesis: [COMPETITIVE_ANALYSIS.md](../01_product/COMPETITIVE_ANALYSIS.md).

### E. Audio quality / IO
| Finding | Conf. | Consequence |
|---|---|---|
| LAME MP3 offset 576 + 529 = 1105 samples. Untrimmed decode is 25.06 ms late at 44.1 kHz | High, **MEASURED** | Gapless trim on import, LAME tag on export, 0-sample golden test |
| Opus pre-skip 312 @ 48 k. AAC priming 1024/2112 plus untrimmed tail (+616/+768 samples measured) | High, **MEASURED** | Opus/AAC need explicit trimming. Both post-MVP |
| PV-TSM raised sample peak to +0.03…+1.25 dBFS on a −0.1 dBFS limited master. WSOLA did not | Med, **MEASURED** (synthetic, RB R2 engine) | Post-render peak gate, never silent (DECISION_SUMMARY §6) |
| 10 lossy generations: MP3 V2 waveform SNR 15.9 → 11.4 dB, true peak +1.0 → +4.2 dBTP | High (direction) / Low (absolute) | Render from source. Re-import guard tag |
| TSM loudness change −0.2…−0.5 LU | Med, **MEASURED** | Report ΔLUFS. No auto-normalise |
| Suno delivery formats/rates | Low, UNVERIFIED | Probe every import. Report effective bandwidth |

### F. UI/UX
| Finding | Conf. | Consequence |
|---|---|---|
| Min/max/RMS pyramid (base 64, ×4) = 14.4 MB per 10-min stereo lane vs 230 MB PCM. CPU redraw ≈ 41k ops/frame | High | CPU drawing, no GPU in MVP (ADR-008) |
| Monotone W lets the corrected envelope be predicted from the source pyramid via W⁻¹ | High (math) / Med (visual fidelity) | Instant "Estimate" lane, replaced by real peaks |
| Tempo-step JND ≈ 1.5–6 % (clicks). Single-displacement JND ≈ 5 % of IOI. Drift JND ≈ 0.27 %/interval | Med / High / High | Heat bins 0.5 / 2 / 5 %, flag at 12 %. Configurable. Calibrate on music |
| Okabe-Ito blue/orange have equal luminance in dark theme | High | Redundant signed strip + glyphs |
| WCAG 2.2: 2.5.7 Dragging Movements, 2.5.8 Target Size | High | Keyboard/single-pointer paths for every drag |

### G. QA
| Finding | Conf. | Consequence |
|---|---|---|
| mir_eval 0.8.2 (MIT) covers standard beat metrics. It trims beats < 5 s by default | High | Report trimmed + untrimmed. Add fine tempo error, anchor precision, spurious-anchor rate |
| ±70 ms F1 and ±4 % Acc are too lax for warp anchors | High | Product gates on CMLt + ms-level metrics ([METRICS.md](../07_quality/METRICS.md)) |
| Groove MIDI / E-GMD are CC BY 4.0 and can be rendered at any tempo → true native-tempo references | High | Tier M corpus, full-reference metrics |
| Most beat datasets are NC, unlicensed, or contaminated (Beat This trained on them) | High | Local-only, **legal review**. GTZAN as the held-out test |
| Suno ToS forbids using Output to "develop or improve any competing products" or train models | High (text) / Low (interpretation) | Tier U (user Suno tracks) evaluation-only, local-only, **legal review** |
| Expert ≈ non-expert listeners for TSM ratings | High | Remote panels and pairwise tests are acceptable |

### H. Architecture
| Finding | Conf. | Consequence |
|---|---|---|
| A/B quality is stack-independent. The differentiators are FFI count, UI speed, licence | High | C++20 + JUCE (zero FFI on render path). Tauri is the fallback shell |
| Signalsmith API supports chunked render (seek pre-roll, flush, latency queries) | High | Chunk-parallel deterministic render (ADR-007) |
| ORT Windows builds emit telemetry by default | High | `DisableTelemetryEvents()` at init |
| EV certs no longer give instant SmartScreen reputation. Max cert life 460 days from 2026-03 | High | Sign everything. Store channel in Phase 2 |
| vcpkg lags upstream for JUCE, ORT, Signalsmith | High | FetchContent with pinned tags + SHA256. vcpkg only for mp3lame (ADR-013) |

## 3. Disagreements and resolutions

### 3.1 Resolved in DECISION_SUMMARY §6
| Conflict | Sources | Resolution |
|---|---|---|
| A/B crossfade 10 ms vs 20 ms | ARCHITECTURE_RESEARCH §4 vs PLAYBACK_AND_AB | **20 ms equal-power**. The UI spec owns behaviour. Bass-heavy clicks |
| Own polyphase SRC vs library | RESAMPLING (orig.) vs LIBRARY_EVALUATION | **r8brain** + contract tests. Own SRC is the fallback |
| dr_libs vs JUCE readers for decode | LIBRARY_EVALUATION vs ARCHITECTURE_RESEARCH | **JUCE readers + minimp3_ex**. JUCE already a dependency. MP3 needs explicit gapless handling |
| Heat colour on `|log2 r|` vs `|log2 r/g|` | WARP_MAP vs WAVEFORM_VIEW / UI_UX_RESEARCH T5 | **`|log2(r/g)|`**. A global tempo change must not paint the whole song |
| Natural↔Strict detents | WARP_FUNCTION vs MAIN_WINDOW | **6 detents**: Global, 32, 16, 8 (default), 4, Strict |
| Candidate range 50–220 vs 40–250 | BPM_ESTIMATION (orig.) vs BEAT_TRACKING/TEMPO research T3 | **40–250**, ×{1/3…3} |
| AAC/Opus import in MVP | AUDIO_QUALITY_RESEARCH rec. 8 vs brief MVP | **Not in MVP**. AAC licensing. Suno delivers MP3/WAV |
| Export defaults | AUDIO_QUALITY_RESEARCH | WAV 24-bit TPDF, FLAC 24, MP3 CBR 320 + LAME tag. Float never dithered |
| Peak handling | AUDIO_QUALITY_RESEARCH | Post-render gate, user choice, nothing silent |
| Performance targets | PERFORMANCE_BENCHMARKS | Provisional. Calibrated in the Phase 0 spike |
| Analysis determinism | BEAT_TRACKING_RESEARCH, DSP_TEST_STRATEGY | Persist activations + model hash. Determinism levels D1–D3 |
| Intentional tempo change default | TEMPO_ESTIMATION_RESEARCH §6 ("flatten drift, ask for steps > 5 %") | **Flag + ask**: flatten or keep unwarped (r = 1). Never auto-smear |

### 3.2 Resolved inside the research (source vs source)
| Conflict | Pick | Why |
|---|---|---|
| Cubase multitrack AudioWarp: SOS says no, MusicTech says yes | MusicTech | Newer. SOS article predates the feature (COMPETITOR_RESEARCH §4) |
| Suno "WAV (Tempo-Locked)" download: secondary site vs Suno help | Suno help (absent) | Primary source. Verify in-app |
| Ballroom/Hainsworth licence: mirdata docstring (CC BY-NC-SA) vs quick-reference table (CC0) | NC, local-only | The more specific and conservative source. Audio is commercial anyway (QA_RESEARCH F8) |
| Signalsmith licence: Bungee compare page says "Commercial", repo says MIT | MIT | Repo licence file is primary (baseline log) |
| JUCE 8 (brief) vs JUCE 9 (current) | 9.0.x | Current, EULA tiers reported unchanged. Prices to re-verify (ADR-001) |
| Dither on 24-bit: DSP_RESEARCH TD-8 says 16-bit only. AUDIO_QUALITY_RESEARCH/DITHERING say all float→int | Dither all integer targets (TPDF) | DECISION_SUMMARY §6 export defaults. Cost at 24-bit is −144 dBFS |
| ML in MVP: brief §36 "avoid unnecessary model dependencies" vs Beat This adoption | One model, narrowly scoped | Measured benefit on octave level and weak-onset material (§5) |
| "madmom DBN is patented" | Unresolved, low-confidence single source | Avoided by using an own DP decoder. **Legal review** only if a bar-pointer HMM is built |

### 3.3 Residual inconsistencies in current docs (not yet fixed)
| Doc | Says | DECISION_SUMMARY says |
|---|---|---|
| MAIN_WINDOW.md §2, §7; UX_BEHAVIOR.md §8 | Mode menu Auto / Smooth / Transient. Reset sets mode Auto | MVP modes Smooth (default) + Resample. No Auto. Transient Phase 1.5 |
| MAIN_WINDOW.md §4.5 | Candidates `{B̂, B̂/2, 2B̂, 2B̂/3, 3B̂/2}` within 50–220 | ×{1/3…3}, 40–250 |
| EXPORT_UI.md §2 | MP3 default VBR V0. Dither "on when reducing to 16-bit". Peak policy "report and offer gain" | CBR 320. TPDF on 24-bit too. Pre-export gate with three choices |

## 4. Answers to the 20 research questions (brief §35)

| # | Question | Answer | Conf. | Detail |
|---|---|---|---|---|
| 1 | How accurately can beat timing be inferred from a full mix? | Detection on pop-like mixes: F1 95–97 % (±70 ms). Unseen GTZAN 89 %. Expressive/SMC-like 55–63 %. *Timing precision* is the real limit: raw 50 fps gives sd ≈ 5.8 ms before model error. With sub-frame + transient refinement the target is ≤ 5 ms median on percussive material (target, unvalidated). Stems add only +0.7 F1 for detection; their gain is precision (unmeasured) | High (detection) / Med (precision) | BEAT_TRACKING_RESEARCH §3, §5, §8 |
| 2 | How to combine kick/snare evidence with general onsets? | Not as the primary detector. Beats are decoded on the ML activation. Band-wise HPSS-percussive SuperFlux (kick 30–150 Hz, snare 1–5 kHz) is used for (a) transient refinement, (b) backbeat and mid-beat snare scores for octave choice, (c) ghost-note suppression | Med-High | BEAT_TRACKING_RESEARCH §1.5; TEMPO_ESTIMATION_RESEARCH §3 |
| 3 | How to distinguish drift from detector errors? | Four tests. Detector noise is white (IOI lag-1 autocorrelation ≈ −0.5) and bounded (≤ 3σ̂). Drift is low-frequency with large `D_i`. Errors coincide with low confidence and ML↔DSP disagreement. Real drift appears in both trackers. Groove is bar-periodic (residual autocorrelation peaks at lag 2/4) | High | TEMPO_ESTIMATION_RESEARCH §5 |
| 4 | How to avoid destroying intentional groove? | The smoother corrects only deviations slower than N_c beats. Faster deviations (groove, swing, micro-timing) pass through unchanged. Snap offsets are global, not per beat. Intentional steps get a smoother break | High (math) / Med (perception) | WARP_FUNCTION.md §4.2–4.3 |
| 5 | Should correction be 100 % strict by default? | **No.** Strict copies detector noise into the output (`e_out = −r·ε`), which is ≈ 1.4 % ratio noise at σ = 5 ms, 120 BPM. Default Natural, N_c = 8 | High (reasoning) / Med (value 8) | WARP_FUNCTION.md §4.1; DECISION_SUMMARY §3.3 |
| 6 | Would a Natural vs Strict control be useful? | Yes. Groove flattening is a stated user concern, and DJ tools ship Fixed/AI/Flex. Implemented as 6 detents with a live readout of grid error (ms) and max stretch (%) | Med (user evidence) / High (design) | COMPETITOR_RESEARCH F7; MAIN_WINDOW.md §3 |
| 7 | How to infer beat phase? | Beat phase comes from the decoder (DP over activations with time-varying τ). Bar phase and meter come from downbeat activations voted over sliding 16-bar windows with a Viterbi penalising changes. The grid phase φ for rendering comes from the user's phase policy (Preserve start / DAW-ready start). Circular statistics give the phase estimate and its coherence R | Med | BEAT_TRACKING_RESEARCH §4; TARGET_BEAT_GRID.md §3–4 |
| 8 | How to handle intros without drums? | The tempo path carries the neighbouring tempo through low-salience frames. Beats there are marked `inferred` with low weight, so the warp follows the smooth tempo. The first downbeat is extrapolated and must be confirmed when its confidence is low | Med-High | TEMPO_ESTIMATION_RESEARCH §6; WARP_FUNCTION.md §4.2 |
| 9 | How to handle drum fills? | The decoder's transition penalty keeps the period. Fill beats get reduced confidence and weight in the smoother. Transient snap never moves across a stronger competing onset within ±10 ms | Med | BEAT_TRACKING_RESEARCH §6 |
| 10 | Vocals with little rhythmic content? | ML activation handles weak-onset material better than DSP. SuperFlux suppresses vibrato false onsets. If confidence still falls, the span is down-weighted and flagged. Render-side, Smooth (linked PV) is best on vocals; Transient adds nothing there | Med | BEAT_TRACKING_RESEARCH §11; TIME_STRETCH_RESEARCH TD-6 |
| 11 | Sustained pads? | Analysis: as Q8 (no pulse → inferred). Render: PV Smooth is best. OLA/WSOLA would warble. Granular is unnecessary at ±5 % | High (render) / Med (analysis) | TIME_STRETCH_RESEARCH TD-6 |
| 12 | Abrupt tempo changes that may be intentional? | Detect change points (PELT/binary segmentation on log-IOI) and phase jumps first, then smooth per segment. MVP default: flag + ask ("flatten" / "keep section unwarped r = 1"). Splices are never spread over N_c beats | Med-High | TEMPO_ESTIMATION_RESEARCH §5; DECISION_SUMMARY §6 |
| 13 | How should Auto choose stretch modes? | It shouldn't switch per region in the MVP. Long term: soft per-bin classification (HPSS masks or fuzzy tonal/noise/transient memberships) inside one engine, with no seams. Region switching only between Smooth and Transient, at bar boundaries, ≥ 4-bar hysteresis, driven by percussive-energy ratio and onset strength, and only if listening tests show per-bin is not enough. Never auto-select Resample or Slice | Med | TIME_STRETCH_RESEARCH TD-7; [HYBRID_AUTO_ENGINE.md](../04_algorithms/HYBRID_AUTO_ENGINE.md) |
| 14 | Should analysis use multiple frequency bands? | Yes, for DSP features: 24-band log filterbank SuperFlux, per-band flux for kick/snare/hats, HPSS. The ML front-end uses its fixed 128-band mel. Multi-band is for evidence and refinement, not for separate trackers | High | BEAT_TRACKING_RESEARCH §1; DSP_RESEARCH TD-3 |
| 15 | Should the system use ML at all? | Yes, narrowly: one model (Beat This) for frame activations. Everything after is deterministic owned code. See §5 | High | ADR-003 |
| 16 | Where does ML help and where does it hurt? | Helps: metrical level/octave (Acc1 0.83 vs 0.66), weak-onset material, downbeats (no competitive DSP method), ignoring ghost notes. Hurts: confident-but-wrong activations, non-periodic beats, 50 fps quantisation, cross-machine float differences, opaque failures, training-data licence uncertainty. Each harm has a mitigation (§5) | High | BEAT_TRACKING_RESEARCH §11 |
| 17 | Best offline time-stretch strategy? | Phase-locked/phase-gradient PV (Signalsmith) with linked stereo, driven sample-accurately by W⁻¹ on a fixed output grid. f64 positions with an integer rounding accumulator. Chunk-parallel with pre-roll and crossfade. Native sample rate. Rubber Band R3 as the internal quality reference. Transient hybrid as the first upgrade | Med-High (quality unconfirmed by listening) | TIME_STRETCH_RESEARCH rec. 1–4, 9; ADR-006, ADR-007 |
| 18 | Best preview strategy? | The same engine as export, rendered in chunks outward from the playhead. Predicted waveform via W⁻¹ shown instantly. A/B at the same musical position with a 20 ms crossfade. Never play stale audio as B. Fast preset allowed for preview, labelled | Med | ARCHITECTURE_RESEARCH §4–5; PLAYBACK_AND_AB.md §4 |
| 19 | How to cache rendering intelligently? | Content-addressed per-chunk cache. Key = hash(source PCM hash, decoder version, warp inputs, engine id/version + params, render SR, format version). Local edits invalidate only chunks whose source span changed. The key *is* the invalidation rule. LRU eviction, never the current generation. Analysis cached by source PCM key + analyser id | High (design) | ARCHITECTURE_RESEARCH §5; [RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md) |
| 20 | What are realistic quality limits? | See §6 | — | — |

## 5. AI / ML policy (brief §36)

### 5.1 Candidate ML uses vs deterministic alternatives
| Use case (brief §36) | Deterministic alternative | Decision | Reason |
|---|---|---|---|
| Beat / downbeat activation | SuperFlux + DP / HMM | **ML (Beat This)** | Clear measured gain on octave level and downbeats (Acc1 +0.17 on GTZAN). No competitive DSP downbeat method |
| Beat confidence classification | Hand-weighted feature score | **Calibrated logistic regression** (7 parameters) over deterministic features | Explainable, tiny, deterministic at inference. Not a neural model |
| Ambiguous tempo ranking | Perceptual prior only | **Feature score + multinomial logistic fit** (salience, activation contrast, downbeat, backbeat, prior, IOI CV) | Prior alone fails (humans disagree, halftime genres) |
| Transient type classification | HPSS soft masks, flux, flatness | **Deterministic** | Masks are needed anyway for Transient mode |
| Region/source classification | Percussive ratio, onset density, tonalness | **Deterministic**, and post-MVP only | Region switching is not in MVP |
| Time-stretch strategy selection | Rules with hysteresis | **Deterministic**, post-MVP | Same |
| Artifact detection | Mechanism-specific metrics (attack ratio, IPD/ICC, pre-echo, true peak) | **Deterministic** for regression. Listening tests for release | Learned TSM quality measures reach PCC 0.865 at best and are unlicensed (zygurt/TSM has no licence) |
| Source separation, neural TSM, restoration | — | **Not used** | Cost, licence, gain too small (QA, TS, AQ docs) |

### 5.2 The one shipped model: Beat This!
| Aspect | Specification | Source |
|---|---|---|
| Model input | Mono (L+R)/2 resampled to 22.05 kHz (pinned resampler). STFT n_fft 1024, hop 441 (50 fps). 128 mel bands 30 Hz–10 kHz. `ln(1 + 1000x)`. Chunks of 1500 frames, border 6, "keep_first" aggregation, replicated exactly from `inference.py` | BEAT_TRACKING_RESEARCH F2, F9 |
| Model output | Frame-wise beat and downbeat logits at 50 fps. Persisted as float16 (≈ 48 KB per 4 min) | §10 |
| Inference cost | Final model ≈ 20 M params, ~78–80 MB. Rust port: 4:32 song in 4.6 s on Apple M4. x86 estimate 3–15 s per 4-min song (**UNVERIFIED**, Phase 0/3 benchmark). Small model ≈ 8 MB, expected 5–10× faster (UNVERIFIED). Analysis budget ≤ 0.15× real time (provisional) | BEAT_TRACKING_RESEARCH §11; DECISION_SUMMARY §6 |
| Runtime | ONNX Runtime 1.30 CPU EP, our own ONNX export from the official checkpoint in CI, verified against PyTorch on a fixed corpus, hash pinned. Telemetry disabled. Inference cancellable via `RunOptions::SetTerminate()` | ADR-003; ARCHITECTURE_RESEARCH F4 |
| Training / data needs | **None for BeatFix.** Released weights are used as is. No fine-tuning on research datasets with restrictive licences. **Never train or fit on Suno outputs** (Suno ToS item 10). The two small calibration models (§5.1) are fitted on Tier S/M/C corpus data. Fitting them on user Suno tracks (Tier U) is **for legal review** | LICENSING_RESEARCH §10; QA_RESEARCH F19 |
| Fallback | Model missing/corrupt, ORT error, or user choice → DSP tracker (HPSS + SuperFlux pseudo-activation → same decoder). Downbeats from backbeat pattern only. Result labelled "DSP mode", global confidence capped at Medium. The DSP path always runs anyway as cross-check | BEAT_TRACKING_RESEARCH §12–13 |
| Reproducibility | ORT CPU not bit-exact across ISAs/threads → activations + model hash + preprocessing version persisted. Analysis is computed once, stored, and re-run only on explicit user action. Golden tests run ORT with `intra_op_num_threads = 1`. Post-processing in float64, ties broken by lowest index, hysteresis on thresholds so 1e-5 activation perturbations can't flip decisions | BEAT_TRACKING_RESEARCH F10, §10 |
| Offline | Model in the installer. No downloads, no network. Works with the network adapter disabled | ADR-003, ADR-013 |
| Licence | Code **and** weights MIT. README caveat: some training data copyrighted / limited-CC, assessment left to the user → **legal review**. Model hash, source URL and licence recorded in the SBOM. madmom models (CC BY-NC-SA) are never bundled or used to derive weights | LICENSING_RESEARCH §10 |

Watch-list (not shipped; no verified code/licence): masked-diffusion tracking, AlignBeat, BeatFM/HingeNet. The `IBeatAnalyzer` interface keeps them swappable (BEAT_TRACKING_RESEARCH F14).

## 6. Realistic quality limits

| Limit | Value / statement | Conf. | Source |
|---|---|---|---|
| Detection failure classes | Rubato, expressive or beatless music, ballads, halftime sections. SMC-like material: F1 55–63 % for all trackers | High | BEAT_TRACKING_RESEARCH §3 |
| Octave ambiguity | ≈ 13–17 % of generic songs are octave-wrong at rank 1. Halftime genres are ambiguous for humans too, so the result is a labelling choice, not an error | High | TEMPO_ESTIMATION_RESEARCH T2, §3 |
| Anchor precision | Raw 5.8 ms sd (frame quantisation). Target ≤ 5 ms median after refinement on percussive material. Worse on soft onsets | High / Med | BEAT_TRACKING_RESEARCH §5 |
| Local tempo resolution | Per-beat BPM noise ≈ 3.4 BPM at σ = 10 ms. 0.5 % precision needs ≥ 16-beat windows | High (derivation) | TEMPO_ESTIMATION_RESEARCH §5 |
| Splices / phase jumps | No smooth warp can hide them. One IOI absorbs them within the ratio bound, otherwise they need a user decision | High | TEMPO_ESTIMATION_RESEARCH §5 |
| TSM artifacts | Every PV render at r ≠ 1 has some phasiness and transient softening. Quality spread between good engines at ±5 % is narrow. Final quality claims need listening tests | High (mechanism) / Med (magnitude) | TIME_STRETCH_RESEARCH F2–F3 |
| Peak growth | Sample peak +0.03…+1.25 dB on a limited master (R2, synthetic). Assume ≥ 1.5 dB possible. The gate dialog will appear for most loud AI masters | Med, MEASURED | AUDIO_QUALITY_RESEARCH F12 |
| Source ceiling | Output can't exceed source quality. An MP3 source exported as FLAC stays lossy-origin | High | AUDIO_QUALITY_RESEARCH §4.1 |
| Resample mode | Pitch moves 17.2 cents per 1 % tempo change (108→110 BPM = +31.8 cents) | High | TIME_STRETCH_RESEARCH TD-10 |
| Determinism | Bit-identical render on same binary/CPU path. Tolerance-based across CPUs (SIMD/FMA). Analysis reproducible only from persisted activations | High | ARCHITECTURE_RESEARCH §5; DSP_TEST_STRATEGY §6 |
| Objective quality | No objective metric reliably predicts TSM listening scores. Release is gated by listening tests | High | QA_RESEARCH F20–F23 |
| Perceptual floors | Tempo-drift JND ≈ 0.27 %/interval (clicks, 7 subjects). Corrections below that are individually inaudible but fix cumulative drift. Music thresholds are likely higher (assumption) | High / Assumption | UI_UX_RESEARCH F7–F8 |
| Performance | All targets provisional until measured on the reference laptop (analysis ≤ 0.15× RT, Smooth render ≤ 0.25× RT, preview ≤ 500 ms, ≤ 1.0 GB at 10 min/48 kHz, cancel ≤ 250 ms) | Low (unmeasured) | DECISION_SUMMARY §6 |

## 7. Unverified items that carry the most risk

| Item | Why it matters | How it gets resolved |
|---|---|---|
| Suno drift statistics (magnitude, steps, splices, version dependence) | Thresholds, N_c default, one-click success rate | ≥ 50 annotated Suno songs (Tier U, pending legal) + synthetic-drift corpus ([BENCHMARK_CORPUS.md](../07_quality/BENCHMARK_CORPUS.md)) |
| Signalsmith quality on AI masters vs RB R3 / Bungee | Default engine choice | Listening test ([SUBJECTIVE_LISTENING_TESTS.md](../07_quality/SUBJECTIVE_LISTENING_TESTS.md)) |
| Signalsmith latency alignment with variable chunks | Constant offset vs grid | Impulse-train test, error < 0.5 ms |
| Beat This CPU time on x86 | Analysis budget, final vs small model | Phase 0/3 benchmark |
| Suno file formats / bandwidth | Lossy-origin messaging, Opus/AAC priority | Probe 10–20 real files |
| JUCE 9 tier prices | Cost model | Re-verify at purchase |
| Legal queue: Beat This data caveat, Suno ToS item 10, Slice patent, AAC via MF, dataset use, "BeatFix" trademark | Shipping, corpus, scope | [OPEN_QUESTIONS.md](../10_implementation/OPEN_QUESTIONS.md) |

## Recommendation
Build to DECISION_SUMMARY. Before freezing any threshold: (1) the synthetic-drift and MIDI-rendered corpora (Tiers S/M) for ms-level engineering gates, (2) the legal answer on Tier U, then the annotated Suno set, (3) the Phase 0 performance spike, (4) the first engine listening test with RB R3 as the reference condition.

## Risks
- The research is desk research plus synthetic experiments. No real Suno material has been measured yet.
- Several "High" findings are about public datasets that don't resemble AI masters (codec smear, heavy limiting).
- Personas and pain rest on Low-confidence evidence.

## Open Questions
Consolidated in [OPEN_QUESTIONS.md](../10_implementation/OPEN_QUESTIONS.md). The track docs keep their own lists.

## Implementation Implications
- Every placeholder threshold lives in one versioned config and is logged with each analysis/render.
- Persist what can't be recomputed bit-exactly (activations). Recompute everything else from inputs.
- Fix the §3.3 inconsistencies in the UI docs before UI implementation starts.
