# Stereo and Phase Preservation

Status: algorithm spec, 2026-10-08. Decision: linked-stereo time-stretch ([DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1 "Time-stretch"). Research: [TIME_STRETCH_RESEARCH.md](../03_research/TIME_STRETCH_RESEARCH.md) (TSR) Finding 5, TD-5; [DSP_RESEARCH.md](../03_research/DSP_RESEARCH.md) (DSPR) TD-7; metric definitions owned by [METRICS.md](../07_quality/METRICS.md) O13.

## Objective
Keep the stereo image, inter-channel phase and mono compatibility of the source through warping; define how this is measured in tests and checked after every render; define mono and > 2-channel handling.

## Why It Matters
Wide AI masters (chorus, Haas, stereo reverbs) are the norm. Independent per-channel stretching makes panned sources wander, widens the image artificially and makes the mono fold-down "phasy" (TSR TD-5). These faults are hard for users to name but easy to hear on club/phone mono playback.

---

## 1. Linked-stereo processing (MVP)

**Rule (High, TSR Rec. 10):** no engine processes channels independently. PV engines share one phase trajectory per bin; time-domain operations are identical on all channels; HPSS masks are shared.

Signalsmith mechanism (verified in source, TSR Finding 5), per frame m and bin k:
```
c_ref(m,k) = argmax_c |X_c(m,k)|                         reference = max-energy channel
φ_s,ref    = engine phase prediction for c_ref            (time + neighbour-bin prediction)
φ_s,c      = φ_s,ref + ( φ_c(m,k) − φ_ref(m,k) )          every other channel: input IPD re-applied
|Y_c|      = |X_c|                                        per-channel magnitude kept
```
Properties that follow (each tested, §6):
| Property | Why it holds | Test |
|---|---|---|
| IPD per bin preserved each frame | re-applied from input | O13f |
| ILD preserved | per-channel magnitudes | DSPR TD-7 ILD |
| L≡R ⇒ outputs identical | IPD = 0, equal magnitudes | O13a (hard) |
| L = −R ⇒ outputs anti-phase | IPD = π re-applied | O13b (hard) |
| ITD (Haas) preserved for stationary content | IPD slope across bins preserved | O13c |
| Mono fold-down tracks the stereo result | `L+R` keeps its phase relations | mono fold-down check |

Other engines: Resample and the percussive OLA branch apply identical time-domain operations → exact. Rubber Band reference runs with `OptionChannelsTogether` (aims at the same goal; mechanism differs). Bungee Basic linking is **UNVERIFIED**: it is not eligible as fallback until it passes O13 a/b exactly (TIME_STRETCH_ENGINE.md §9). HPSS masks come from `max(|X_L|,|X_R|)` (TRANSIENT_PRESERVATION.md §1.1).

## 2. Why not independent L/R, and why not M/S processing
| Method | What goes wrong | Evidence |
|---|---|---|
| Independent L/R PV | per-bin phase drift is propagated separately per channel; IPD error grows frame by frame → image wander, artificial widening, comb filtering in mono | TSR TD-5; RB docs (`OptionChannelsApart` widens). Supporting measurement: RB R2 channels-apart raised the sample peak up to +1.25 dBFS vs +0.68 with channels together (AUDIO_QUALITY_RESEARCH §4.12 E2), consistent with decorrelation |
| M/S encode → independent PV → decode | M and S phases drift relative to each other → panned sources (which live in both M and S) move | TSR TD-5 |
| Linked phase | none of the above, by construction | chosen |

M/S is used for **analysis** only (§3), never for processing.

## 3. Mid/side analysis
`M = (L + R)/2`, `S = (L − R)/2`. Quantities:
- `R_MS = 10·log10(E_S / E_M)` per block: width. A rise after processing means artificial widening, a fall means collapse.
- Mono fold-down: integrated loudness (BS.1770, libebur128) of `M` before and after, compared with the stereo loudness change.
- Out-of-phase content (`E_S > E_M` sustained) is legal music (some wide synths); it is reported, never "fixed".

## 4. Metrics and thresholds
Comparison principle: an output block at output time τ is compared with the source block at `W⁻¹(τ)` of length `len/r` (DSPR TD-7). All thresholds **PROVISIONAL** until the Phase 0 calibration (DSP_TEST_STRATEGY §7).

| Metric | Definition | Test threshold (synthetic, METRICS O13 owns) | In-app check (real music, after render) |
|---|---|---|---|
| Mono invariance | input L≡R ⇒ `max|L−R|` | ≤ 1e-7 (hard) | — |
| Polarity | input L=−R ⇒ `max|L+R|` | ≤ 1e-7·peak (hard) | — |
| ITD | GCC-PHAT lag on S12(c) | \|ΔITD\| ≤ 1 sample | — |
| Inter-channel correlation (ICC) | `ρ = ΣLR / sqrt(ΣL² ΣR²)`, zero lag, 50 ms blocks (tests) / 400 ms blocks (in-app) | median \|Δρ\| ≤ 0.02 | Review if median \|Δρ\| > 0.05 |
| Phase coherence (IPD) | `Δ_k = IPD_out − IPD_in`; error `= arg Σ_k w_k e^{iΔ_k}`, coherence `C = |Σ_k w_k e^{iΔ_k}| / Σ_k w_k`, `w_k` = magnitude, bins < 1.5 kHz | error ≤ 5° | — (needs frame-exact mapping; test only) |
| ILD | `20·log10(|X_L|/|X_R|)` per band | \|Δ\| ≤ 0.5 dB | — |
| M/S energy ratio | `ΔR_MS` | ≤ 0.5 dB | Review if \|ΔR_MS\| > 1 dB |
| Mono fold-down | `ΔLUFS(M) − ΔLUFS(stereo)` | ≤ 0.3 LU loss | Review if loss > 0.5 LU |

Note: DSPR TD-7 gives ICC `< 0.05` per band (mid/high); METRICS O13 gives `≤ 0.02` broadband per 50 ms block. Tests use METRICS (QA owns thresholds); the in-app check uses the looser 0.05 because real-music block ICC varies with content. In-app thresholds are set loose on purpose: they are diagnostics that create a Review item, never a block (QUALITY_VALIDATION.md V8).

## 5. Channel layouts
| Source | Handling | Why |
|---|---|---|
| Mono | engine instance with 1 channel; mono output; stereo checks skipped | never upmix silently (AUDIO_QUALITY_RESEARCH §4.3) |
| Stereo, L≡R (dual mono) | processed as stereo; O13a guarantees identical outputs | no special path needed |
| Stereo, one channel silent | reference is the active channel per bin; silent channel's input magnitude stays ~0 | per-channel magnitudes |
| > 2 channels | **MVP: rejected at import** ("Multichannel audio (6 ch) isn't supported yet; use a stereo mixdown.", UX_BEHAVIOR.md §15) | MVP scope (DECISION §5) |
| > 2 channels, Post-MVP | one N-channel Signalsmith instance links every channel to the per-bin reference (the mechanism is channel-count generic); layout semantics (LFE, downmix) are a separate design | — |

## 6. Tests
| ID | Signal (DSP_TEST_STRATEGY §3) | Pass |
|---|---|---|
| ST-1 | S12(a) L≡R, Smooth/Resample/Transient, r ∈ TSE-1 set | O13a exact |
| ST-2 | S12(b) L=−R | O13b exact |
| ST-3 | S12(c) ITD 0.5 ms noise bursts | \|ΔITD\| ≤ 1 sample |
| ST-4 | S12(d) ILD 6 dB, S12(f) 90° pair | ILD ≤ 0.5 dB, IPD error ≤ 5° |
| ST-5 | S12(e) decorrelated noise | ICC stays ≈ 0 (\|Δρ\| ≤ 0.02); no added correlation |
| ST-6 | S12(g) alternating hard-L/R clicks | no channel swap, no crosstalk > −60 dB |
| ST-7 | Corpus wide masters (GOLD-06, GOLD-11) | all O13 c–f; in-app check produces no Review item |
| ST-8 | Mono file | mono output; channel count in header = 1 |

## Open Questions
1. Audible thresholds for ICC/IPD change on wide AI masters (DSPR Open Q2): calibrate with a small listening test.
2. Bungee Basic stereo linking (TSR Open Q3).
