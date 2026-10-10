# ADR-008: Waveform Rendering

**Status:** Accepted, 2026-10-08.

## Context
Two lanes (original, corrected) must zoom and scroll smoothly on songs up to 60 minutes, with the corrected lane visible **before** the render finishes.

## Alternatives
| Option | Verdict | Why |
|---|---|---|
| Draw from raw PCM at every zoom | Rejected | 28.8 M samples/channel for 10 min at 48 kHz, so far too slow when zoomed out |
| **Min/max/RMS peak pyramid (base 64 samples/px, ×4 per level, float32), raw PCM below 64 spp** | **Chosen** | same proven approach as audiowaveform/peaks.js, Audacity summaries, REAPER `.reapeaks`. 14.4 MB per channel per lane for 10 min @ 48 kHz |
| GPU (Direct2D/OpenGL) drawing | Deferred | CPU redraw ≈ 41 k reductions per frame at 2560 px. Add GPU/tiles only if the ≤ 4 ms/lane budget is missed |
| Wait for the render before showing the corrected lane | Rejected | slow feedback breaks the edit loop |
| **Estimate: query the source pyramid over `[W⁻¹(t_a), W⁻¹(t_b)]` per pixel column** | **Chosen** | instant, exact in shape for small ratios; replaced chunk by chunk with real peaks |

## Decision
Peak pyramids are built once per source and per render generation (cached). The corrected-lane estimate is drawn at 60 % opacity labelled "Estimate". Column edges snap to whole samples to prevent shimmer. Correction classes come from `|log2(r/g)|` ([WAVEFORM_VIEW.md](../06_ui/WAVEFORM_VIEW.md)).

## Tradeoffs
float32 pyramid memory is 2× int16 (7.2 MB). We accept it for simplicity and RMS precision.

## References
[UI_UX_RESEARCH.md](../03_research/UI_UX_RESEARCH.md), [WAVEFORM_VIEW.md](../06_ui/WAVEFORM_VIEW.md), [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §6.
