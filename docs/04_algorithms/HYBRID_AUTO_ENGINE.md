# Hybrid "Auto" Engine (future design, not MVP)

Status: design note, 2026-10-08. **Post-MVP.** The MVP has no Auto mode and no region switching ([DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1 "Slice / Texture / region-switching Auto", §4). This file records what Auto should become and what must be proven first. Research: [TIME_STRETCH_RESEARCH.md](../03_research/TIME_STRETCH_RESEARCH.md) (TSR) TD-7, TD-8, Finding 14; [DSP_RESEARCH.md](../03_research/DSP_RESEARCH.md) (DSPR) TD-3. Brief §9 "Auto mode" asks to validate, not assume, the region heuristic.

## Objective
Define the long-term automatic processing choice, why region switching is deferred, the preferred soft per-bin alternative, the rules any region switching must obey, the classification features, the validation required before shipping, and whether ML is warranted.

---

## 1. Why region switching is deferred
The brief's example ("intro/ambient → smooth, vocal → smooth, drum-heavy → transient/slice") assumes sections are acoustically homogeneous. In a full mix they are not.

| Argument | Evidence | Weight |
|---|---|---|
| Full mixes carry every source type at once; "drum section" also contains vocals, pads and bass | TSR TD-7 item 2 | High |
| At ±5 % ratios the quality spread between a good linked PV and a hybrid is narrow; gains concentrate on drum attacks | TSR Finding 3, TD-7 item 1 | Med (listening test pending) |
| Every switch is a seam; two renders of a dense mix are partially decorrelated, so crossfades comb-filter or dip | TSR TD-7 item 3; TSE §3.4 (phase history) | High |
| Misclassification is silent and confusing ("why did bar 33 change sound?") | TSR TD-7 item 4 | High |
| Resample on a region changes pitch; Slice chops sustains: neither can be auto-selected on full mixes | TSR TD-7 conclusion | High |

Consequence: in the MVP the whole song uses one mode. The UI's "separate indicator for processing-mode change" (brief §12) therefore has nothing to show in the MVP; WAVEFORM_VIEW.md owns it for later.

## 2. Preferred design: soft per-bin classification inside one engine
Auto should mean "the right treatment per time-frequency bin", not "the right engine per region".

**Stage A (first, cheap): HPSS hybrid everywhere.** The Phase 1.5 Transient mode ([TRANSIENT_PRESERVATION.md](TRANSIENT_PRESERVATION.md)) is already a soft, per-bin, two-class Auto: a bin dominated by sustained energy goes through the PV, a bin dominated by broadband short energy goes through short OLA. Where there are no drums, `M_p ≈ 0` and the output equals Smooth. If Transient passes its listening gate on drums **and** shows no loss on vocals/pads, "Auto" can simply be Transient, with no seams and no classifier.

**Stage B (research): fuzzy three-class bins (Damskägg & Välimäki 2017, TSR Finding 14).** Per bin, memberships `μ_tonal, μ_noise, μ_transient ∈ [0,1]`, `Σμ = 1`, derived from the HPSS ratio and a tonalness measure (spectral flatness / peakiness, DSPR TD-3). Phase synthesis per bin:
```
φ_s(m,k) = μ_tonal · φ_PV(m,k)            phase-gradient propagation (horizontal coherence)
         ⊕ μ_noise · φ_rand(m,k)           randomised phase: avoids "tonalised" metallic noise/reverb
         ⊕ μ_transient · φ(m,k)            analysis phase: vertical coherence at the attack (reset-like)
```
(⊕ = magnitude-weighted circular combination.) Reported to work on singing, techno and jazz with vocals without explicit decomposition. Requires our own PV core (Signalsmith exposes no per-bin phase hooks) → Phase 2+, after the own-PV option (TSR Alternatives "Own PV") is justified. The transient term is a phase-reset mechanism and joins the legal review in TRANSIENT_PRESERVATION §5.

## 3. Region rules (only if Stage A/B is proven insufficient)
If listening tests show a need for region-level Smooth↔Transient switching, all of these apply (TSR TD-7(b), TD-8):

| Rule | Value | Why |
|---|---|---|
| Candidate modes | Smooth and Transient only. Never auto-select Resample, Slice or Texture on full mixes | pitch change / chopped sustains |
| Boundaries | bar lines (downbeats) only | musical units; listeners expect changes there |
| Hysteresis | minimum region length ≥ 4 bars; a switch requires the decision to hold for 4 consecutive bars | avoids flicker on fills |
| Handles | each region rendered ±H, H = max(engine pre-roll, 250 ms), same global W⁻¹ | renders are time-aligned in the output |
| Crossfade placement | fade **ends ≥ 5 ms before** the next strong onset (prefer the preceding downbeat's quiet span); never on an onset | the attack comes from one engine only |
| Crossfade length | 20 ms default, 5–50 ms; longer for Smooth↔Transient | TSR TD-8 rule 3 |
| Crossfade law | ρ-adaptive: `g_a² + g_b² + 2ρ·g_a·g_b = 1` (same as chunk assembly, TSE §8.3) | linear at ρ≈1, equal-power at ρ≈0 |
| Stereo | identical gains on all channels | image |
| Seam validation | short-term loudness deviation ≤ 0.5 dB; no click flag (O15) | automated test |
| User control | each region shows its mode and `modeSource` (`auto`/`user`); user override per region (WARP_MAP.md `regions[]`) | misclassification must be visible and fixable |

Decision rule sketch (thresholds **UNVERIFIED**, fit on the corpus by listening):
`Transient if P_bar > 0.35 and onset_strength_bar > θ for ≥ 4 bars, else Smooth`; regions with `|log r| ≈ 0` are rendered by whatever mode surrounds them (Passthrough is a separate decision, TSE §6).

## 4. Source-classification features (all from the existing analysis pass)
| Feature | Definition | Use |
|---|---|---|
| Percussive ratio `P` | `Σ|M_p X|² / Σ|X|²` per bar (DSPR TD-3) | primary |
| Onset density / strength | SuperFlux peaks per second; mean peak height (DSPR TD-3) | primary |
| Per-band flux | sub < 120 Hz, low-mid, > 5 kHz | kick vs hat dominance |
| Tonalness | inverse spectral flatness `exp(mean log|X|²)/mean |X|²` | tonal vs noise (Stage B memberships) |
| Local `|log2 r|` | from W | if ≈ 0 the mode barely matters |
| Stereo width (ICC) | STEREO_PHASE_PRESERVATION §3 | informational only |
| Effective bandwidth | AUDIO_QUALITY_RESEARCH §4.10 | guards HPSS against codec-noise misclassification |

## 5. Validation required before any Auto ships
1. **Stage A**: Transient's own gate (TRANSIENT_PRESERVATION §6) extended to all LIS-MUSHRA categories (vocals, pads, piano, bass, dense mix, stereo-wide): no category significantly worse than Smooth. Pass → Auto = Transient.
2. **Region switching** (only if Stage A fails on some category): (a) seam test on S16 and corpus items with forced switches at every bar: zero O15 flags, loudness deviation ≤ 0.5 dB; (b) classifier accuracy vs listener-chosen best mode per bar on ≥ 50 corpus excerpts (labels from LIS-PAIR); (c) LIS-PAIR Auto vs best single mode: Auto must be preferred or equal on every category; (d) ABX on seams: no listener significant (SUBJECTIVE_LISTENING_TESTS §6).
3. **Stage B**: own-PV parity with Signalsmith on TSE tests first, then (1).
4. Calibrate every threshold on the corpus; none ships as a guess (brief §39).

## 6. ML option analysis
| Option | Benefit | Cost / risk | Verdict |
|---|---|---|---|
| Learned region classifier (CNN on mel spectrogram → {Smooth, Transient}) | might beat hand features | needs per-bar "best mode" labels from listening (expensive, subjective); region switching itself is the problem, not the classifier | **No**: a better classifier does not remove seams |
| Learned per-bin masks (separation-style, e.g. Demucs drums stem as M_p) | cleaner percussive split than median filtering | large model, CPU cost, non-bit-exact inference (ONNX, DECISION §3 item 4), separation artefacts become stretch artefacts | Possible Stage A+ upgrade; evaluate only if HPSS leakage is the measured limiting factor |
| Neural TSM end-to-end | could avoid PV artefacts | no mature licence-clean offline-validated model found (TSR Alternatives, Open Q8) | Not now |
| Learned quality predictor to choose the mode | automatic A/B | best published TSM measure PCC 0.865; code without licence (METRICS Key Finding 5) | Research only |

Policy reason (brief §36): ML only where it measurably beats a deterministic method and remains testable. HPSS and fuzzy classification are deterministic, explainable and cheap; ML enters only to fix a measured weakness.

## Open Questions
1. Does Transient-everywhere lose quality on vocals/pads versus Smooth? (decides whether Stage A alone is Auto)
2. Is the own-PV investment (Stage B) justified by listening data?
3. Per-bar "best mode" labels: can LIS-PAIR produce enough of them at acceptable cost?
