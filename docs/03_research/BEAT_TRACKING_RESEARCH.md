# Beat Tracking, Onset Detection & Downbeat Estimation — Research (Agent B)

Date: 2026-10-08 · Author: Research Agent B · Status: research input, not a decision record
Companion: `TEMPO_ESTIMATION_RESEARCH.md` (global/local tempo, octave resolution, drift-vs-noise statistics).
Extends (does not repeat) `_baseline_research_log.md`.

## Objective

Determine how BeatFix should turn a full stereo mix (often a Suno/AI-generated MP3/WAV) into a **dense, continuous, millisecond-accurate sequence of beat and downbeat anchors with per-beat confidence**, suitable for building a monotonic warp map. Choose between DSP, ML, and hybrid approaches; specify post-processing, anomaly detection, confidence scoring, determinism, and a DSP fallback.

## Why It Matters

The warp map is only as good as its anchors. Failure costs are asymmetric:

| Error type | Effect on BeatFix render | Severity |
|---|---|---|
| Consistent octave error (all beats at 2x or 0.5x) | Harmless for warping if the target grid uses the same level; only the displayed BPM is wrong. | Low (UI fix: "x2 / /2") |
| Consistent off-beat phase (pi) | Harmless for warping; downbeat/bar labels wrong. | Low |
| **Continuity error** (level or phase switches mid-song) | Local stretch ratio ~2x or ~0.5x at the switch → audible lurch. | **Critical** |
| Missing / extra beat | One IBI doubled/halved → local ratio spike 2x/0.5x. | **Critical** |
| Per-beat jitter (5–20 ms) | Micro-wobble in warp ratio (r_i noise ~2–4%), smeared transients, "seasick" feel. | High |
| Wrong beat in a fill / ghost note | Local warp bend in the fill. | Medium–High |

Standard MIR metrics (F1 at ±70 ms, CMLt, AMLt) are **too lax for BeatFix**: ±70 ms is 14% of a beat at 120 BPM. BeatFix needs continuity (CMLt-like) plus ~5 ms anchor jitter. This shapes every recommendation below.

## Key Findings

| # | Claim | Evidence / source | Conf. | Implication | Recommendation |
|---|---|---|---|---|---|
| F1 | Beat This! (Foscarin, Schlüter, Widmer, ISMIR 2024) is the most suitable ML tracker: MIT code **and** weights, beats+downbeats, no DBN required. | Repo README: "The code and the published model weights are released under the MIT license." https://github.com/CPJKU/beat_this ; arXiv 2407.21658 | High | Commercially usable activation model. Repo adds: users must verify training-data compatibility → **legal review**. | Primary activation source. |
| F2 | Beat This input: 22.05 kHz mono, STFT n_fft 1024, hop 441 (50 fps), 128 mel bands 30 Hz–10 kHz, `ln(1+1000x)`. Output: frame-wise beat & downbeat logits at 50 fps. ~20 M params (final, ~80 MB fp32) / ~2 M (small, ~8–10 MB). | arXiv 2407.21658 HTML; HF musetric/beat-this-onnx config | High | 20 ms frame quantization: minimal post-proc returns frame times → uniform ±10 ms error (sd ≈ 5.8 ms) before any detector error. | **Sub-frame refinement is mandatory** (parabolic peak interpolation + optional transient snap). |
| F3 | Beat This minimal post-processing: max-pool ±3 frames (±70 ms), keep prob > 0.5, merge adjacent equal peaks to centre, snap each downbeat to nearest beat. Optional madmom DBN (`DBNDownBeatTrackingProcessor`, beats_per_bar [3,4], 55–215 BPM, fps 50, transition_lambda 100). | `beat_this/model/postprocessor.py` (GitHub raw) | High | Minimal post-proc has no continuity model → non-periodic beats possible. DBN path pulls in madmom (CC BY-NC-SA models irrelevant here, but adds dependency and range limits). | Use Beat This activations, **own** deterministic decoder (below), never madmom at runtime. |
| F4 | Beat This SOTA F1 but weaker continuity: GTZAN (unseen) beat F1 89.1 / CMLt 79.8 / AMLt 89.8 vs Hung et al. 88.7 / 81.2 / 92.0. Adding DBN: F1 88.1 (beats), downbeat 77.4; DBN "increases CMLt downbeat ... but reduces F1". Authors: "for complex or underrepresented pieces, our network introduces non-periodic beats". | arXiv 2407.21658 Tables 1–2 & discussion | High | Continuity (our critical failure) is exactly where Beat This is weakest. | Add continuity-enforcing decoding + anomaly repair on top of activations. |
| F5 | DSP gets periodicity mostly right but not the metrical level: on GTZAN tempo, Percival & Tzanetakis (pure DSP) Acc1 0.658 / Acc2 0.924 vs Böck & Davies 2020 TCN 0.830 / 0.950. | Böck & Davies, ISMIR 2020, Table 1, http://archives.ismir.net/ismir2020/paper/000223.pdf | High | ML's main win is octave/metrical level + robustness on weak-onset material; DSP is a credible cross-check. | Hybrid: ML activation, DSP cross-check & refinement. |
| F6 | Source separation helps only slightly: All-In-One ablation on Harmonix — beat F1 0.958 → 0.951 and downbeat 0.915 → 0.898 without demixing. Requires Demucs + NATTEN + madmom DBN. | Kim & Nam, WASPAA 2023, arXiv 2307.16425, Fig. 3 & §3.3 | High | Demixing a full mix is costly for ≤1–2 F1 points. | Not in MVP. If the user has a Suno **drum stem**, analyse it instead (cheap, likely bigger win; UNVERIFIED magnitude). |
| F7 | SOTA DNN trackers fail on expressive music with "confident-but-wrong" activations; DBN 55 BPM floor forces double tempo on 21% of SMC tracks; authors recommend multi-hypothesis tempo. | Ahn, Hwang, Jung, arXiv 2605.12287 (May 2026) | Med-High | Activation height alone is not a reliable confidence. | Confidence must combine activation, periodicity consistency, and independent-tracker agreement. Wide tempo range (≥ 40–250 BPM). |
| F8 | ONNX exports of Beat This exist and match PyTorch: max logit diff ~1e-5, "Beat times are bit-identical" on 20 tracks (musetric, opset 17, 513-frame windows, 6-frame border); ashudesai export opset 14, max abs diff < 2e-5. Rust crate `beat-this` 1.1.0 (MIT, rten or ort) reports F1 = 1.0 vs Python for final model, bit-identical at 22.05 kHz; 4:32 track in 4.6 s on Apple M4. | HF model cards musetric/beat-this-onnx, ashudesai/songbird-models; docs.rs/crate/beat-this | Med (third-party ports, young) | CPU ONNX inference is practical; ports are unofficial. | Export our own ONNX from the official checkpoint in CI, verify against PyTorch on a fixed corpus, pin hash. |
| F9 | Official chunking: chunk 1500 frames (30 s), border 6 frames, overlap mode "keep_first". | `beat_this/inference.py` | High | Must replicate exactly in host code to reproduce reference outputs. | Implement split/aggregate identically; regression-test vs Python. |
| F10 | Floating-point results of ORT on CPU are not guaranteed identical across thread counts / ISAs; ORT has no full determinism guarantee. | Web search summary of ORT issues (Med); general FP non-associativity (High) | Med | Re-analysis on another PC could flip a near-threshold peak. | Persist activations + model hash + preprocessing version in the project; analysis is "computed once, stored". Fix intra-op threads to 1 for reference/golden runs. |
| F11 | Suno officially states tempo "is not consistent over time, just like live music, which characteristically drifts around a little bit in speed"; Suno Studio offers "Manual BPM" for a consistent tempo. | help.suno.com/en/articles/8363457 | High (that Suno says it) | Drift is a recognised property of Suno output, not only user lore. | Treat drift as expected; size the local tempo model for it. |
| F12 | Magnitudes/locations of Suno drift (e.g., "by measure 32 the drums are completely off", piano example 206→214 BPM over 3 min) are **anecdotal / vendor examples**; no peer-reviewed measurement found. | jgbeatslab.com blog; neuralanalog.com FAQ; PyPI `aislice` 0.1.0 (2026-10-07) | Low | Design targets must come from our own measurements. | Build a Suno corpus with manual beat annotations (see Open Questions). **UNVERIFIED** |
| F13 | Bar-pointer DBN default meter/tempo assumptions (3 or 4 beats/bar, 55–215 BPM) are "optimised on pop, rock and dance". | arXiv 2407.21658 §DBN discussion; madmom `DBNBeatTrackingProcessor` defaults MIN_BPM 55, MAX_BPM 215, TRANSITION_LAMBDA 100, OBSERVATION_LAMBDA 16 (Context7 /cpjku/madmom) | High | Good prior for Suno pop; dangerous for slow ballads/halftime. | Own decoder with 40–250 BPM and soft octave handling. |
| F14 | Newer research (2025–26): masked-diffusion beat tracking for coherent grids (Foscarin, Korzeniowski, Vogl, arXiv 2608.04624); AlignBeat latent-variable model (+3.6 downbeat CMLt over Beat This, arXiv 2510.14391); BeatFM/HingeNet foundation-model fine-tunes (arXiv 2508.09790/09788). | arXiv abstracts | Med (no code/licence verified) | Continuity is the active research frontier; none verified as shippable. | Watch-list; keep activation model swappable behind an interface. |

## Technical Details

### 1. Onset detection (DSP layer)

All operate on a magnitude STFT X(n,k) (frame n, bin k). Typical analysis for refinement: 44.1 kHz, window 2048, hop 441 (100 fps) for the tempo layer, hop 128 (≈2.9 ms) for transient refinement.

**1.1 Spectral flux (SF)** — Bello et al. 2005 (IEEE TSAP tutorial); Dixon 2006 (DAFx "Onset detection revisited").
`SF(n) = Σ_k H( |X(n,k)| − |X(n−1,k)| )`, `H(x) = max(x,0)` (half-wave rectification).
Simple, strong on percussive material; fires on vibrato/tremolo and amplitude modulation.

**1.2 Log-filtered spectral flux (LogFiltSF)** — Böck, Krebs, Schedl, ISMIR 2012.
`Y(n,b) = log10(1 + γ · Σ_k F(b,k)|X(n,k)|)` with F a logarithmic (e.g., 12–24 bands/octave) triangular filterbank, γ ≈ 1–20; `ODF(n) = Σ_b H(Y(n,b) − Y(n−μ,b))`.
Log compression equalises loud/quiet sources; filterbank reduces bin-level noise. Reported best in its class in that study (Med confidence, not re-fetched).

**1.3 SuperFlux** — Böck & Widmer, DAFx 2013. LogFiltSF plus (a) frequency max-filter on the reference frame and (b) a lag μ > 1 frames derived from window overlap:
`ODF(n) = Σ_b H( Y(n,b) − max_{|j|≤w} Y(n−μ, b+j) )`, w = 1 (3-bin max, madmom `diff_max_bins=3`), μ from `diff_ratio = 0.5`.
madmom `SuperFluxProcessor`: STFT → 24-band unnormalised log filterbank → log → `SpectrogramDifference(diff_ratio=0.5, diff_max_bins=3, positive_diffs=True)`; canonical fps 200 (Context7 `/cpjku/madmom`). librosa equivalent: `onset_strength(S=power_to_db(mel), lag=2, max_size=3)` (Context7 `/websites/librosa_doc`).
**Why it matters for Suno:** vibrato-heavy AI vocals and pads produce false flux peaks; SuperFlux suppresses them. Recommended DSP ODF.

**1.4 Complex-domain** — Duxbury/Bello et al. 2003–2004. Predicts the next frame from magnitude and phase advance: `X̂(n,k) = |X(n−1,k)| e^{j(2φ(n−1,k) − φ(n−2,k))}`; `CD(n) = Σ_k |X(n,k) − X̂(n,k)|` (rectified variant counts only magnitude increases). Detects soft/tonal onsets (note changes without energy rise). Phase is fragile in dense mixes and after lossy coding. madmom `complex_flux` = SuperFlux weighted by a local group-delay tremolo mask. **Use:** optional feature for non-percussive intros; not primary.

**1.5 Multi-band onset strength and kick/snare emphasis** — Scheirer 1998 (6-band resonators), Klapuri, Eronen, Astola 2006 (IEEE TASLP, multi-band accent + HMM for tatum/tactus/measure), Goto 2001 (JNMR, kick/snare pattern templates for beat/bar).
Compute band-wise ODFs: low (≈30–150 Hz, kick), low-mid (150–500 Hz, body/toms), mid/high (1–5 kHz snare crack, claps), air (5–12 kHz hats). madmom has `MultiBandSpectrogramProcessor(crossover_frequencies=[270])`.
Pre-step: **HPSS** (median-filter harmonic/percussive separation, Fitzgerald 2010): percussive mask = medfilt over frequency dominates medfilt over time; costs ~2 median filters, deterministic, removes sustained pads/vocals from the ODF.
Uses in BeatFix: (a) transient refinement on the percussive low band (kick) and mid band (snare); (b) backbeat pattern score for half/double resolution (see TEMPO doc §4); (c) ghost-note suppression: ghost snares have low mid-band energy relative to backbeat hits → weight by band energy percentile.

**1.6 Adaptive thresholding / peak picking** — madmom `peak_picking`: a frame is an onset iff
`ODF(n) == max(ODF[n−pre_max .. n+post_max])` and `ODF(n) ≥ mean(ODF[n−pre_avg .. n+post_avg]) + δ`, then events closer than `combine` (default 30 ms) are merged (Context7 `/cpjku/madmom`). SuperFlux paper settings (Med confidence, from memory of the paper): pre_max 30 ms, post_max 30 ms, pre_avg 100 ms, post_avg 70 ms, combine 30 ms, δ ≈ 1.1 on log ODF. Offline we may use symmetric windows (non-causal).
For BeatFix we **do not need an onset list** for the main path (beat decoding works on continuous activations); onset peaks are used only for refinement and cross-checking.

### 2. Beat tracking families

**2.1 Ellis 2007 dynamic programming** (JNMR 36(1), "Beat tracking by dynamic programming"; librosa `beat_track`).
Objective over beat times {t_i}: `C = Σ_i O(t_i) + α Σ_i F(t_i − t_{i−1}, τ)`, `F(Δ, τ) = −(log(Δ/τ))²`, α = "tightness" (librosa default 100), τ = global target period from a tempo estimate with a log-normal prior around 120 BPM.
Recursion: `S(t) = O(t) + max_{t' ∈ [t−2τ, t−τ/2]} ( S(t') + α F(t − t', τ) )`, backtrack from the best end frame. O(N·τ) time, exact, deterministic.
Weakness: single global τ (fails on drift); variant with time-varying τ(t) fixes this (used by `aislice`, per its PyPI description "dynamic-programming beat tracker whose expected period follows the song's drift").

**2.2 Davies & Plumbley 2007** (IEEE TASLP 15(3), "Context-dependent beat tracking of musical audio"). Two-state model: a *general state* (no context; comb-filter bank on the ODF autocorrelation weighted by a Rayleigh prior peaking near 120 BPM finds period, then phase by comb alignment) and a *context-dependent state* (constrains period/phase to the previous estimate with Gaussian weighting); switches back to general state on a detected tempo change. Processes 6 s windows hop 1.5 s. Relevant idea: **explicit change detection between "keep tempo" and "re-search tempo"** — maps directly to our drift-vs-step handling. (Med confidence on details; paper not re-fetched.)

**2.3 Bar-pointer HMM / DBN** (Whiteley, Cemgil, Godsill ISMIR 2006; Krebs, Böck, Widmer ISMIR 2015 "An efficient state-space model for joint tempo and meter tracking"; madmom).
Hidden state = (position-in-beat/bar φ, tempo τ in frames/beat). Position advances by 1 per frame; tempo changes only at beat boundaries with transition probability `p(τ_j | τ_i) ∝ exp(−λ |τ_j/τ_i − 1|)` (λ = transition_lambda, madmom default 100). Observation: activation a(t) if φ in the "beat" region (first 1/λ_obs of the beat, λ_obs = 16), else (1 − a(t))/(λ_obs − 1). Viterbi gives the jointly optimal tempo-phase path. Krebs 2015 reduces states by making the number of position states per tempo equal to its period (≈ thousands of states rather than ~10^5).
Strengths: strong continuity, models tempo drift naturally, handles missing activations. Weaknesses: hard tempo range & meter set, can lock to the wrong level confidently, reduces F1 on expressive music (F4, F7). Licence: madmom code BSD-2, model files CC BY-NC-SA 4.0 (madmom LICENSE). A "DBN is patented" claim appears only on a third-party HF card; no patent found with Böck as inventor (Google search, Low confidence) → **flag for legal review**; implementing the published model from papers in our own code is the low-dependency route.

**2.4 Beat This! transformer** (arXiv 2407.21658). Conv front-end + transformer blocks alternating attention over frequency and time; shift-tolerant loss (max-pool 7 frames ≈ ±70 ms) so the net isn't penalised for annotation jitter — *which also means it isn't trained for <20 ms precision*. Trained on 18 datasets / 4,556 tracks (incl. Harmonix, Ballroom, Beatles, RWC, GuitarSet, ASAP, Groove MIDI, Candombe). Outputs independent beat and downbeat logits; downbeats snapped to beats in post-proc. Checkpoints: final0/1/2 (~78 MB), small0/1/2 (~8 MB), fold0–7. Small model GTZAN beat F1 88.8 vs 89.1 (ablation table; Med confidence on split).

**2.5 BeatNet** (Heydari et al., ISMIR 2021; repo mjhydri/BeatNet). CRNN + particle filter (online) or DBN (offline); 22.05 kHz; licence CC BY 4.0 (repo). Depends on madmom and librosa; madmom 0.16.1 compatibility issues with Python ≥3.10 / NumPy ≥1.24 (repo). Designed for causal/streaming → not needed offline; weaker than offline SOTA. **Not recommended.**

**2.6 All-In-One** (Kim & Nam, WASPAA 2023; repo mir-aidj/all-in-one, MIT per repo sidebar; weights licence not stated → **UNVERIFIED, legal review**). Inputs Demucs (htdemucs) 4-stem spectrograms; ~300 K params; dilated neighbourhood attention (NATTEN; Windows requires building NATTEN from source); madmom DBN post-processing; outputs beats, downbeats, tempo, segment boundaries + labels (intro/verse/chorus...). Harmonix-only training. Default = 8-fold ensemble. Runtime claim: 10 songs (33 min) in 73 s on high-end hardware (GPU presumably). Valuable idea for later: **section boundaries** to explain intentional tempo changes and suspected Suno stitch points. Not MVP (dependency weight, Demucs cost, Windows build friction).

**2.7 TCN trackers** (Davies & Böck 2019 EUSIPCO; Böck, Davies, Knees ISMIR 2019 multi-task tempo+beat; Böck & Davies ISMIR 2020). Dilated TCN on log-filtered spectrogram at 100 fps with heads for beat, downbeat and tempo (0–300 BPM classes); DBN post-proc; tempo augmentation. Small (~tens–hundreds of K params). madmom ships TCN-style processors but with CC BY-NC-SA model files → not commercially usable without permission (contact JKU, per LICENSE). Architecture is a good template if we ever train our own small model.

### 3. Benchmark numbers (current SOTA, with caveats)

Beat / downbeat F1 (×100), ±70 ms tolerance. Ballroom/Hainsworth/SMC/Harmonix/Beatles are 8-fold cross-validation (in-distribution, differing training sets per paper); GTZAN is held-out for Beat This and Böck 2020. **Not strictly comparable across rows.**

| System | Ballroom | Hainsworth | SMC | Harmonix | Beatles | GTZAN (unseen) beat F1 / CMLt / AMLt | GTZAN downbeat F1 |
|---|---|---|---|---|---|---|---|
| Beat This (final, minimal PP) | 97.5 / 95.3 | 91.9 / 80.0 | 62.7 / – | 95.8 / 90.7 | 94.5 / 88.8 | 89.1 / 79.8 / 89.8 | 78.3 |
| Beat This + DBN | – | – | – | – | – | 88.1 / – / – | 77.4 |
| Hung et al. 2022 (SpecTNT-TCN, as reported in Beat This) | 96.2 / 93.7 | 87.7 / 74.8 | 60.5 / – | 95.3 / 90.8 | 94.3 / 87.0 | 88.7 / 81.2 / 92.0 | 75.6 |
| Böck & Davies 2020 TCN (joint) | 96.2 (CMLt 94.7) / 91.6 | 90.2 / 72.2 | 54.4 (beat-only 55.2) | – | – / 83.7 | 88.5 / 81.3 / 93.1 | 67.2 |
| All-In-One 2023 (demixed) | – | – | – | 95.8 / 91.5 | – | – | – |
| All-In-One w/o demix | – | – | – | 95.1 / 89.8 | – | – | – |

Other Beat This per-dataset: RWC Pop 96.1 / 93.7, RWC Classical 77.1 / 66.3.
Sources: arXiv 2407.21658 Tables 1–2; ISMIR 2020 paper 000223 Tables 2–3; arXiv 2307.16425 Fig. 3.
**Reading for BeatFix:** Suno output is overwhelmingly pop/electronic/rock-like, i.e., Harmonix/Ballroom/RWC-Pop-like, where F1 is 95–97. The residual risk is concentrated in: ballads/rubato, sparse intros/outros, breakdowns, halftime sections, and AI artefacts. SMC-like material (~55–63) is where every tracker fails.

### 4. Downbeat estimation

- ML downbeat F1 on pop-like data ~88–95 (table above); on GTZAN 78. Errors are dominated by bar-phase (off-by-one/two beat) and meter (3 vs 4) confusions.
- Beat This snaps downbeats to the nearest beat and has no bar-length constraint → can emit irregular bars. AlignBeat (2025) addresses this structurally (not shipped).
- **Recommendation:** derive bar phase by voting: for meter M ∈ {3,4} (optionally 2, 6), and phase p ∈ [0, M), score `Σ_i a_down(t_i) · [ (i − p) mod M == 0 ]` over sliding 16-bar windows; choose (M, p) per window with a Viterbi over windows that penalises meter/phase changes (cost ~ a few nats). This yields a regular bar grid with explicit, flagged exceptions (2-beat bars at Suno stitch points are plausible — UNVERIFIED).
- For warping, downbeats matter for (a) UI bar numbers, (b) choosing grid phase φ so bar 1 lands on a target bar line, (c) "bar-level" anchoring mode (warp only downbeats = more natural groove preservation). They do not change the per-beat warp if all beats are anchors.

### 5. Timing precision (the BeatFix-specific gap)

- Benchmarks reward ±70 ms; BeatFix needs anchor jitter σ ≲ 5 ms (target; derivation in TEMPO doc §5: per-beat ratio noise ≈ √2·σ/IBI; σ = 5 ms at 120 BPM → 1.4% ratio noise before smoothing).
- Error budget at 50 fps without refinement: quantization sd 5.8 ms + network localisation error (shift-tolerant loss does not train sub-70 ms precision; magnitude UNVERIFIED) + annotation bias.
- Refinement chain (deterministic):
  1. **Parabolic sub-frame interpolation** on the beat logit/probability around the picked frame k: `δ = 0.5 (a[k−1] − a[k+1]) / (a[k−1] − 2a[k] + a[k+1])`, clamp |δ| ≤ 0.5; `t = (k + δ)/50`.
  2. **Transient snap (optional, percussive material only):** in ±25 ms around t, compute high-resolution (hop 128 @ 44.1 kHz ≈ 2.9 ms) percussive SuperFlux on low+mid bands; if a peak exceeds the local adaptive threshold **and** moving to it reduces the residual against the smoothed tempo curve, move the anchor to that peak minus a global median offset (estimated over all snapped beats, so a constant ML-vs-onset bias is not introduced). A constant offset across all beats is harmless to warping; only variance matters.
  3. Never snap across a stronger competing onset within ±10 ms (flams) — keep ML time and lower confidence.

### 6. Missing / false beats (ghost notes, fills, toms, syncopation, non-percussive onsets)

| Situation | ML behaviour (expected) | DSP behaviour | BeatFix handling |
|---|---|---|---|
| Ghost notes / hi-hat 16ths | Mostly ignored (learned metrical salience) | ODF peaks everywhere | Decode on ML activation, not on onsets; DSP only refines near decoded beats. |
| Drum fills / tom runs | Activation may weaken or add off-grid peaks | Dense onsets, periodicity smeared | Decoder transition penalty keeps period; fills marked low-confidence; anchors in fills get reduced weight in the smoother. |
| Syncopation / anticipations (push on the "and") | Usually correct | Strong onsets off-beat → phase errors | Rely on ML + long-context decoder. |
| Missing kick (breakdown) | Activation drops | No onsets | Decoder interpolates; anchors flagged "inferred"; warp in region follows smoothed tempo. |
| Non-percussive intro (pads, vocals) | Low/ambiguous activation | Complex-domain helps slightly | Extrapolate tempo backward from first confident region; mark inferred; UI asks user to confirm first downbeat. |
| Claps/snare doubles (flams) | Single peak | Double onset | Combine window 30 ms; snap rule 5.3. |

Repair rules applied after decoding (IBI = inter-beat interval, P_loc = local smoothed period):
- `IBI_i / P_loc ∈ [1.7, 2.3]` → missing beat: insert at the max of activation in the middle third of the gap (fallback: midpoint), confidence = low.
- Two consecutive `IBI / P_loc ∈ [0.35, 0.65]` summing to ≈ 1 → extra beat: drop the one with lower activation.
- Sustained (≥ 8 beats) `IBI / P_loc ≈ 2` or `≈ 0.5` → metrical level switch → re-decode with forced level unless a genuine halftime section is confirmed (both periodicity at new level and backbeat pattern support it) → flag to user.

### 7. Confidence scoring

Per-beat features (all deterministic, computed after decoding):
1. `a_i` — beat activation peak (sigmoid prob).
2. `s_i` — peak salience: a_i minus the median activation in ±½ IBI (rejects flat-high regions).
3. `agree_i` — independent DSP tracker (SuperFlux + time-varying-τ DP) has a beat within ±35 ms (binary or Gaussian-weighted).
4. `res_i` — |residual| of t_i against the robust smoothed beat curve, in units of the jitter scale σ̂ (MAD-based).
5. `ioi_i` — IBI consistency: |log(IBI_i / P_loc)|.
6. `snap_i` — whether transient refinement found a clean percussive onset.

Per-beat score (simple, explainable; calibrate weights later on annotated Suno data):
`c_i = σ( w0 + w1·a_i + w2·s_i + w3·agree_i − w4·min(res_i,5) − w5·ioi_i + w6·snap_i )`.
Start with hand weights; fit logistic regression on our annotated corpus (label = beat within ±15 ms of annotation). This is the only "ML" beyond the activation model and it is a 7-parameter linear model.

Region/global scores:
- **Tracker agreement** F-measure between Beat This-decoded beats and the DSP tracker at ±50 ms (and at AMLt-style levels). Mean-mutual-agreement between multiple trackers is known to predict difficulty (Holzapfel, Davies, Zapata, Oliveira, Gouyon, IEEE TASLP 2012, "Selective sampling for beat tracking evaluation"; Zapata, Davies, Gómez, IEEE/ACM TASLP 2014 "Multi-feature beat tracking") — Med confidence (not re-fetched this pass).
- Optional seed ensemble: final0/1/2 agreement (3× inference; benefit UNVERIFIED).
- Decoder path score normalised per beat (log-probability per beat) and its margin vs the best path at the alternative metrical level.
- Fraction of beats with c_i > 0.7; longest low-confidence run.
- Map to UI traffic light: Green (agreement ≥ 0.9, no unresolved anomalies), Amber, Red (agreement < 0.6 or level ambiguity margin small) → Red triggers "please confirm BPM / first downbeat".

### 8. Accuracy on full mixes vs stems

- Published systems are trained and evaluated on full mixes; All-In-One gains only +0.7 beat / +1.7 downbeat F1 from demixing on Harmonix (F6).
- For BeatFix the gain from a drum stem is mostly **timing precision** (clean transients for refinement), not detection. Suno Studio exports stems (help.suno.com "Exporting from Studio"), so an optional "analysis source: drum stem" input is cheap to support: analyse stem, warp the full mix with the same map. Magnitude of precision gain: UNVERIFIED → measure.
- Running Demucs inside BeatFix: deferred (model size, CPU minutes per song — UNVERIFIED; licence of htdemucs weights → legal review).

### 9. AI-generated music (Suno) specifics

Verified:
- Suno acknowledges non-constant tempo in generated audio and provides a Manual BPM lock in Studio (F11).
- A competing open-source tool (`aislice` 0.1.0, MIT, released 2026-10-07) targets exactly this use-case with DP beat tracking + "fix missed and extra beats, ignore weak beats in fade-ins and fade-outs, smooth out tracking jitter while keeping real drift" + Rubber Band R3 time map (PyPI). Confirms the problem framing; no accuracy metrics published.

Anecdotal / UNVERIFIED:
- Progressive drift over 16–32 bars; drift larger in "older genre" prompts (disco, jazz) per Suno help; 206→214 BPM (~3.9%) over 3 min in one vendor example.
- Stitching of "Extend"/continuation segments creating tempo steps or phase jumps at seams.

Mechanism hypotheses (all UNVERIFIED for Suno, whose architecture is not public):
- H1 learned human-performance tempo variation from training data (consistent with Suno's own wording).
- H2 compounding error in autoregressive generation: models that must infer tempo/phase from their own past drift; shown for causal accompaniment models in "Silent Metronome" (Bretz, Soydaner, Plaat, arXiv 2609.07688, 2026), where an explicit metrical conditioning signal improved beat alignment 3.2× over the causal baseline.
- H3 window/segment boundaries of latent-diffusion or codec-token generation with no global grid → tempo/phase discontinuities at seams.
- H4 neural codec/vocoder decoding softens transients → lower onset contrast → noisier DSP refinement.
- H5 MP3 delivery: encoder delay/priming and decoder differences cause constant offsets ("MP3 files can have slight offset differences" — All-In-One README). Constant offsets are harmless to warping but must be **identical between analysis and render decode paths**.

Implications: the system must model (a) smooth drift of a few %, (b) occasional tempo steps, (c) occasional phase jumps that a smooth warp cannot fix (needs a flagged local strong stretch or user decision), (d) weak transients.

### 10. Reproducibility & determinism

- Pin: model checkpoint hash, ONNX opset/export script version, resampler (Beat This reference uses soxr), downmix (arithmetic mean L/R per musetric card), STFT/mel parameters, chunking (1500/6 keep_first).
- ORT CPU: set `intra_op_num_threads = 1` for golden tests; allow multithread in production but persist outputs. ORT offers a deterministic-compute session option but full determinism across ISAs is not guaranteed (F10).
- Post-processing must be numerically robust to ~1e-5 activation perturbations: thresholds applied with hysteresis, ties broken by index, DP/Viterbi in float64 with deterministic argmax (lowest index on ties).
- Persist per-analysis: activations (2 × 50 fps float16 ≈ 2 × 12k × 2 B ≈ 48 KB per 4 min), decoded beats, confidences, parameters, versions. Re-render never re-analyses unless the user asks.

### 11. Should ML be used at all?

Yes, narrowly: **one ML model (Beat This, MIT) produces frame activations; everything after is deterministic code we own.**

| Where ML helps | Where ML hurts | Mitigation |
|---|---|---|
| Metrical level & octave choice (Acc1 .83 vs .66 DSP on GTZAN tempo) | Confident-but-wrong activations (F7) | DSP agreement + periodicity checks in confidence |
| Beats in weak-onset material (vocals, pads, piano) | Non-periodic outputs, lower CMLt (F4) | Own continuity decoder + repair |
| Downbeats (DSP has no competitive method) | 80 MB asset, ~seconds of CPU | Small model option; run once, cache |
| Ignoring ghost notes / fills | Opaque failure, training-data licence uncertainty | Fallback DSP path; legal review |
| | Cross-machine FP differences | Persist activations |

Inference cost (estimate): Beat This final ≈ 20 M params at 50 fps; Rust port reports 4.6 s for 4:32 on Apple M4; typical x86 desktop CPU estimated 3–15 s per 4-min song (**UNVERIFIED**, benchmark in Phase 1). Small model: roughly an order of magnitude fewer parameters (expected ~5–10× faster, UNVERIFIED). Memory: model 80 MB + activations negligible; Rust port reports ~620 MiB peak for 60 min at 48 kHz streaming.

### 12. Recommended pipeline

```
ANALYZE(audio_source):
  # 0. Decode once, identical path to renderer (gapless, same decoder, same offset handling)
  x_stereo, sr = decode(audio_source)                     # float32
  x_mono = (L + R) / 2

  # 1. ML activations (primary)
  x22 = resample_soxr(x_mono, sr -> 22050)                # pinned resampler
  M = logmel(x22, n_fft=1024, hop=441, mels=128, fmin=30, fmax=10000, f=ln(1+1000x))
  a_beat, a_down = onnx_beat_this(M, chunk=1500, border=6, mode="keep_first")  # logits @50fps
  p_beat, p_down = sigmoid(a_beat), sigmoid(a_down)

  # 2. DSP features (independent; also fallback)
  P = hpss_percussive(STFT(x_mono @ sr, win=2048, hop=sr/100))
  odf      = superflux(P, bands=24/oct, mu=from diff_ratio 0.5, maxbins=3)     # 100 fps
  odf_low  = superflux(P, band=30..150 Hz);  odf_mid = superflux(P, band=1..5 kHz)
  odf_hi_res = superflux(P_hop128, bands=low+mid)                               # ~2.9 ms, for refinement

  # 3. Tempo hypotheses & local period path (see TEMPO doc)
  cands = tempo_candidates(p_beat, odf)                   # multi-hypothesis, 40..250 BPM
  level = rank_metrical_level(cands, p_down, odf_low, odf_mid, prior)          # octave choice + margin
  tau_path = viterbi_local_period(tempogram(p_beat), level, max_step=|log ratio| small)

  # 4. Decode beats with continuity (own code; Ellis DP with time-varying tau)
  B = dp_decode(obs = p_beat, tau = tau_path, tightness = alpha)               # deterministic
  B_dsp = dp_decode(obs = normalize(odf), tau = tau_path_dsp, tightness = alpha)  # independent cross-check
  B_min = beat_this_minimal_pp(p_beat)                     # reference/agreement only

  # 5. Repair & refine
  B = repair_missing_extra(B, p_beat, local_period(B))
  B = subframe_parabolic(B, p_beat)
  B = transient_snap(B, odf_hi_res, smooth_curve(B), max_shift=25ms)

  # 6. Downbeats / bars
  bars = decode_meter_phase(B, p_down, meters={3,4}, change_penalty)

  # 7. Smooth tempo curve, anomalies (see TEMPO doc §5-6)
  curve, residuals = robust_smooth(B, weights=conf_prelim, lambda=user_mode)
  anomalies = detect(changepoints(curve), phase_jumps(residuals), level_switches(B), low_conf_runs)

  # 8. Confidence
  conf_beat = logistic(features(B, p_beat, B_dsp, residuals, snaps))
  conf_global = {agree: F(B, B_dsp, 50ms), margin: level.margin, frac_hi: mean(conf_beat > .7)}

  persist(activations, B, bars, curve, conf, anomalies, versions)
  return AnalysisResult

FALLBACK (model missing / ONNX error / user forces DSP):
  skip step 1; obs = normalize(odf_low*w1 + odf_mid*w2 + odf*w3); downbeats from backbeat pattern only;
  mark result "DSP mode", cap global confidence at Amber.
```

DP decoder detail (time-varying Ellis, our implementation):
```
dp_decode(obs[0..N), tau[0..N), alpha):
  S = copy(obs); back = -1 * ones(N)
  for t in 0..N:
     lo = t - round(2.0*tau[t]); hi = t - round(0.5*tau[t])
     best = -inf
     for u in max(0,lo)..hi:                      # ascending → ties keep lowest u
        v = S[u] - alpha * (ln((t-u)/tau[t]))^2
        if v > best: best = v; arg = u
     if best > 0: S[t] = obs[t] + best; back[t] = arg   # only chain if it improves (Ellis)
  end = argmax over last ~tau frames of S[t] where obs[t] is a local max
  return backtrack(end, back)
```
Complexity ~ N · 1.5τ ≈ 12,000 × 40 = 5×10^5 ops for 4 min @ 50 fps: negligible.

### 13. Role of the pure-DSP fallback

1. **Availability:** model file missing/corrupt, ONNX runtime failure, or user preference → still produce a usable map (expected weaker on octave and weak-onset material).
2. **Cross-check:** independent beat sequence for agreement-based confidence (§7).
3. **Precision:** high-resolution transient refinement (§5) — ML can't do this at 50 fps.
4. **Kick/snare evidence:** band ODFs feed half/double resolution and downbeat sanity (TEMPO doc §4).
5. **Testing:** deterministic baseline for regression tests and synthetic-drift benchmarks.

## Alternatives Considered

| Option | Verdict | Reason |
|---|---|---|
| Pure DSP (SuperFlux + DP/HMM) | Fallback only | Octave/level & weak-onset robustness clearly worse (F5); no competitive downbeats. |
| Beat This + madmom DBN | Rejected for runtime | Adds madmom dependency (Python, NumPy pinning), fixed 55–215 BPM & 3/4 meters, reduces F1; patent claim unresolved (legal review). |
| Beat This + minimal PP only | Rejected as final output | No continuity guarantee; frame-quantized; used only as agreement reference. |
| Beat This + own continuity decoder + DSP refinement | **Recommended** | MIT model, deterministic owned post-processing, addresses continuity and precision. |
| All-In-One | Deferred | Demucs + NATTEN + madmom, Harmonix-only, weights licence unverified; segment labels attractive for v2. |
| BeatNet | Rejected | Online-oriented, CC BY 4.0 (attribution OK), madmom/PyAudio dependency friction, not SOTA offline. |
| madmom RNN/TCN models | Rejected | CC BY-NC-SA model files. |
| Essentia (RhythmExtractor2013, TempoCNN) | Rejected | AGPLv3/commercial (baseline log). |
| Train our own small TCN | Later option | Needs licensed training data (Harmonix audio not distributed); only if Beat This licence/data review fails. |
| Masked-diffusion / AlignBeat / BeatFM | Watch-list | No verified code/licence; iterative inference cost. |

## Evidence / Sources

- Foscarin, Schlüter, Widmer, "Beat this! Accurate beat tracking without DBN postprocessing", ISMIR 2024 — https://arxiv.org/abs/2407.21658 (HTML v1 tables), code/licence https://github.com/CPJKU/beat_this, `beat_this/inference.py`, `beat_this/model/postprocessor.py`.
- ONNX ports — https://huggingface.co/musetric/beat-this-onnx ; https://huggingface.co/ashudesai/songbird-models ; Rust crate https://docs.rs/crate/beat-this/latest (v1.1.0, 2026-10-02).
- Böck & Davies, "Deconstruct, analyse, reconstruct: how to improve tempo, beat, and downbeat estimation", ISMIR 2020 — http://archives.ismir.net/ismir2020/paper/000223.pdf
- Kim & Nam, "All-In-One Metrical and Functional Structure Analysis with Neighborhood Attentions on Demixed Audio", WASPAA 2023 — https://arxiv.org/abs/2307.16425 ; https://github.com/mir-aidj/all-in-one
- Ahn, Hwang, Jung, "The SMC Blind Spot", arXiv 2605.12287 (2026) — https://arxiv.org/abs/2605.12287
- BeatNet — https://github.com/mjhydri/BeatNet (CC BY 4.0)
- madmom licence — https://github.com/CPJKU/madmom/blob/main/LICENSE ; API via Context7 `/cpjku/madmom` (SuperFluxProcessor, peak_picking, DBNBeatTrackingProcessor defaults, TempoEstimationProcessor, CombFilterbankProcessor, MultiBandSpectrogramProcessor).
- librosa API via Context7 `/websites/librosa_doc` (onset_strength lag/max_size SuperFlux example, beat_track, tempogram, fourier_tempogram, plp with priors).
- Suno help "Fixing Tempo Drift" — https://help.suno.com/en/articles/8363457 ; "Exporting from Studio" — https://help.suno.com/en/articles/8128193
- Anecdotal: https://www.jgbeatslab.com/ai-music-lab-blog/suno-studio-fix-tempo-drift-daw-sync ; https://neuralanalog.com/faq/why-does-a-suno-song-speed-up-or-slow-down ; https://pypi.org/project/aislice/
- Bretz, Soydaner, Plaat, "Silent Metronome", arXiv 2609.07688 (2026).
- Recent: arXiv 2608.04624 (masked diffusion), 2510.14391 (AlignBeat), 2508.09790 (BeatFM), 2508.09788 (HingeNet), 2510.25560.
- Cited from established literature, not re-fetched in this pass (Med confidence on exact parameters): Ellis 2007 JNMR; Davies & Plumbley 2007 IEEE TASLP; Whiteley, Cemgil, Godsill ISMIR 2006; Krebs, Böck, Widmer ISMIR 2015; Böck, Krebs, Schedl ISMIR 2012; Böck & Widmer DAFx 2013; Dixon DAFx 2006; Bello et al. IEEE TSAP 2005; Scheirer JASA 1998; Klapuri, Eronen, Astola IEEE TASLP 2006; Goto JNMR 2001; Fitzgerald DAFx 2010; Holzapfel et al. IEEE TASLP 2012; Zapata, Davies, Gómez IEEE/ACM TASLP 2014.

## Recommendation

1. **Activation:** Beat This `final0` via our own verified ONNX export, CPU ORT (or pure-Rust runtime if the stack is Rust — Agent C/H to decide). `small0` as a "fast analysis" option. (Confidence High.)
2. **Decoding:** own deterministic time-varying-τ DP decoder over Beat This activations, wide tempo range 40–250 BPM, plus repair rules; no madmom at runtime. (High.)
3. **Precision:** parabolic sub-frame interpolation always; transient snap on percussive material with global offset correction. Target anchor jitter ≤ 5 ms median on drum-heavy material (target to validate). (Med-High.)
4. **Downbeats:** Beat This downbeat activation + meter/phase Viterbi over windows; flag irregular bars. (Med.)
5. **Confidence:** feature-based logistic score; DSP-vs-ML agreement as the backbone of global confidence. (Med-High.)
6. **DSP fallback:** HPSS + SuperFlux + same DP decoder; always computed (cheap) for cross-check and refinement. (High.)
7. **Persist** activations and versions; analysis is never silently recomputed. (High.)
8. **Validation:** build (a) synthetic-drift corpus (known warp applied to grid-locked electronic tracks) for ms-level metrics and (b) a manually annotated Suno corpus (≥ 50 songs) before committing thresholds. (High.)

## Risks

- Beat This training-data licensing compatibility (repo explicitly warns) → **legal review** before shipping weights.
- Systematic ML timing bias vs perceived transient differs per instrument → snap offset correction must be global, not per-beat, or groove gets altered.
- Over-aggressive continuity decoding erases genuine intentional tempo changes or meter changes → soft penalties + changepoint detection + user override.
- Suno-specific failure modes unmeasured; thresholds from public datasets may not transfer.
- Third-party ONNX/Rust ports are young; rely on our own export + test.
- Unresolved "DBN patent" claim (single, low-quality source) → legal review if a bar-pointer HMM is implemented.

## Open Questions

1. Real distribution of Suno drift (magnitude, shape, steps, phase jumps) — needs an annotated corpus; which Suno versions (v4.5/v5/v5.5) differ?
2. Measured CPU time of Beat This final/small on target Windows hardware (and with pure-Rust runtime vs ORT).
3. Does a 3-seed activation average improve continuity enough to justify 3× cost?
4. Actual Beat This localisation error (ms) vs clean transients on electronic music — determines whether transient snapping is necessary on most material.
5. Does a Suno drum stem materially improve anchor precision vs full mix?
6. Legal: Beat This training-data implications; All-In-One / Demucs weight licences; DBN patent claim.

## Implementation Implications

- Interface `ActivationModel { fn run(mel) -> (beat_logits, down_logits) }` with Beat This ONNX and a "DSP pseudo-activation" implementation → swappable, testable.
- Analysis artefact schema: `{versions, sr_analysis, activations(f16), beats[{t, conf, flags[inferred|snapped|repaired]}], downbeats, meter_segments, tempo_curve, anomalies[], global_conf}` stored in the project.
- Decode/render must share one decoder path (gapless, identical MP3 priming handling).
- Golden tests: fixed-thread ORT inference on 10 reference clips, compare beats bit-exact; synthetic-drift test asserts recovered warp error < target.
- UI hooks required from this layer: per-beat confidence colouring, anomaly markers (missing/extra repaired, level switch, phase jump, tempo step), x2 / ÷2 / shift-phase / set-downbeat actions that re-run only steps 4–8 (cached activations make this instant).
