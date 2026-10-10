# ADR-006: Time-Stretch Architecture

**Status:** Accepted, 2026-10-08.

## Context
The engine must apply per-beat ratio changes (typically ±0–5 %) to full stereo mixes, preserve pitch, transients and image, and run offline faster than realtime. Licences narrow the field ([LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md)): Rubber Band is GPL or paid (£590/£1,490/£9,320), élastique is a commercial SDK, StaffPad (Audacity) is GPL by default, and Signalsmith (MIT) and Bungee (MPL-2.0) are permissive.

## Alternatives
| Option | Verdict | Why |
|---|---|---|
| **Signalsmith Stretch 1.4.0** (MIT) | **Chosen: "Smooth"** | native variable ratio per `process()` call, linked stereo per bin, `seek()`/`flush()` for exact chunking. Quality "comparable" to Rubber Band per OpenMPT (listening confirmation pending) |
| Rubber Band R3 | Reference only (dev builds) | best on full mixes, offline `setKeyFrameMap` fits the warp. GPL, so never distributed unless a commercial licence is bought later |
| Bungee (MPL-2.0) | Alternate | per-grain position control maps directly to W⁻¹. Stereo linking unconfirmed |
| SoundTouch (WSOLA, LGPL) | Rejected | weaker on full mixes |
| Own phase vocoder / PGHI | Rejected | months of DSP work to reach parity |
| Per-region engine switching ("Auto") in MVP | Rejected | seams and mode flapping. No evidence of benefit yet. Preferred future: soft per-bin H/P classification inside one engine |
| HPSS transient hybrid | Phase 1.5 | Driedger/Müller/Ewert 2014. Ship after an A/B test |
| Transient remove-and-reinsert ("Slice") | Deferred | patent flag US 9,230,558 B2 (to 2029), **for legal review** |

## Decision
`ITimeStretcher` is one of only two interfaces. MVP modes: **Smooth** (Signalsmith, default), driven by `W⁻¹` on a fixed output hop with absolute sample accounting, and **Resample** (r8brain varispeed), allowed only for a single constant ratio with the pitch shift shown in cents. Rubber Band R3 sits behind a dev-only build flag for calibration and listening references.

## Reasoning
Signalsmith is the only permissive engine verified (in source) to support exactly the drive model we need. Keeping Rubber Band as a reference gives a quality yardstick without a licence. Resample costs nothing extra and is artifact-free for users who accept the pitch change.

## Tradeoffs
Drums in dense mixes may smear more than with a transient-aware hybrid. Mitigations: cuts at beats, Phase 1.5 Transient mode, and the listening gate (non-inferiority ≤ 5 MUSHRA points vs R3).

## Consequences
Phase 0 spike: impulse-alignment test with variable chunks (< 0.5 ms), chunk-join null test, r = 1 pass-through measurement, CPU benchmark.

## References
[TIME_STRETCH_RESEARCH.md](../03_research/TIME_STRETCH_RESEARCH.md), [TIME_STRETCH_ENGINE.md](../04_algorithms/TIME_STRETCH_ENGINE.md), [TRANSIENT_PRESERVATION.md](../04_algorithms/TRANSIENT_PRESERVATION.md), [HYBRID_AUTO_ENGINE.md](../04_algorithms/HYBRID_AUTO_ENGINE.md), [LIBRARY_EVALUATION.md](../03_research/LIBRARY_EVALUATION.md).
