# Time-Stretch (TSM) Research for Beat-Anchored Warping of Full Mixes

Agent A (DSP / Time-Stretch). Date: 2026-10-08. Extends `_baseline_research_log.md` (library licences/prices not repeated except where re-verified).

---

## Objective

Choose and specify the time-scale-modification (TSM) architecture for BeatFix: offline, pitch-preserving, **variable-ratio** warping of full stereo mixes (often Suno/AI masters) onto a fixed-BPM grid via a monotonic warp map `t_target = W(t_source)`. Cover the algorithm families, their failure mechanisms, library APIs for time-varying ratios, per-source behaviour, whether an automatic per-region mode ("Auto") is justified, and a concrete engine architecture with mode names, segment crossfades and stereo-linking rules.

## Why It Matters

- The render engine is the product's audible output. Beat detection errors are fixable in UI; stretch artifacts (phasiness, transient smear, flams, stereo collapse, seams) are not.
- BeatFix's case is unusual: **small, continuously varying ratios** (typ. 0.95–1.05 locally, piecewise per beat) on **dense, already-lossy full mixes**. Most TSM literature optimises constant ratios 0.5–2.0 on clean sources. Several conclusions differ for our regime (see Key Findings 2, 3).
- Licensing of the engine is binding (baseline): MIT/MPL engines vs GPL/commercial Rubber Band.

---

## Key Findings

| # | Claim | Evidence | Conf. | Implication / Recommendation |
|---|---|---|---|---|
| 1 | Non-linear (warp-map) TSM is done by fixing the synthesis hop Hs, placing synthesis instants `s_m = m·Hs/Fs`, and computing analysis instants `a_m = τ⁻¹(s_m)` from the inverse warp. | Driedger & Müller 2016, §7.1, eqs. 34–35 | High | Every BeatFix engine is driven by `W⁻¹` sampled on a fixed output grid. Store/expose `W⁻¹` with f64 precision. |
| 2 | At ratio exactly 1 a standard PV with Ha = Hs is (numerically) the identity; any α ≠ 1 starts the loss of vertical phase coherence, which is what causes phasiness and transient smear — even at α = 0.98. | Derivation (§TD-3) + Driedger & Müller §5.5 | High | "Small ratio" ≠ "no artifacts". Use phase-locked / phase-gradient PV, not textbook PV. Pass-through unchanged regions where possible. |
| 3 | For modest ratios, good modern PVs (Signalsmith, Rubber Band R3, PGHI, élastique) are close; Signalsmith states its sweet spot as 0.75x–1.5x. PGHI was rated ≈ élastique Pro / IRCAM at α = 1.5 and better than élastique/Melodyne at α = 2. | Signalsmith README; Průša & Holighaus 2017 listening test (8 subjects, 7 excerpts) | Med | Engine choice for BeatFix should be decided by licence, API fit for variable ratio, stereo linking and transient behaviour on full mixes — then confirmed by our own listening test. |
| 4 | Signalsmith Stretch (MIT, tag 1.4.0, repo active 2026-09-25) supports time-varying ratio natively: the ratio is inferred per block from the `inputSamples/outputSamples` of each `process()` call; synthesis hop is fixed (`intervalSamples()`); the previous frame is re-analysed at exactly one synthesis interval in the past when the analysis hop changes. | Source `signalsmith-stretch.h` lines 280–312 (fetched 2026-10-08); Context7 `/signalsmith-audio/signalsmith-stretch` | High | Driving loop = one `process()` call per synthesis interval with `n_in = round(W⁻¹(t+Hs)) − round(W⁻¹(t))`. No API change needed. |
| 5 | Signalsmith links stereo per bin: for each bin it computes the phase for the max-energy channel, then sets every other channel's phase = reference output phase + the **input** inter-channel phase difference. | Source lines 725–800 ("All other bins are locked in phase") | High | Inter-channel phase difference (IPD) is preserved by construction → stable image and mono compatibility. Meets our stereo requirement without M/S tricks. |
| 6 | Signalsmith has no explicit transient detector; vertical coherence comes from predicting each bin's phase from neighbouring bins (short and long frequency steps) as well as time. Above stretch 2x it randomises per-bin time factors (`maxCleanStretch = 2`). | Source lines 509, 636–790 | High | Transient crispness on drums must be verified; a transient-preserving wrapper (HPSS hybrid) may be needed for "Transient" mode. |
| 7 | Bungee (MPL-2.0, tag v2.4.30, active 2026-08) is a grain-based PV (~100 grains/s) whose `Request.position` (double, input frame of grain centre) is set **per grain** by the caller; `speed` is used only when it cannot be inferred from successive positions; supports reverse/zero speed. | `bungee/Bungee.h` (fetched); README | High | Most direct mapping to `W⁻¹`: `position = W⁻¹(grain output centre)`. Stereo-linking behaviour of Basic edition is **UNVERIFIED** → test IPD. |
| 8 | Rubber Band offline supports pre-planned variable stretch via `setKeyFrameMap(std::map<size_t,size_t>)` (source frame → output frame); must be called after `setTimeRatio()` (which must equal total out/in), before first `process()`; not available in real-time mode. R3 (`OptionEngineFiner`) = multi-resolution, best on mixes/vocals/bass, much more CPU; `OptionChannelsTogether` improves centre clarity/mono compatibility; `OptionTransients*` have no effect in R3. | `RubberBandStretcher.h` (fetched, v4.0.0); Context7 `/breakfastquay/rubberband` | High | Excellent quality reference and potential premium engine, but GPL-2.0-or-later / paid (baseline: £590+). Use as QA reference; ship only after legal review. |
| 9 | HPS-based hybrid TSM (PV with identity phase locking on harmonic part + very short-window OLA on percussive part) preserves transients **without explicit transient detection**; OLA's transient doubling is too fast to hear with a short window. | Driedger & Müller 2016 §6; Driedger, Müller, Ewert 2014 (IEEE SPL 21(1):105–109); TSM Toolbox example (OLA Hs=128, win 256; PV Hs=512, win 2048; median lengths 10/10) | High | Best documented, licence-clean concept for a "Transient" mode. Implement ourselves (Toolbox is GPL — reference only). |
| 10 | Fraunhofer patent family (e.g., US 9,230,558 B2, priority 2008-03-10, status active, expected expiry 2029-02-17) claims: remove a transient time portion, time-stretch the remainder, reinsert the unprocessed transient copy. Related US 9,236,062 / 9,275,652. | Google Patents US9230558B2 (fetched) | Med (claim read via summary) | **For legal review** before shipping any "cut-transient / stretch / reinsert" mode (our "Slice" and any explicit transient-copy variant). HPSS soft-mask hybrid and phase-reset differ in mechanism but should also be reviewed. |
| 11 | Röbel 2003 (DAFx) detects transients per bin and re-initialises phases at the transient position, without constraining the stretch ratio in transient segments. Rubber Band R2's `OptionTransientsCrisp` is a phase-reset scheme (R2 only). | DAFx archive / HAL hal-01161124; RB header | High | Phase reset at onsets is the alternative "Transient" implementation if we write our own PV. |
| 12 | Phase Vocoder Done Right (PGHI-based TSM; Průša & Holighaus, EUSIPCO 2017, arXiv 2202.07382) integrates the phase gradient in time **and** frequency via a max-heap ordered by magnitude; no peak picking, no transient detection; tol = 1e-6 relative; test setup 44.1 kHz, Hann ~4096, M = 8192, Hs = 1024. | ar5iv 2202.07382 | High | Strong candidate algorithm if we ever implement our own PV core (MIT/clean-room from paper). Reference implementations in LTFAT/phaseret are GPL — don't copy. |
| 13 | Objective TSM quality measures correlate poorly with listening scores; Roberts & Paliwal built a dataset (88 train sources × 6 methods × 10 scales + 20 test sources × 3 methods × 4 scales; 42,529 ratings; JASA 148(1):201–210, 2020). | arXiv 2006.00848; IEEE DataPort 1987 | High | Engine selection needs a listening test; objective metrics only as regression guards (see DSP_RESEARCH.md). |
| 14 | Fuzzy per-bin classification (tonal / noise / transient memberships controlling phase propagation per bin) works well on mixed material (singing, techno, jazz with vocals) without explicit decomposition. | Damskägg & Välimäki 2017, Appl. Sci. 7(12):1293 | Med | This is the principled form of "Auto": soft, per-bin, inside one engine — no region seams. Post-MVP research item. |

### Verified facts vs recommendations vs assumptions

- **Verified** (source read 2026-10-08): items 1, 4, 5, 6, 7, 8, 9 (Toolbox params), 11, 12, 13; licences: Signalsmith MIT, Bungee MPL-2.0, Rubber Band GPL-2.0(+)/commercial, TSM Toolbox GPL, PyTSMod GPL-3.0, libsamplerate BSD-2-Clause, r8brain-free-src MIT, rubato MIT OR Apache-2.0.
- **Recommendations**: see §Recommendation.
- **Assumptions** (unverified, to test): AI/Suno sources are typically 44.1/48 kHz, heavily limited, with pre-existing codec smear in highs; local warp ratios mostly within ±5 %; users prefer "unchanged regions untouched".
- **UNVERIFIED**: Bungee Basic stereo linking; Rubber Band keyframe hit accuracy (sample-exact or approximate); Signalsmith header `version` constant reads 1.3.2 while latest tag is 1.4.0 (constant may lag the tag).

---

## Technical Details

### TD-1 Notation

- `Fs` sample rate; `x` input; `N` frame (window) length; `Ha` analysis hop; `Hs` synthesis hop.
- Stretch factor (Driedger convention) `α = Hs / Ha` = output duration / input duration. α > 1 stretch (slower), α < 1 compress (faster).
- BeatFix local ratio between anchors `r_i = (T_{i+1} − T_i)/(S_{i+1} − S_i)` is the same quantity (α = r).
- Warp `W: source time → target time`, strictly increasing; renderers need `W⁻¹`.

### TD-2 Time-domain family

**OLA.** Frames at `a_m = m·Ha` windowed and overlap-added at `s_m = m·Hs`:
`y(t) = Σ_m w(t − m·Hs) · x(t − m·Hs + m·Ha) / Σ_m w(t − m·Hs)`.
No waveform alignment → phase jumps on periodic content (warble/flutter). Excellent for percussive/noise with very short N (~10 ms) — transient doubling at offset |Hs−Ha| is inaudible when N is tiny. Cost: trivial.

**SOLA** (Roucos & Wilgus 1985). Shift each *synthesis* frame position by the lag maximising cross-correlation with the existing output; output length becomes data dependent (needs correction).

**WSOLA** (Verhelst & Roelands 1993). Keep synthesis positions fixed; shift the *analysis* frame by `δ ∈ [−Δmax, Δmax]` to maximise similarity between the frame and the "natural continuation" of the previously used analysis frame:
`δ_m = argmax_δ Σ_n x̃_{m−1}(n) · x(n + m·Ha + δ)·w(n)` where `x̃_{m−1}` = segment following the previous frame by Hs.
Driedger & Müller guidance: N ≈ 50 ms, Δmax ≈ 25 ms (at least half the longest period). At 44.1 kHz: N = 2205, Δmax ≈ 1102. Artifacts: transient doubling/skipping (a transient within Δmax can be repeated or dropped), warble on polyphonic dense mixes where no single lag aligns all components, stereo requires one shared δ for all channels (compute similarity on mid or summed correlation). Transient-preserving variant: set Ha = Hs in a neighbourhood of detected transients (copy unmodified) and compensate the ratio elsewhere (Driedger & Müller §4.3 citing [13]). SoundTouch (LGPL-2.1) is WSOLA-like. Cost: correlation search O(N·Δmax) per frame unless FFT-based; cheap offline.

**TD-PSOLA** (Moulines & Charpentier 1990). Pitch-synchronous frames (2 periods long) centred on pitch marks; duplicate/drop periods to change duration. Requires a single dominant f0 → speech/monophonic only. **Unsuitable for full mixes**; at most a niche for isolated vocal stems. Not in scope.

**Granular.** Grains 20–100 ms, Hann, overlap 2–8x, grain read positions follow `W⁻¹` with optional jitter; phases not managed. Diffuse/"cloud" sound; acceptable for pads/ambience/reverb tails, unacceptable on rhythm/vocals at musical ratios. PaulStretch is an extreme-ratio relative (phase randomisation).

### TD-3 Phase vocoder (PV) family

STFT: `X(m,k) = Σ_n x(n + a_m) w(n) e^{−2πikn/N}`, bin centre `ω_k = 2πk/N` (rad/sample).

Instantaneous frequency (heterodyned phase difference over the analysis hop):
`Δφ(m,k) = princarg(φ(m,k) − φ(m−1,k) − ω_k·Ha)`
`ω̂(m,k) = ω_k + Δφ(m,k)/Ha`
(valid when |true freq − ω_k| < π/Ha, i.e. Ha ≤ N/4 with Hann for a sinusoid inside the main lobe).

Phase propagation over the synthesis hop:
`φ_s(m,k) = φ_s(m−1,k) + ω̂(m,k)·Hs`, `Y(m,k) = |X(m,k)| e^{iφ_s(m,k)}`, inverse FFT, window, overlap-add at Hs (WOLA normalisation).

**Why α ≈ 1 still degrades.** With Ha = Hs the propagated phase equals the analysis phase (`φ_s(m) = φ(m)` by induction), i.e. identity. For Ha ≠ Hs each bin's synthesis phase drifts from its analysis phase by `ω̂(m,k)(Hs − Ha)` per frame. For a stationary sinusoid that is harmless (it is a pure delay). For a transient or any non-stationary event the per-bin `ω̂` estimates are not those of a single sinusoid, so the per-bin drifts are no longer linear in frequency, the bins lose their mutual (vertical) phase alignment, and each of the ~N/Hs overlapping frames reconstructs the event at a different, dispersed position → smear over up to N (≈93 ms at N = 4096) and the reverberant "phasiness" colouration. Worked number at 108→110 BPM below: the drift is ≈1.19 rad per frame at 440 Hz — vertical coherence is gone within one or two frames.

**Identity phase locking** (Laroche & Dolson 1999, IEEE TSAP 7(3):323–332; precursor Puckette 1995). Pick magnitude peaks `k_p` per frame; propagate only peak phases; lock every other bin to its region's peak:
`φ_s(m,k) = φ_s(m,k_p) + φ(m,k) − φ(m,k_p)` for k in the region of influence of k_p.
Preserves the analysed relative phases around each peak → less phasiness, less smear. Cost ≈ PV + peak picking.

**Scaled phase locking** (same paper): additionally accounts for peaks moving between frames (peak k_p in frame m connected to the peak in frame m−1 whose region contained k_p) and allows scaling the locked phase offsets by a factor β (β = 1 ≡ identity locking); improves partials whose frequency changes across bins (vibrato, glides). For BeatFix: identity locking is sufficient baseline; scaled locking helps vocals.

**PGHI / "Phase Vocoder Done Right"** (Průša & Holighaus 2017). Estimate both phase derivatives from the analysis STFT:
time: `Δt φ(m,n) = ½[(1/a_a)·princarg(φ(m,n)−φ(m,n−1) − 2π m a_a/M) + 2πm/M] + (same, forward)`
freq: `Δf φ(m,n) = ½[(1/b_a)·princarg(φ(m,n)−φ(m−1,n)) + forward]`
Synthesis by heap integration: seed at the largest-magnitude coefficient, propagate to neighbours in time with trapezoid `φ_s(m,n) = φ_s(m,n−1) + (a_s/2)(Δtφ(m,n−1)+Δtφ(m,n))` and in frequency using `Δf φ`, always expanding the current largest-magnitude coefficient; coefficients below `tol·max|s|` (tol = 1e-6) get random phase. Effect: horizontal **and** vertical coherence without peak picking or transient detection. Listening test: at α = 1.5 ≈ élastique Pro and IRCAM; at α = 2 ≈ IRCAM, better than élastique Pro and Melodyne; classic PV worst. Cost: PV + heap ops O(K log K) per frame (K bins) — fine offline.

**Signalsmith Stretch (observed design).** Kaiser-windowed STFT, default block 120 ms, interval 30 ms (`presetDefault`), cheaper 100/40 ms. Phase of each output bin = time prediction (previous output × input phase advance measured over one synthesis interval, re-analysing the previous frame at exactly Hs lag when Ha changes) combined with vertical predictions from neighbour bins at ±1 and ±(FFT/interval) bins; energy-weighted. Conceptually close to phase-gradient integration. Stereo linked (Finding 5). Formant/pitch features irrelevant here (keep transpose = 1).

**Transient handling options in PV**
1. Phase reset at onsets (Bonada 2000; Duxbury et al. 2002 AES; Röbel 2003 per-bin): at a detected transient frame, set `φ_s = φ` (analysis phase) for transient bins, restoring vertical coherence exactly at the attack. Risk: reset discontinuities on sustained partials ("interruptions in stable sounds" — RB docs note this for R2 Crisp).
2. Hop locking near transients (Ha = Hs for a few frames, compensate elsewhere) — pure "copy the attack" in PV or WSOLA. Same family as the Fraunhofer claim → legal review.
3. HPSS hybrid (Driedger et al. 2014): `x = x_h + x_p` with soft masks, `y = PV_locked(x_h) + OLA_short(x_p)`, both following the same `W⁻¹`.
4. Multi-resolution: short windows for HF/transient bands, long windows for LF tonal (RB R3 "full multi-resolution processing scheme").
5. Per-bin fuzzy classification (Damskägg & Välimäki 2017).

**PVSOLA** (Moinet & Dutoit 2011 and similar): periodically copy an analysis frame unmodified into the output (WSOLA-aligned) to re-establish vertical coherence. Useful idea for our "pass-through" of unchanged regions.

### TD-4 Variable-ratio driving (all engines)

Generic driver (fixed synthesis grid, Driedger eq. 34–35):

```
// all positions in f64 samples; never f32 (2^24 samples = 6.3 min at 44.1 kHz)
Hs        = engine.synthesis_hop()
out_len   = round(W(src_len))
t_out     = 0
p_prev    = Winv(0.0)
while t_out < out_len:
    n_out  = min(Hs, out_len - t_out)
    p_next = Winv(t_out + n_out)
    n_in   = round(p_next) - round(p_prev)          // drift-free integer accounting
    engine.process(src_from(round(p_prev)), n_in, out_at(t_out), n_out)
    p_prev = p_next; t_out += n_out
```

- **Signalsmith**: use `process()` exactly as above (one interval per call); start with `outputSeek(input, outputSeekLength(rate0))` (pre-roll, rate0 = local `dW⁻¹/dt` at 0); end by feeding `inputLatency()` further samples (real source if available, else silence) then `flush()`. `exact()` is constant-ratio only — don't use for warps. Latency/offset alignment must be verified with an impulse-train test (see Implementation Implications).
- **Bungee**: per grain set `request.position = W⁻¹(t_grain_centre_out)`, `request.speed = (W⁻¹)'` at that point, `pitch = 1`, `reset = false`; loop `specifyGrain → analyseGrain → synthesiseGrain`; track output frames from `OutputChunk`. Exact mapping between output chunk frames and grain centres needs verification (UNVERIFIED detail).
- **Rubber Band R3 offline**: `setTimeRatio(out_len/src_len)`; `setKeyFrameMap({S_i → T_i})` at beat anchors (≈2/s at 110 BPM — a "reasonable gap"); `study()` all; `process()/retrieve()`. Engine interpolates between key frames (matches piecewise-linear W; precision of anchor hits UNVERIFIED).

Ratio steps at anchors (piecewise-linear W) are handled gracefully by PV-type engines (the hop simply changes) — no clicks. Large step changes (> ~5 %) between adjacent beats are audible as a tempo lurch, not as an artifact; smoothing belongs to the warp-map stage (Agent for warp math), not the engine.

### TD-5 Stereo coherence

| Method | Mechanism | IPD preserved? | Mono compat. | Notes |
|---|---|---|---|---|
| Independent L/R PV | separate phase propagation | No — drifts per bin | Poor ("phasy" mono) | RB `OptionChannelsApart` default; widens image unnaturally (RB docs). |
| M/S then independent PV | encode M=(L+R)/2, S=(L−R)/2, process, decode | Partially — M and S phases drift relative to each other → panned sources wander | Centre good | Cheap retrofit for engines without linking. |
| **Linked phase** | one phase trajectory per bin (from reference/max-energy channel or mid), other channels = ref + input IPD | **Yes, per frame** | Good | Signalsmith (verified); RB `OptionChannelsTogether` aims at the same goal. **Required for BeatFix.** |
| Time-domain methods (Resample, OLA, WSOLA with shared δ, Slice) | identical operations on all channels | Yes (exactly) | Exact | WSOLA must use one shared lag for both channels. |

For HPSS hybrid: compute masks once from a channel-shared magnitude (`max(|X_L|,|X_R|)` or `|X_M|`) and apply the same mask to L and R, so the harmonic/percussive split is identical across channels; otherwise a source can be "percussive" on L and "harmonic" on R and smear across the image.

### TD-6 Source-type behaviour (BeatFix regime: |1 − r| ≤ 5 %, piecewise)

| Material | Dominant risk | Smooth (linked PV) | Transient (HPSS hybrid) | Slice | Resample | Notes |
|---|---|---|---|---|---|---|
| Full mix (default case) | mild phasiness + kick/snare softening | Good | Good–better on drums; small risk of hollow attacks from mask leakage | Poor (sustained parts chopped) | Pitch shift ≈ 17 cents per 1 % | Smooth default; Transient if drums soften. |
| Electronic / 4-on-floor | kick attack smear, sub-bass phase | Good | Best | OK on drum-only intros | — | Sidechain pumping is time-shaped: fine with all methods. |
| Drum-heavy / drops | smear, flams | Fair | Best | Best on isolated drums | — | Transient detection errors in Slice cause flams. |
| Vocals | phasiness, "chorus", formant OK (pitch=1) | Best | Good (sibilants may be classified percussive; harmless) | Bad | Audible pitch | Scaled locking / R3 strongest. |
| Sustained pads / strings | warble in OLA/WSOLA | Best | Same as Smooth (all harmonic) | Bad | OK if constant | Granular acceptable but unnecessary at ±5 %. |
| Piano / plucked guitar | attack softening + sustain phasiness | Good | Best | Bad (sustain cut) | — | Attacks are mixed harmonic+percussive. |
| Bass / sub | LF needs long windows; phase-cancel with kick | Best with long N (≥ 90 ms) | Good if masks keep LF harmonic | — | — | Multi-resolution helps (R3). |
| Reverb tails / ambience | metallic/phasey tails, noise "tonalisation" | Fair–good | Same | Bad (tails truncated/doubled) | — | Noise components benefit from phase randomisation (fuzzy noise class). |
| Wide stereo / chorus / Haas | image collapse or smear if channels unlinked | Good *only if linked* | Good if shared masks | Exact | Exact | Measure IPD/ICC (DSP_RESEARCH.md). |
| Dense limited masters | inter-sample overs after re-synthesis; dense transients | Good | Good | — | Exact | True-peak can rise after PV → check/headroom. |
| Sparse intros (solo instrument, silence) | exposed artifacts, pre-echo into silence | Good | Good | OK | — | Silence: Signalsmith has passthrough below noise floor (1e-15). Pre-echo of smear into silence is most audible here. |
| AI/Suno-specific (assumption) | pre-existing codec smear/"swirl" in HF, unstable transients | Same as full mix | HPSS may mis-split codec noise | — | — | Already-smeared highs mask PV artifacts somewhat; test on corpus. |

### TD-7 Is Auto (per-region mode selection) justified?

Evidence for: no single TSM method is best for all material (Driedger & Müller abstract/§1); hybrids (HPSS, fuzzy per-bin) beat single methods on mixed music (Driedger 2014; Damskägg 2017).

Evidence against region-level switching for BeatFix:
1. Our ratios are small; quality spread between a good linked PV and a hybrid is narrow — gains concentrate on drum transients.
2. A full mix contains all source types simultaneously; region classification ("drum section" vs "vocal section") is coarse — the vocal and drums overlap in the same region.
3. Every engine switch creates a seam that must be crossfaded; seams on dense mixes are audible (comb filtering if the two renders are partially decorrelated).
4. Misclassification is silent and confusing to users.

**Conclusion:** Auto is justified only in two narrow forms:
- (a) **Soft, per-bin, inside one engine** (HPSS soft masks or fuzzy tonal/noise/transient memberships): Transient-mode quality everywhere it helps, no seams. This is what "Auto" should mean long-term.
- (b) **Region-level choice between Smooth and Transient only**, at bar boundaries, with hysteresis (min segment ≥ 4 bars), driven by percussive-energy ratio and onset density, *only* if listening tests show (a) is not enough. Never auto-select Resample (changes pitch) or Slice (chops sustains) for full mixes.

Proposed feature set for (b), all computable from the analysis pass:
- `P = E_percussive / (E_h + E_p)` from HPSS soft masks per bar;
- onset density (onsets/s, spectral-flux peaks) and onset strength;
- tonalness (spectral flatness inverse / peakiness);
- local `|log r|` (if r ≈ 1 → Passthrough wins regardless);
- stereo width (ICC) — informational only.
Decision rule sketch: `Transient if P > 0.35 and onset_strength > θ for ≥ 4 bars; else Smooth` (thresholds to be fit on the benchmark corpus by listening — UNVERIFIED numbers).

### TD-8 Segment boundary crossfades (when segments use different processing)

Rules:
1. Every segment renders with **handles**: extend the render ±H on both sides, H = max(engine pre-roll, 250 ms), using the same global `W⁻¹`, so overlapping renders are time-aligned in the output timeline.
2. Place the crossfade so it **ends ≥ 5 ms before** a strong onset (prefer the downbeat preceding the region change) so the attack comes entirely from one engine; avoid placing it on an onset.
3. Length L = 20 ms default (5–50 ms). Short when Passthrough↔PV (highly correlated), longer for Smooth↔Transient.
4. Correlation-adaptive law: measure `ρ` (normalised cross-correlation, zero lag) of the two renders over the overlap. Gains `g_a(t)`, `g_b(t)` satisfying `g_a² + g_b² + 2ρ g_a g_b = 1`. ρ ≈ 1 → linear (equal-gain); ρ ≈ 0 → equal-power. Practical: `g_b = sin(πt/2L)`-shaped for ρ < 0.5, `g_b = t/L` for ρ > 0.9, interpolate between.
5. Stereo: identical gains on all channels.
6. Validate: no level dip/bump > 0.5 dB in short-term loudness across the seam; no click (HF energy spike) — automated test.

**Passthrough (internal mode).** Where |r − 1| < 1e-4 for ≥ 2 bars, the output is the source shifted by a constant offset `Δ = W(s) − s`. If Δ is integer → bit-exact copy; else apply a fractional-delay (windowed-sinc) shift. Crossfade into/out of PV output with rule 4 (ρ is typically high because a PV at r ≈ 1 is near-identity). Benefit: "unchanged" regions are truly unchanged (matches the UI's correction-intensity map). Risk: seam audibility; ship behind a setting until tested.

### TD-9 Extreme ratios, offline vs real-time, CPU

- BeatFix needs ~0.9–1.1; tempo-change features (e.g. 90 → 128 BPM) imply 0.70–1.42. Warn above ±15 % (|log2 r| > 0.2). Signalsmith: clean up to 2x then per-bin time-factor randomisation; Bungee supports any speed incl. reverse; PGHI good at 2x. >2x is out of scope (Texture/granular territory).
- **Offline advantages** we should use: look-ahead (full-file analysis for onsets/HPSS masks with non-causal median filters), two-pass (RB study), chunked multi-threading with handles, no latency constraint.
- **Preview**: run the same engine streaming at playback time from the warp map (Signalsmith and Bungee both stream) so preview ≈ render. Offline export may use higher-quality settings (e.g. longer block) — state clearly in UI if preview differs.
- **CPU estimate (UNVERIFIED, benchmark needed)**: PV with N = 4096, Hs = 1024 at 44.1 kHz: 43 frames/s/channel; forward+inverse FFT ≈ 2·5·N·log2 N ≈ 0.49 MFLOP/frame → ≈ 21 MFLOP/s/channel → 3-min stereo song ≈ 7.6 GFLOP → well under a few seconds single-threaded including overheads. HPSS (STFT + two median filters ~17 taps over ~1025 bins/frame) adds a similar order. Rubber Band R3 is documented as "much more CPU" than R2 (R3 multi-resolution); expect it to be the slowest option. Target: full-song render < 10 s for a 4-min track on a mid-range laptop.
- **Determinism**: render-from-source must be reproducible. Signalsmith only uses its RNG above 2x stretch (default-seeded engine). Fix FFT library and avoid thread-count-dependent summation order.

### TD-10 Worked numeric example: 108 → 110 BPM at 44.1 kHz

Constant-tempo case (one section):

| Quantity | Value |
|---|---|
| Source beat period | 60/108 = 0.555556 s = 24 500.0 samples |
| Target beat period | 60/110 = 0.545455 s = 24 054.55 samples |
| Ratio r = α = T/S | 108/110 = 0.981818 (compress 1.818 %) |
| 3:00 source duration → output | 176.73 s |
| If done by **Resample** instead | pitch +1200·log2(110/108) = **+31.8 cents** (audible vs. reference; ≈ 1/3 semitone) |
| Reference: cents per tempo change | 0.5 % → 8.6 c; 1 % → 17.2 c; 2 % → 34.3 c; 5 % → 84.5 c |

Phase-vocoder parameters (generic PV, long window for a full mix):

| Param | Value |
|---|---|
| N (Hann) | 4096 (92.9 ms); bin spacing 10.77 Hz |
| Hs = N/4 | 1024 (23.2 ms) |
| Ha = Hs/α | 1042.96 samples (fractional → interpolate or accumulate rounding) |
| Hs − Ha | −18.96 samples/frame |
| Synthesis frames per target beat | 23.49 |
| 440 Hz partial: synthesis phase advance per frame | 2π·440·1024/44100 = 64.19 rad |
| Its phase drift vs. analysis per frame | 2π·440·(−18.96)/44100 = −1.19 rad → vertical coherence lost within ~2 frames unless locked |
| 3-min song | ≈ 7 752 frames/channel |

Signalsmith `presetDefault` at 44.1 kHz: block 5292 (120 ms), interval Hs = 1323 (30 ms); per call `n_out = 1323`, `n_in ≈ 1323/0.981818 = 1347.5` → alternate 1347/1348 via the rounding accumulator.

Variable case (drifting AI track): bar where detected local tempo = 109.3 BPM → r = 109.3/110 = 0.99364 (−0.64 %) → if resampled, +11 cents; with PV, Ha = 1030.6 for Hs = 1024. Adjacent bar at 108.6 BPM → r = 0.98727: step of 0.64 % between bars — inaudible as a tempo jump, handled per frame by the driver.

WSOLA equivalent: N = 2205 (50 ms), Hs = 1102, Δmax ≈ 1102 (25 ms) per Driedger & Müller's rule of thumb — too coarse for kick/snare on a dense mix (doubling risk within ±25 ms); not recommended as default.

HPSS hybrid at 44.1 kHz (scaled from Toolbox example; tune by listening): HPSS STFT N = 2048, hop 512; median lengths ≈ 17 frames (harmonic, ≈ 200 ms) and ≈ 17 bins (percussive, ≈ 370 Hz) — Toolbox example used 10/10 at its own resolution; soft (Wiener-type, power 2) masks; percussive OLA window 512 (11.6 ms), Hs = 256; harmonic → Smooth engine.

---

## Alternatives Considered

| Option | Verdict | Why |
|---|---|---|
| Signalsmith Stretch as default "Smooth" | **Chosen (MVP)** | MIT; native variable ratio via per-call in/out counts; per-bin linked stereo verified in source; Rust crates exist; quality reported comparable to RB in OpenMPT eval (baseline). |
| Bungee Basic | **Alternate engine / fallback** | MPL-2.0 (file-level copyleft — legal review for static linking/modifications); per-grain `position` = cleanest `W⁻¹` mapping; stereo linking unverified; Pro edition (proprietary) adds "crisp transients". |
| Rubber Band R3 + keyframe map | **QA reference; optional premium engine after legal review** | Best documented quality on mixes/vocals/bass; GPL or paid (£590 standard, attribution). Use CLI/library internally for A/B (licence implications of internal use → legal review). |
| Own PV (identity/scaled locking or PGHI) | **Phase 2+ option** | Full control (transient reset, HPSS integration, multi-resolution) and clean IP from papers; costs months of tuning to match Signalsmith/R3. |
| HPSS hybrid wrapper ("Transient") | **Phase 1.5 (after MVP), engine-agnostic** | Documented transient benefit without explicit detection; wrapper uses any Smooth engine for the harmonic part. |
| WSOLA / SoundTouch | Rejected as default | Warble/doubling on dense polyphonic mixes; LGPL. Possibly used internally for the percussive branch only (short window OLA suffices). |
| TD-PSOLA | Rejected | Monophonic assumption. |
| Granular "Texture" | Deferred | Not needed for ±5 % corrections; useful only for extreme ratios/ambient. |
| élastique (zplane) | Not pursued now | Commercial SDK, upfront + per-unit (baseline); functional study only. |
| Neural TSM | Not for MVP | No mature licence-clean, offline-validated model found in this pass (not deeply searched) — open question. |

## Evidence / Sources

- Driedger, J., Müller, M. "A Review of Time-Scale Modification of Music Signals." Appl. Sci. 2016, 6(2), 57. https://www.mdpi.com/2076-3417/6/2/57 (text read via https://www.cs.bu.edu/fac/snyder/cs583/Literature%20and%20Resources/AReviewOfTimeScaleModification.pdf)
- Driedger, J., Müller, M. "TSM Toolbox: MATLAB Implementations of Time-Scale Modification Algorithms." DAFx-14 (GPL). https://dafx14.fau.de/papers/dafx14_jonathan_driedger_tsm_toolbox_matlab_imple.pdf
- Driedger, J., Müller, M., Ewert, S. "Improving Time-Scale Modification of Music Signals Using Harmonic-Percussive Separation." IEEE SPL 21(1):105–109, 2014. https://audiolabs-erlangen.com/resources/2014-SPL-HPTSM/
- Laroche, J., Dolson, M. "Improved phase vocoder time-scale modification of audio." IEEE TSAP 7(3):323–332, 1999 (cited via Driedger & Müller §5.6; equations standard).
- Průša, Z., Holighaus, N. "Phase Vocoder Done Right." EUSIPCO 2017, pp. 1006–1010; arXiv 2202.07382. https://ar5iv.arxiv.org/html/2202.07382
- Röbel, A. "A new approach to transient processing in the phase vocoder." DAFx-03. https://hal.archives-ouvertes.fr/hal-01161124
- Damskägg, E.-P., Välimäki, V. "Audio Time Stretching Using Fuzzy Classification of Spectral Bins." Appl. Sci. 7(12):1293, 2017. https://www.mdpi.com/2076-3417/7/12/1293
- Fierro, L., Välimäki, V. fuzzy-decomposition-based objective TSM evaluation / enhanced 3-way decomposition. https://research.aalto.fi/fi/publications/towards-objective-evaluation-of-audio-time-scale-modification-met/ , https://arxiv.org/abs/2210.14041
- Roberts, T., Paliwal, K. "A time-scale modification dataset with subjective quality labels." JASA 148(1):201–210, 2020. https://arxiv.org/abs/2006.00848 ; dataset https://ieee-dataport.org/open-access/time-scale-modification-dataset-subjective-quality-labels
- Signalsmith Stretch repo/README/header (MIT, tag 1.4.0). https://github.com/signalsmith-audio/signalsmith-stretch ; ADC22 talk "Four Ways To Write A Pitch-Shifter" https://www.youtube.com/watch?v=fJUmmcGKZMI ; Context7 `/signalsmith-audio/signalsmith-stretch`, `/colinmarc/signalsmith-stretch-rs`
- Bungee repo/header (MPL-2.0, v2.4.30). https://github.com/bungee-audio-stretch/bungee
- Rubber Band Library header v4.0.0 (GPL-2.0+/commercial). https://github.com/breakfastquay/rubberband ; https://breakfastquay.com/rubberband/ ; Context7 `/breakfastquay/rubberband`
- Fraunhofer patent US 9,230,558 B2. https://patents.google.com/patent/US9230558B2/en (related US 9,236,062; US 9,275,652)

## Recommendation

1. **Engine abstraction** `StretchEngine { synthesis_hop(), latency(), render(src, Winv, out_range) }`, driven exclusively by `W⁻¹` (f64) on a fixed synthesis grid. All modes are implementations of it. (High)
2. **MVP default mode "Smooth"** = Signalsmith Stretch, `presetDefault` (120 ms / 30 ms), stereo linked (built-in), one `process()` per interval with drift-free rounding. Offer a "High quality" export preset with a longer block (e.g. 160–200 ms / 40 ms — tune by listening; UNVERIFIED benefit). (High for engine choice on licence/API; Med on final quality until listening test)
3. **MVP mode "Resample"** = variable-rate windowed-sinc resampling following W (libsamplerate BSD-2 / r8brain MIT / rubato MIT-Apache). Label "pitch follows tempo"; show cents shift. Zero TSM artifacts; for users who prefer varispeed or for ±0.3 % nudges. (High)
4. **Phase 1.5 "Transient"** = HPSS hybrid wrapper: channel-shared soft masks → harmonic through Smooth engine, percussive through warped short-window OLA (512/256 at 44.1 kHz), both on the same `W⁻¹`, sum. Ship after an A/B listening test on drum-heavy corpus items shows benefit. (Med)
5. **"Slice"** (onset/beat slicing: each inter-onset slice copied unmodified to `W(onset)`, tails crossfaded on compress, tail Smooth-stretched on expand) — post-MVP, drum stems / drum-only intros only; **legal review** re Fraunhofer family first. (Med)
6. **"Texture"** (granular) — defer; not needed for beat correction. (High)
7. **"Auto"** — do **not** ship region-switching Auto in MVP. Phase 2: per-bin soft hybrid (HPSS/fuzzy) inside one engine; region-level Smooth/Transient switching only if tests show need, at bar boundaries, ≥ 4-bar hysteresis, correlation-adaptive crossfades. Never auto-select Resample or Slice on full mixes. (Med)
8. **Internal "Passthrough"** for unchanged regions (|r−1| < 1e-4 for ≥ 2 bars) behind a setting; verify seams. (Med)
9. **Rubber Band R3 (keyframe map, `OptionChannelsTogether`)** as the internal quality reference for listening tests and regression baselines; decide on licensing it as a premium engine after tests + legal review. (Med)
10. **Stereo rule**: no engine may process channels independently; linked phase or identical time-domain ops only; HPSS masks shared across channels. (High)

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Signalsmith transients on kick/snare soften on dense masters | Med | Med | Transient mode (HPSS); listening test; RB R3 comparison. |
| Latency/offset misalignment → constant output offset vs grid | Med | High | Impulse-train alignment test (error < 0.5 ms) per engine and per ratio. |
| f32 time accounting drift on long files | High if naive | High | f64/i64 positions; integer rounding accumulator. |
| Seams in mixed-mode renders | Med | Med | Handles, onset-aware placement, ρ-adaptive crossfades, automated seam test. |
| HPSS mask leakage → hollow/flammy attacks | Med | Med | Shared masks, same `W⁻¹`, alignment test; soft masks. |
| Patent exposure (transient copy/reinsert) | Unknown | High | Legal review before Slice/transient-copy modes. |
| MPL/GPL obligations (Bungee, RB) | — | High | For legal review; default engine MIT. |
| True-peak overs after PV re-synthesis on limited masters | Med | Med | True-peak meter at export; optional −1 dBTP safety gain (user choice). |
| Preview ≠ export if settings differ | Med | Low | Same engine & settings by default; label HQ export. |

## Open Questions

1. Does Signalsmith at 120/30 ms vs RB R3 vs Bungee Basic differ audibly at |1 − r| ≤ 3 % on AI masters? (Listening test, MUSHRA-style, ≥ 15 listeners, Roberts-Paliwal-style material categories.)
2. Exact Signalsmith input/output latency alignment when driving variable chunks — verify with impulse trains; does `outputSeek` pre-roll behave correctly with non-unity initial rate?
3. Bungee Basic: stereo linking (IPD preservation)? Exact output-frame ↔ grain-position correspondence?
4. Rubber Band keyframe accuracy (sample error at anchors) and CPU for a 4-min stereo song with R3.
5. Is Passthrough's seam less audible than the near-identity PV it replaces?
6. HPSS parameters at 44.1/48 kHz for AI masters (codec noise may be classed percussive).
7. Patent landscape for phase reset / HPSS hybrid / slicing — legal review.
8. Neural TSM state of the art (2024–2026) — not surveyed here.

## Implementation Implications

- Warp-map module must expose `W` and `W⁻¹` as f64 functions plus local slope; engines never see beats.
- Render pipeline: decode → float32 planar at native rate → plan segments (mode per segment) → render segments with handles (parallel) → assemble with crossfades → true-peak check → encode/dither. Source stays immutable.
- Engine trait implementations: `SignalsmithEngine` (MVP), `ResampleEngine` (MVP), `HpssHybridEngine<Inner>` (1.5), `BungeeEngine` (eval), `RubberBandEngine` (QA/premium, licence-gated build feature), `SliceEngine` (post-legal).
- Required automated tests (details in DSP_RESEARCH.md): impulse-train alignment, r = 1 near-null, constant-ratio duration exactness (±1 sample), IPD/ICC preservation on panned noise + Haas test signal, sine-sweep pitch preservation (< 1 cent), transient attack-time ratio, seam click/level tests, determinism (bit-identical re-render).
- Expose to UI: mode per region (Auto deferred), cents readout for Resample, warning above ±15 %, "unchanged" regions (Passthrough).
