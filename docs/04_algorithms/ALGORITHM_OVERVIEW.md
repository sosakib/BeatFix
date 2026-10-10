# Algorithm Overview: End-to-End Pipeline

Brief §26, §42, §43. This is the map. Each stage links to the doc that owns its maths, parameters and tests. Decisions: [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md).

## 1. Pipeline diagram

```
INPUT  file (WAV/AIFF/FLAC/MP3/Ogg)
  │  probe: codec, SR, channels, duration → reject early if unsupported        AUDIO_PIPELINE §2
  ▼
DECODE once, gapless (LAME/Xing trim) → float32 PCM @ native SR, cached by PCM hash
  │  out: PcmCache (immutable)                                               AUDIO_PIPELINE §2–3
  ▼
PRE-ANALYSIS  analysis branch: mono downmix → 22.05 kHz (zero-phase) → mel 128 @ 50 fps
  │  + STFT/HPSS for DSP features                                            ONSET_DETECTION §1–3
  ▼
ONSET / TRANSIENT EXTRACTION  SuperFlux ODF, band ODFs (kick/body/snare/air), hi-res ODF
  │  out: novelty curves, kick/snare evidence, refinement ODF                ONSET_DETECTION §4–6
  ▼
BEAT ACTIVATIONS  Beat This! ONNX (CPU) → beat + downbeat activations (50 fps)
  │  fallback: DSP tracker on SuperFlux (same decoder below)                 BEAT_TRACKING §1, §6
  ▼
TEMPO HYPOTHESES  tempogram + activation periodicity → B̂ × {1/3…3} in 40–250 BPM,
  │  scored (salience, contrast, downbeat consistency, backbeat, prior) → margin G/A/R
  │                                                                          HALF_DOUBLE_TIME_RESOLUTION
  ▼
BEAT TRACKING  owned time-varying DP decoder, ±40 % tempo corridor around chosen level
  │  out: frame-quantised beats                                              BEAT_TRACKING §2–3
  ▼
REPAIR  missing/extra beats via IOI ratio tests vs local period; inferred flags  OUTLIER_REJECTION
  ▼
DOWNBEATS / METER  beat-synchronous bar-phase Viterbi (4/4; 3/4 detected)   DOWNBEAT_ESTIMATION
  ▼
REFINEMENT  parabolic peak + fixed-band onset snap (±25 ms) + global median offset
  │  out: anchors S_i (float64 s)                                            WARP_FUNCTION §5
  ▼
CONFIDENCE  per-beat c_i; global High/Medium/Low/None with reason            BEAT_TRACKING §5, BPM_ESTIMATION §7
  ▼
LOCAL TEMPO + STABILITY  robust line fit (global BPM ± CI), Whittaker IRLS local curve,
  │  drift-vs-noise test, change points, splices → class + g_i breaks
  │                                                         BPM_ESTIMATION, TEMPO_STABILITY_ANALYSIS
  ▼  ─────────────── persisted analysis result (project) ───────────────
TARGET BPM  user/suggested B (round(bpmGlobal) default)                      BPM_ESTIMATION §5
  ▼
TARGET GRID  T_k = φ + k·60/B, phase policy (Preserve start / DAW-ready)     TARGET_BEAT_GRID
  ▼
WARP MAP  Whittaker/Huber-IRLS smoothed anchors x_i (N_c detent, weights c_i, breaks g_i)
  │  → piecewise-linear monotone W, ratios r_i, validate (bounds, monotonicity)
  │                                                         WARP_FUNCTION, WARP_MAP
  ▼
REGION CLASSIFICATION  MVP: per-segment correction class + flags only.
  │  Post-MVP: source-type regions for Auto                                  HYBRID_AUTO_ENGINE
  ▼
STRETCH ENGINE  Smooth = Signalsmith driven by W⁻¹ on a fixed output hop, linked stereo;
  │  Resample = varispeed (constant ratio only); chunked parallel render
  │                                       TIME_STRETCH_ENGINE, STEREO_PHASE_PRESERVATION, RENDER_PIPELINE
  ▼
ARTIFACT CONTROL  chunk cuts at low-RMS beats, pre-roll, overlap crossfade; (1.5: HPSS transient path)
  │                                                         RENDER_PIPELINE §3, TRANSIENT_PRESERVATION
  ▼
QUALITY VALIDATION  NaN/silence/clicks, Path K timing, re-detected beats (Path R),
  │  stereo checks, peak/true-peak, loudness delta                           QUALITY_VALIDATION
  ▼
MASTER BUFFER  float32 render generation (cached, content-addressed)        RENDER_PIPELINE §5–6
  ▼
EXPORT  peak gate → (SRC once, r8brain) → (TPDF dither for int) → encode once → tags
        → re-decode validation → atomic rename          AUDIO_PIPELINE §7, RESAMPLING, DITHERING
```

## 2. Complete pseudocode

```
function processSong(path, settings):
    # ── import ─────────────────────────────────────────────
    info = probe(path);              require supported(info) else fail(F9)
    pcm  = pcmCache.getOrDecode(path, gapless=true)         # once; float32 native SR
    # ── analysis (persisted; re-run only on explicit request) ─
    a  = analysisBranch(pcm)                                 # mono, 22.05 kHz, zero-phase SRC
    odf = onsetFeatures(a)                                   # SuperFlux, bands, hi-res ODF, HPSS
    act = beatModel.available() ? beatThis(a.mel) : dspActivation(odf)   # fallback F11
    cands = tempoCandidates(act, odf)                        # ×{1/3..3}, 40–250 BPM, scored, margin
    level = cands.top                                        # user may relabel later
    beats = dpDecode(act, corridor(level, ±40%))             # time-varying period
    beats = repairMissingExtra(beats, localPeriod)           # inferred flags, low weights
    bars  = decodeDownbeats(act.downbeat, beats)             # meter 4 (3 if detected)
    S     = refine(beats, act, odf.hiRes)                    # sub-frame + band snap + global offset
    conf  = beatConfidence(S, act, odf, cands)               # c_i and global level + reason
    tempo = estimateTempo(S, conf)                           # Huber line fit, Theil–Sen check, CI, range
    stab  = stability(S, conf)                               # drift vs noise, steps, splices, class
    persist(analysis = {act-hash, S, conf, bars, cands, tempo, stab})

    # ── correction (pure, instant, recomputed on every edit) ─
    B   = settings.target or round(tempo.bpmGlobal)
    W   = buildWarp(S, conf, stab.breaks + userDecisions, B,
                    settings.phasePolicy, settings.cutoffBeats)   # WARP_FUNCTION §9
    W.validate() or fail(F6)                                 # monotone, 0.5 ≤ r ≤ 2, warnings
    viewEstimate = mapPeaks(sourcePeaks, W)                  # instant corrected-lane estimate

    # ── render (background, progressive, cached) ─────────────
    plan = chunkPlan(W, pcm)                                 # cuts at low-RMS beats every 8–16 beats
    for chunk in plan ordered by distance to playhead:
        out = cache.get(chunkKey) or stretch(pcm, W⁻¹, chunk, settings.mode)
        validateChunk(out) or retrySinglePass(chunk) or fail(F12)
    gen = stitch(plan)                                       # overlap crossfades
    report = validateRender(gen, W, S)                       # Path K/R, peaks, stereo, loudness

    # ── export (explicit) ──────────────────────────────────
    gen = peakGate(gen, format)                              # user choice if overs; never silent
    gen = srcIfRequested(gen)                                # r8brain, at most once
    pcmOut = quantise(gen, bitDepth, ditherTPDF if integer)
    file = encodeOnce(pcmOut, format, tags + BEATFIX_RENDER)
    validateExport(file) and atomicRename(file)
```

Improvements over the brief's draft pipeline, and why:
- **Analysis is persisted and correction is a pure function.** Target, strictness and mode changes never re-run detection. That makes them instant and deterministic.
- **Tempo hypotheses come before beat tracking.** The decoder is constrained to one metrical level, so mid-song octave switches (the catastrophic failure) are impossible.
- **Strict mapping of raw beats is replaced by an optimised mapping (Natural↔Strict).** Strict mapping injects detector noise into the output.
- **"Classify regions → choose stretch method" is deferred.** One linked-stereo engine is the MVP. Per-region switching needs evidence (HYBRID_AUTO_ENGINE).
- **Validation re-detects beats on the output (Path R)** in addition to checking known event positions (Path K). This separates detector errors from engine errors.

## 3. Answers to brief §26

1. **BPM estimator:** a Huber-IRLS linear fit of refined beat times vs beat index (BPM = 60/slope), with Theil–Sen as a cross-check, a block-bootstrap CI, and the 5–95 % local-tempo range for drifting songs. Mean IOI telescopes to its endpoints, and median IOI snaps to the frame lattice. → [BPM_ESTIMATION.md](BPM_ESTIMATION.md)
2. **Beat detector:** Beat This! activations (MIT, state-of-the-art accuracy on full mixes) + an owned time-varying DP decoder. The DSP SuperFlux tracker is the fallback and cross-check. ML is used where it helps (activation salience, octave), and deterministic code where reliability and continuity matter. → [BEAT_TRACKING.md](BEAT_TRACKING.md)
3. **Local tempo drift:** the Whittaker IRLS curve on refined anchors (fixed N_c = 8 for display). Drift is separated from detector noise by IOI lag-1 autocorrelation (noise ≈ −0.5, drift > 0), the variance ratio, and the residual trend. → [TEMPO_STABILITY_ANALYSIS.md](TEMPO_STABILITY_ANALYSIS.md)
4. **False/missing beats:** anchors come from the decoder, never from raw onsets. IOI ratio tests against the local period insert or remove beats. Inserted beats are flagged with low weight. Huber weights down-weight remaining outliers. → [OUTLIER_REJECTION.md](OUTLIER_REJECTION.md)
5. **Target grid:** `T_k = φ + k·60/B` in float64. φ comes from the phase policy (Preserve start by default, DAW-ready optional). Beats map one-to-one to slots. → [TARGET_BEAT_GRID.md](TARGET_BEAT_GRID.md)
6. **Warp ratios:** `r_i = P / (x_{i+1} − x_i)` on the *smoothed* anchors (output/input). Library conventions are converted only at the adapter. → [WARP_FUNCTION.md](WARP_FUNCTION.md) §1
7. **Warp function:** piecewise linear (exact inverse, tiny ratio steps after smoothing). PCHIP only if steps prove audible. → WARP_FUNCTION §2
8. **Monotonicity:** guaranteed by construction (strictly increasing anchors, P > 0) and enforced by `validate()` (minimum spacing, ratio bounds [0.5, 2]) before every render. → WARP_FUNCTION §3
9. **Stretch algorithms supported:** MVP has Smooth (phase-vocoder family, Signalsmith) and Resample (varispeed, constant ratio only). Transient (HPSS hybrid) is Phase 1.5. Slice and Texture are future. → [TIME_STRETCH_ENGINE.md](TIME_STRETCH_ENGINE.md)
10. **Default:** Smooth. It is the only mode that handles per-beat ratio changes on full mixes without pitch change, and it is MIT-licensed.
11. **How Auto chooses:** it doesn't in the MVP. The preferred future design is soft per-bin harmonic/percussive classification inside one engine, not region switching. If regions are ever used, switching happens only at bar boundaries with hysteresis and crossfades, validated by listening. → [HYBRID_AUTO_ENGINE.md](HYBRID_AUTO_ENGINE.md)
12. **Transients:** cuts and ratio steps happen at beats, where the PV re-anchors. Chunk cuts sit in low-RMS spots. Phase 1.5 adds the HPSS hybrid (percussive part via short OLA). The patent-flagged "remove-and-reinsert transient" approach is avoided pending legal review. → [TRANSIENT_PRESERVATION.md](TRANSIENT_PRESERVATION.md)
13. **Vocals:** pitch-preserving PV with phase locking (Signalsmith). Small ratios (typically ≤ 3 %). No varispeed on per-beat ratios. Pitch deviation is measured (≤ 5 cents median) and checked by listening.
14. **Stereo image:** linked stereo: one time map, and phase derived from the dominant channel with the inter-channel phase difference preserved. Independent L/R processing is never used. → [STEREO_PHASE_PRESERVATION.md](STEREO_PHASE_PRESERVATION.md)
15. **Phase problems:** phase locking in the engine, linked stereo, fresh engine state per chunk with pre-roll, overlap crossfades of near-identical material, and absolute sample accounting so nothing drifts.
16. **Large corrections:** ≥ 8 % per segment warns, ≥ 12 % flags a probable beat error, ≥ 15 % global warns, and outside [0.5, 2] the render is refused with the half/double fix offered. Natural mode spreads corrections over N_c beats. → DECISION_SUMMARY §6
17. **Efficient rendering:** deterministic chunk-parallel rendering on N−1 cores, content-addressed per-chunk cache, chunks nearest the playhead first, and an instant waveform estimate via W. → [RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md)
18. **Verifying the correction worked:** Path K (known source events mapped through W, measured on the output: tests the engine) and Path R (re-detect beats on the output vs the grid: end-to-end), plus residual drift, pitch, stereo, peaks and listening tests. → [QUALITY_VALIDATION.md](QUALITY_VALIDATION.md), [METRICS.md](../07_quality/METRICS.md)
