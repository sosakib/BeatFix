# DSP Fundamentals for BeatFix

Agent A (DSP / Time-Stretch). Date: 2026-10-08. Companion to `TIME_STRETCH_RESEARCH.md` (engine choice, modes, warp-driven rendering). This file covers the DSP basics the whole app depends on: STFT parameters, windows, onset-related spectral features, resampling vs TSM, artifact mechanisms, and measurable stereo/phase/transient/pitch integrity checks.

---

## Objective

Give one shared set of DSP definitions, default parameters and integrity measures for analysis (beats/onsets/classification), rendering (TSM, resampling) and QA, so every module uses the same conventions and the test suite can check them objectively where that is possible.

## Why It Matters

- Analysis and rendering use STFTs with different goals: time resolution for onsets, frequency resolution for tonal PV. Using one parameter set for both is a common mistake.
- Resampling (pitch changes with speed) and TSM (pitch kept) are different operations. Mixing them up, or resampling more than once, costs quality.
- "Audio quality" for TSM has no reliable objective metric (Roberts & Paliwal 2020). We need narrow, mechanism-specific measures (alignment, IPD, attack time, pitch), plus listening tests.

---

## Key Findings

| # | Claim | Evidence | Conf. | Implication |
|---|---|---|---|---|
| 1 | The time-frequency trade-off is fixed per window: N = 4096 at 44.1 kHz gives 10.8 Hz bins but 93 ms frames; N = 1024 gives 43 Hz bins and 23 ms frames. PV-TSM normally uses N ≈ 100 ms for frequency resolution; onset detection uses ~23–46 ms frames at ~100 frames/s. | Driedger & Müller 2016 §5.6 (N ≈ 100 ms for PV); standard onset literature (Bello et al. 2005, Böck & Widmer 2013 — not re-fetched in this pass) | High | Use separate STFT configurations for analysis and render (table TD-1). |
| 2 | With a Hann window, overlap-add is constant (COLA) at hop N/2 and N/4. For analysis+synthesis windowing (WOLA) you normalise by Σ w²(n − mHs), or use sqrt-Hann at 50 % overlap. | Driedger & Müller §2–3 (Hann with Hs = N/2 gives a constant denominator) | High | All in-house STFT code must use the WOLA normalisation and be unit-tested for perfect reconstruction (< −120 dB error at r = 1). |
| 3 | Phasiness and transient smearing both come from losing **vertical** phase coherence (relations between bins in a frame). Phase jumps/warble come from losing **horizontal** coherence (continuity of a bin over time). | Driedger & Müller §5.5; Laroche & Dolson 1999 | High | Map each artifact to its mechanism and measure (TD-5). |
| 4 | Objective TSM quality measures predict listening scores poorly; a retrained neural measure gets close to inter-session agreement. Fuzzy tonal/transient/noise energy curves show smearing and loss of tonality. | Roberts & Paliwal JASA 2020 (arXiv 2006.00848); Fierro & Välimäki (Aalto) | High | Use objective metrics as **regression guards**. Release decisions need listening tests. |
| 5 | Permissive resamplers exist: libsamplerate (BSD-2-Clause), r8brain-free-src (MIT), rubato (MIT OR Apache-2.0, v5.0.1). | GitHub licence API / Cargo.toml, fetched 2026-10-08 | High | Use one for "Resample" mode and for any export sample-rate conversion. Variable-ratio support: libsamplerate allows changing `src_ratio` between calls, and rubato has async resamplers with adjustable ratio (both **Med**: from documentation knowledge, not re-fetched; verify through Context7 when implementing). |
| 6 | Sample positions held as f32 lose integer precision past 2^24 samples (≈ 6.3 min at 44.1 kHz, 5.8 min at 48 kHz). | IEEE-754 single precision has a 24-bit significand | High | Keep all time and position maths in f64 or i64. Audio data can stay f32. |
| 7 | Time-stretching changes waveform peak structure, so the true peak can exceed the source's true peak on limited masters. | Mechanism: PV re-synthesis changes relative phases and therefore crest factor. Magnitude not measured here (UNVERIFIED) | Med | Measure true peak (BS.1770 oversampled) at export and warn above −1 dBTP. Do not auto-limit by default. |

Verified vs assumed: items 1–2 and 4–6 come from the sources given. The numbers in TD-1 are design defaults derived from them, not measured optima. Item 7's magnitude is an assumption to measure.

---

## Technical Details

### TD-1 STFT parameter sets (defaults, 44.1 kHz; scale N and hop with Fs and keep the durations)

| Use | Window | N (ms) | Hop (ms) | FFT size | Bin Δf | Rationale |
|---|---|---|---|---|---|---|
| Onset / spectral flux (analysis) | Hann | 2048 (46.4) | 441 (10, i.e. 100 fps) | 2048 | 21.5 Hz | ~10 ms onset precision, enough resolution to separate kick/bass from snare. |
| Fine onset refinement (local) | Hann | 512 (11.6) | 64–128 (1.5–2.9) | 512 | 86 Hz | Re-localise detected onsets to ±1–2 ms for anchoring and Slice. |
| Beat-tracker input | as the model requires (Beat This: 22.05 kHz mono spectrogram, per baseline) | — | — | — | — | Resample for analysis only, never for render. |
| HPSS masks (Transient mode) | Hann | 2048 (46.4) | 512 (11.6) | 2048 | 21.5 Hz | Medians over ~17 frames (≈200 ms) and ~17 bins (≈370 Hz). Tune by listening. |
| PV render (Smooth, generic) | Hann / Kaiser | 4096 (92.9) | 1024 (23.2), Hs = N/4 | 4096–8192 | 10.8 Hz | Resolves bass partials; Driedger: N ≈ 100 ms. |
| Signalsmith `presetDefault` | Kaiser (in library) | 5292 (120) | 1323 (30) | library-chosen | — | Verified in source. |
| PGHI reference setup | Hann (≈4096) | 92.9 | Hs 1024, M = 8192 | 8192 | 5.4 Hz | Průša & Holighaus test setup. |
| Percussive OLA | Hann | 512 (11.6) | 256 (5.8) | — | — | Short window, so transient doubling is inaudible (Driedger §6). |
| Spectral display (UI) | Hann | 2048–4096 | 256–512 | — | — | Visual only. |

At 48 kHz keep the durations: for example, onset N = 2048 (42.7 ms) with hop 480 (100 fps), and PV N = 4096 (85 ms).

Zero-padding (FFT > N) interpolates the spectrum but does not add resolution. It helps peak picking for phase locking.

### TD-2 Windows

| Window | Highest sidelobe | Main-lobe null-to-null width | COLA hops | Use here |
|---|---|---|---|---|
| Rectangular | −13 dB | 2 bins | any | Never for analysis. |
| Hann | −31.5 dB | 4 bins | N/2, N/4 | Default for analysis, PV and OLA. |
| sqrt-Hann (analysis and synthesis) | — | — | N/2 (Σw² = 1) | Simple WOLA for in-house STFT processors. |
| Hamming | −43 dB | 4 bins | N/2 (constant offset) | Not needed. |
| Blackman | −58 dB | 6 bins | N/3 | Peak picking when sidelobes matter. |
| 4-term Blackman-Harris | −92 dB | 8 bins | N/4 (approx.) | Measurement and test analysis (pitch/THD checks). |
| Kaiser (β) | tunable | tunable | depends on β | Signalsmith; windowed-sinc resampler design. |
| Gaussian | none (theoretically) | — | — | PGHI theory assumes a Gaussian. Hann is used with a fitted λ constant: Průša's PGHI work gives λ ≈ 0.25645·N² for Hann (**Med**, from memory of the PGHI papers; verify against the paper before implementing). |

Sidelobe figures are standard textbook values (Harris 1978), not re-fetched in this pass.

### TD-3 Onset-related spectral features (for beat anchoring, transient classification and Auto features)

With `Y(m,k) = log(1 + γ|X(m,k)|)` (log compression; γ ≈ 1–100):

- **Spectral flux (rectified):** `SF(m) = Σ_k H(Y(m,k) − Y(m−1,k))`, where `H(x) = max(x,0)`. This is the robust default for mixed music.
- **SuperFlux** (Böck & Widmer, DAFx-13): compare against a frequency max-filtered earlier frame, `Y_max(m−μ,k) = max_{|j|≤1} Y(m−μ, k+j)`, with lag μ ≥ 1. Suppresses vibrato and tremolo false onsets, which matters for vocals and pads.
- **High-frequency content:** `HFC(m) = Σ_k k·|X(m,k)|²`. Sensitive to hi-hats and snares and good for "percussiveness". Biased on bright masters.
- **Complex domain** (Bello/Duxbury): `CD(m) = Σ_k |X(m,k) − X̂(m,k)|`, where X̂ is predicted from the previous magnitude and linearly extrapolated phase. Catches soft tonal onsets.
- **Phase deviation:** the second difference of phase, weighted by magnitude. Detects tonal note changes but is noisy in dense mixes.
- **Energy envelope derivative:** cheap but weak on dense masters.
- **Per-band flux** (for example sub < 120 Hz, low-mid, high > 5 kHz) separates kick, snare and hats. This is useful for half/double-time disambiguation (Agent B) and for the Auto percussive score.
- **HPSS percussive ratio:** `P(m) = Σ|X_p|² / Σ|X|²` from median-filter soft masks (Fitzgerald 2010). Doubles as the Transient-mode input.
- **Tonalness / spectral flatness:** flatness `= exp(mean log|X|²) / mean |X|²`. Low means tonal. This is the input for fuzzy tonal/noise classes (Damskägg & Välimäki 2017).

Peak picking: adaptive threshold (moving median + δ), minimum inter-onset gap ≈ 20–30 ms, local-max window ±30 ms. Then refine each onset to ±1–2 ms with the short STFT or envelope (TD-1). Onset times feed anchoring. Beats are **not** onsets (Agent B handles beat selection).

### TD-4 Resampling vs time-scale modification

| | Resampling ("Resample" mode, varispeed) | TSM (Smooth, Transient, Slice) |
|---|---|---|
| Operation | Read input at rate ρ(t) = dW⁻¹/dt with band-limited interpolation | Change duration while keeping spectral content |
| Pitch | Scales by 1/r (pitch rises when compressing). Cents = 1200·log2(1/r) | Preserved (≤ 1 cent target) |
| Formants and timbre | Shifted with pitch | Preserved |
| Artifacts | None beyond interpolation error (aliasing/imaging at −100 to −120 dB with a good design) | Phasiness, smear, warble, seams |
| Stereo | Exact (same operation on every channel) | Needs linked processing |
| Variable ratio | Gives pitch modulation (vibrato-like) if the ratio changes per beat. Only acceptable for constant-ratio sections or tiny nudges | Native |
| Cost | Very low | Moderate |

Numbers: 1 % tempo ≈ 17.2 cents and 2 % ≈ 34.3 cents. 108→110 BPM is +31.8 cents (see the TIME_STRETCH worked example).

Resampler quality target for render and export SRC: passband ripple < 0.01 dB up to 20 kHz, stopband attenuation ≥ 100 dB, transition band 20–22.05 kHz at 44.1 kHz. In practice this means Kaiser windowed-sinc or polyphase (libsamplerate "best", r8brain, rubato sinc).

Rules:
1. **Render at the source's native sample rate.** Allow at most one SRC stage, at export, and only if the user picks a different rate.
2. Analysis may downsample (22.05 kHz mono for the beat model), but analysis audio is never used for render.
3. TSM engines also use interpolation internally (fractional analysis positions). Keep their input at native rate.
4. A separate pitch-shift feature (not in scope) would be TSM plus resampling. Don't build it now (YAGNI).

### TD-5 Artifact mechanisms → measurable proxies

| Artifact | Mechanism | Where it shows | Proxy measure |
|---|---|---|---|
| Phasiness ("reverberant", "loss of presence") | Vertical phase coherence lost; partials around a peak are no longer phase-related | PV without locking; vocals, piano | Fuzzy tonal/noise energy ratio change (Fierro & Välimäki); listening |
| Transient smearing / pre-echo | Overlapping frames carry the attack at dispersed positions after phase propagation; smear extends up to N | Kick, snare, piano attacks; most audible after silence | Attack-time ratio, pre-echo energy, flux-peak ratio (TD-6) |
| Transient doubling / skipping | Time-domain frame repetition or omission within the tolerance window (WSOLA/OLA with a long N) | WSOLA on drums | Onset count, input vs output (aligned by W) |
| Warble / phase jumps | Horizontal coherence lost (OLA, poor WSOLA alignment on polyphony) | Pads, bass in time-domain methods | Partial-frequency jitter of sinusoidal test tones |
| Stereo smear / widening / mono loss | Channels processed independently, so IPD drifts | Unlinked PV, M/S independent processing | IPD/ICC deviation, mono fold-down loss (TD-7) |
| Seams / clicks | Engine switch or splice with decorrelated phases or misaligned timing | Mixed-mode renders, Passthrough | HF energy spike, short-term loudness dip/bump at seams |
| Pitch error | Wrong mode (Resample), wrong IF estimate, sample-rate mismatch | — | f0 / peak-frequency deviation in cents |
| Timing error | Engine latency not compensated; rounding drift | All engines | Impulse/onset position error vs W |
| Inter-sample overs | Crest-factor change after re-synthesis | Limited masters | True-peak delta |

### TD-6 Transient, timing and pitch integrity measures (automated)

For each reference onset `s_j` in the source (refined to ±1 ms), let `t_j = W(s_j)` be its expected output time.

- **Timing error:** `e_j = argmax_τ xcorr(env_out, env_in_shifted)` near `t_j`, or the detected output onset minus `t_j`. Targets: |mean e| < 0.5 ms and max |e| < 2 ms on synthetic click trains. On music, report the median and 95th percentile.
- **Attack-time ratio:** 10→90 % rise time of the Hilbert or RMS envelope (1 ms windows), output vs input. Target ≤ 1.2× for the Transient mode at |1−r| ≤ 5 %.
- **Pre-echo energy:** energy in [t_j − 50 ms, t_j − 2 ms] relative to the same window in the input, in dB. Most important for sparse intros.
- **Flux-peak ratio:** peak SuperFlux at the onset, output/input (sharpness proxy).
- **Pitch:** on test tones and sweeps, the peak-frequency estimate with a 4-term Blackman-Harris window and quadratic interpolation. Target |Δ| < 1 cent for TSM modes. For Resample, the exact expected cents ±0.5.
- **Duration exactness:** output length = round(W(L_src)) ±1 sample.
- **Null at r = 1:** for a constant r = 1 render, the residual should be < −60 dB re signal for PV engines (UNVERIFIED that Signalsmith reaches this; measure). Passthrough must be bit-exact.
- **Determinism:** re-rendering the same project produces a bit-identical (or hash-identical) file.

### TD-7 Stereo and phase integrity measures

Computed per frame over bands (for example ERB or octave bands), output vs input, with input frames mapped by W:

- **ICC** (inter-channel correlation): `ρ = Σ L·R / sqrt(ΣL² ΣR²)` at zero lag, per band. Report |Δρ| (target < 0.05 in mid and high bands for linked engines).
- **IPD** (inter-channel phase difference): `∠(X_L · X_R*)` per bin, magnitude-weighted circular mean error vs input. Target < 5° below 1.5 kHz, where IPD is perceptually relevant (target UNVERIFIED; calibrate).
- **ILD** (inter-channel level difference): `20 log10(|X_L|/|X_R|)` per band. Target |Δ| < 0.5 dB.
- **Inter-channel delay (ITD):** GCC-PHAT lag on Haas-panned test material. Must be preserved within ±1 sample.
- **Side/mid energy ratio:** `E_S/E_M`, with S = (L−R)/2 and M = (L+R)/2. A rise means artificial widening (unlinked processing).
- **Mono fold-down loss:** integrated loudness (BS.1770) of (L+R)/2, output minus input, vs the stereo loudness delta. Mono should not lose more than 0.3 LU relative to the stereo change.
- Test signals: hard-panned tones (L only, R only), centred noise, decorrelated noise, a 0.5 ms Haas-delayed click train, an M/S-encoded sweep, and a real wide master.

### TD-8 Loudness, peaks and numeric hygiene

- Internal format: f32 planar audio, f64 time. Enable flush-to-zero/denormals-are-zero in render threads.
- Loudness: integrated LUFS (BS.1770-4/5) of input vs output. For |1−r| ≤ 5 % a TSM render should be within ±0.3 LU (UNVERIFIED target; measure on the corpus).
- True peak: 4x-oversampled peak per BS.1770 at export. Warn if > −1 dBTP or if it is more than 0.5 dB above the source true peak.
- Export: dither (TPDF) only when reducing word length to 16-bit. No dither for 24-bit or float. Encode MP3 once, from the float render, never from another lossy file.
- Engine latency and offsets are compensated inside the engine wrapper and verified by the impulse test, never by the UI.

---

## Alternatives Considered

| Topic | Alternatives | Pick | Why |
|---|---|---|---|
| Analysis STFT | One shared STFT vs per-purpose STFTs | Per-purpose (TD-1) | Onsets need time resolution; PV needs frequency resolution. |
| Window | Hann vs Blackman-Harris vs Kaiser | Hann default; BH4 for measurement | COLA convenience vs sidelobe rejection. |
| Onset function | Energy, HFC, flux, SuperFlux, complex | SuperFlux plus per-band flux | Robust on mixes, suppresses vibrato. HFC kept as a percussive feature. |
| Quality evaluation | Objective only (PEAQ, OMOQ) vs listening | Mechanism-specific metrics plus listening tests | PEAQ needs a same-length, time-aligned reference, which TSM output lacks. Objective TSM measures correlate poorly (Roberts & Paliwal). |
| Resampler | libsamplerate / r8brain / rubato / soxr (LGPL) | rubato (if Rust) or r8brain/libsamplerate (if C++) | Permissive licences; soxr is LGPL, so avoid it unless there is a need. |

## Evidence / Sources

- Driedger & Müller 2016, Appl. Sci. 6(2):57: https://www.mdpi.com/2076-3417/6/2/57
- TSM Toolbox (DAFx-14, GPL): https://dafx14.fau.de/papers/dafx14_jonathan_driedger_tsm_toolbox_matlab_imple.pdf
- Průša & Holighaus, "Phase Vocoder Done Right", arXiv 2202.07382: https://ar5iv.arxiv.org/html/2202.07382
- Roberts & Paliwal, JASA 148(1) 2020: https://arxiv.org/abs/2006.00848
- Damskägg & Välimäki 2017: https://www.mdpi.com/2076-3417/7/12/1293 ; Fierro & Välimäki: https://arxiv.org/abs/2210.14041
- Fitzgerald, "Harmonic/percussive separation using median filtering", DAFx-10 (cited via Driedger & Müller ref. 23)
- Bello et al., "A Tutorial on Onset Detection in Music Signals", IEEE TSAP 2005; Böck & Widmer, "Maximum Filter Vibrato Suppression for Onset Detection" (SuperFlux), DAFx-13 (standard references, not re-fetched this pass)
- Harris, F.J., "On the Use of Windows for Harmonic Analysis with the DFT", Proc. IEEE 1978 (window sidelobe values)
- ITU-R BS.1770 (loudness and true peak)
- Licences fetched via the GitHub API on 2026-10-08: libsamplerate BSD-2-Clause; r8brain-free-src MIT; rubato MIT OR Apache-2.0 (Cargo.toml v5.0.1); PyTSMod GPL-3.0 (reference only)

## Recommendation

1. Adopt the per-purpose STFT table (TD-1) as the project default, with sample-rate-independent durations. (High)
2. Onset pipeline: log-compressed SuperFlux plus three-band flux, adaptive peak picking, then short-STFT refinement to ±1–2 ms. (Med-High; Agent B owns final beat logic)
3. Render at native rate; at most one export SRC; Resample mode through a permissive windowed-sinc resampler. (High)
4. All time maths in f64/i64. (High)
5. Build the automated integrity suite (TD-6/TD-7/TD-8) before choosing between engines, and use Rubber Band R3 output as a reference point. Gate releases on the suite **plus** a listening test. (High)

## Risks

- Metric thresholds (IPD 5°, ±0.3 LU, < −60 dB null) are initial guesses. If they are miscalibrated, tests will be either flaky or meaningless. Calibrate on the corpus first.
- Onset refinement on dense masters can lock onto hi-hats instead of the kick. Use per-band flux and keep the beat logic separate.
- If the analysis sample rate differs from the render rate, the conversion must stay in f64 seconds. A wrong factor gives a constant drift.

## Open Questions

1. Final HPSS median lengths and mask exponent for AI masters.
2. Which perceptual thresholds for IPD/ICC change are audible on wide AI masters? Needs a small listening calibration.
3. Is a PEAQ-style comparison usable if the reference is the input warped by a "perfect" engine (for example Rubber Band R3)? That would be a reference-relative metric, not an absolute one.
4. Is the Roberts-Paliwal objective measure (OMOQ) code available under a usable licence? Not checked; for licensing review.

## Implementation Implications

- Shared `dsp` module: STFT/ISTFT with WOLA normalisation (tested for perfect reconstruction), windows, SuperFlux, band flux, HPSS soft masks, onset refinement, true-peak and loudness meters, and a stereo metrics module (ICC/IPD/ILD/GCC-PHAT).
- Test-signal generator: click trains at known positions, sines and sweeps, panned and Haas signals, decorrelated noise, a synthetic drum loop, a piano note, and silence-then-attack.
- The QA harness renders each engine and mode at r ∈ {0.90, 0.95, 0.98, 1.00, 1.02, 1.05, 1.10} plus a random piecewise warp, and emits a JSON report of TD-6/TD-7/TD-8 metrics for CI regression tracking.
