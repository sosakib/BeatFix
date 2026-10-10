# UI/UX & Visualization Research (Agent F)

Status: research input, 2026-10-08. Engine capabilities referenced here are pending final decisions in `docs/10_implementation/DECISION_SUMMARY.md`. Specs derived from this file live in `docs/06_ui/`.

## Objective

Establish evidence-backed answers for: (1) efficient waveform rendering for long files, including a cheap *predicted* corrected waveform; (2) the dual synchronized original/corrected view and its time-axis model; (3) a quantitative visual difference model (stretch ratio -> colour) with perceptual justification and a validation plan; (4) tempo/confidence/marker/inspection visuals; (5) main-window workflow, states, accessibility and shortcuts for a single-purpose tool.

## Why It Matters

- The product promise ("see original vs corrected timing, see where and how much it changed") is visual. If the view lies (wrong mapping, noisy colours, misleading greys), users either distrust the correction or ship bad audio.
- Auto beat detection is wrong often enough (half/double tempo, downbeat errors; baseline log: SMC blind spot paper, Logic/Melodyne/SOS) that the UI must make verification and correction fast. The UI is the error-recovery path of the algorithm.
- 10-minute files at 48 kHz are ~29 M frames/channel; naive per-frame drawing is not viable, but a peak pyramid makes it trivial.

## Key Findings

| # | Claim | Evidence | Conf. | Implication / Recommendation |
|---|---|---|---|---|
| F1 | Industry waveform tools precompute min/max (optionally RMS) summaries at multiple samples-per-pixel (spp) levels and pick a level per zoom. | audiowaveform `.dat`: header (version, flags 8/16-bit, sample_rate, samples_per_pixel, length, channels v2) then interleaved min/max per channel ([DataFormat.md](https://github.com/bbc/audiowaveform/blob/master/doc/DataFormat.md)); peaks.js zoom in spp, discrete `zoomLevels` e.g. [512..4096], `setZoom({scale})` (Context7 `/bbc/peaks.js`); Audacity keeps `(min,max,rms)` summaries at 256 and 65536 samples, chooses divisor 65536/256/1 by spp ([doxy.audacityteam.org](https://doxy.audacityteam.org/_get_wave_display_8cpp_source.html)); REAPER `.reapeaks`: int16 max/min pairs per channel, up to 16 mipmaps, v7 writes ~400/s, ~10/s, ~1/s levels; `RPKL` variant log-encodes float ranges up to +-8.0 ([reapeaks.txt](https://www.reaper.fm/sdk/reapeaks.txt)) | High | Use an in-memory min/max/RMS pyramid; below the base level draw from PCM. |
| F2 | For a 10-min 48 kHz stereo file, a full pyramid (base 64 spp, factor 4, 6 levels) holds 599,855 entries/channel: 4.8 MB as int16 min/max, 7.2 MB with int16 RMS, 14.4 MB as float32 (min,max,rms). Raw float32 PCM is 230.4 MB. | computed (see Technical Details) | High | Memory is a non-issue; prefer float32 entries (no clipping of >0 dBFS float material, which REAPER needed `RPKL` for). |
| F3 | Per-frame draw cost with the pyramid is O(width x channels x <=factor): ~2 lanes x 2560 px x 2 ch x 4 = ~41k min/max ops. | arithmetic | High | CPU drawing comfortably fits a 16.7 ms frame; GPU path not required for MVP. Benchmark to confirm (Agent G). |
| F4 | A monotonic warp makes target-time intervals map to source-time intervals, so the corrected waveform's envelope can be *predicted* by querying the source pyramid over `[W^-1(t_a), W^-1(t_b)]` per output pixel column. | follows from monotonicity required in brief s7.5; TSM preserves local amplitude envelope approximately (assumption, verify against renders) | High (math) / Med (visual fidelity) | Show a predicted bottom lane within one frame of any edit; replace with real peaks as render chunks complete. Mark it visually as "estimate". |
| F5 | Tempo discrimination JND for isochronous click sequences is ~1.5% at IOI 300-800 ms and falls with number of intervals: 2 events 6%, 3: 4%, 5: 3.2%, 7: 3%. | Drake & Botte 1993, Perception & Psychophysics, "Tempo sensitivity in auditory sequences: Evidence for a multiple-look model" ([Springer](https://link.springer.com/article/10.3758/BF03205262); numbers via search summary, abstract not fetched directly) | Med | A 2% bin edge sits at the lower end of step-change detectability; 5% is clearly detectable even for short spans. |
| F6 | JND for displacing one tone in an isochronous sequence: ~10 ms for IOI < ~240 ms, ~5% of IOI above that. | Friberg & Sundberg, ASA 126th meeting abstract ([auditory.org](https://auditory.org/asamtgs/asa93dnv/4pPP/4pPP2.html)); full paper JASA 98:2524-2531 (1995) | High (abstract is primary) | Single-beat corrections >5% are audible timing changes; Weber's law breaks at fast IOIs, so thresholds at very high BPM/subdivision should switch to ms. The prompt's "~2-5%" range is consistent; values like "2.5%" for this paper are **UNVERIFIED**. |
| F7 | Continuous tempo *drift* JND is far smaller: mean 0.27% of nominal IOI per interval (1.35 ms at 500 ms IOI), 7 subjects, PEST, click sequences. Authors note this is small vs earlier ~2% estimates and cite Madison (~0.17% for longer sequences; **UNVERIFIED**, secondary). | Dahl & Granqvist 2003, Ann. N.Y. Acad. Sci. 999:161-165 ([preprint PDF](https://vbn.aau.dk/ws/portalfiles/portal/65338357/DahlGranqvistVenezia_proc3subm_openacc.pdf), text extracted) | High (primary) | Per-beat corrections of 0.3-0.5% are individually inaudible as "change" but are exactly what fixes audible drift. The grey floor is a *display noise floor*, not "inaudible". Cumulative displacement (ms) must be shown separately. |
| F8 | All JND data above are for isolated click sequences, not polyphonic music. | same sources | High | Music thresholds are likely higher (texture, expressive-timing expectations): **Assumption**. Bins must be configurable and calibrated by listening tests. |
| F9 | WCAG 2.2 adds 2.4.11 Focus Not Obscured (AA), 2.5.7 Dragging Movements (AA, single-pointer alternative to any drag), 2.5.8 Target Size Minimum (AA, 24x24 CSS px or spacing), 2.4.13 Focus Appearance (AAA); 4.1.1 Parsing removed. | [W3C What's New in 2.2](https://www.w3.org/WAI/standards-guidelines/wcag/new-in-22/) | High | Warp/beat-marker dragging needs a non-drag pointer alternative and keyboard path. Dense beat markers rely on the "equivalent control" exception (inspector nudge buttons). |
| F10 | Windows "Animation effects" (SPI_GETCLIENTAREAANIMATION) off => Chromium/WebView2 reports `prefers-reduced-motion: reduce`. | [Chromium animation_win.cc](https://chromium.googlesource.com/chromium/src.git/+/HEAD/ui/gfx/animation/animation_win.cc), [Vispero note](https://vispero.com/resources/short-note-on-prefers-reduced-motion-and-puzzled-windows-users/) | High | Single OS signal for reduced motion regardless of UI stack: read SPI directly (native) or the media query (WebView2). |
| F11 | Okabe-Ito CUD palette: #E69F00 orange, #56B4E9 sky blue, #009E73 bluish green, #F0E442 yellow, #0072B2 blue, #D55E00 vermillion, #CC79A7 reddish purple, #000000. | [conceptviz reference](https://conceptviz.app/blog/okabe-ito-palette-hex-codes-complete-reference), [Siegal lab](https://siegal.bio.nyu.edu/color-palette/) | High | Blue (stretch/slowed) vs orange (compress/sped up) diverging pair. In dark theme they have equal luminance (contrast 1.02:1), so direction must also be encoded non-chromatically (signed bar height, glyph). |
| F12 | Ableton warp UX: double-click adds marker; grey *pseudo-warp markers* appear on hover over transients and convert on drag; "Set 1.1.1 Here"; "Warp From Here" variants; Quantize with Amount %; warp modes named by material (Beats, Tones, Texture, Re-Pitch, Complex, Complex Pro). | [Ableton manual ch. Audio Clips, Tempo, and Warping](https://www.ableton.com/en/manual/audio-clips-tempo-and-warping/) | High | Adopt: snap-candidate markers on hover, "Set bar 1 here", a strength amount (our Natural<->Strict), material-named modes. |
| F13 | FL Studio NewTime: single waveform editor with Warp, Shift, Groove (yellow) and Downbeat markers; auto tempo detection places markers; manual documents no stretch-intensity colouring. | [Image-Line NewTime manual](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Newtime.htm) | Med (manual summary) | Differentiator: dual-lane before/after with quantified intensity colouring and connector lines. Functional study only; no internals. |
| F14 | REAPER stretch markers expose playrate numerically, editable via drag beside the marker / properties dialog. | [SOS "Stretch Time"](https://www.soundonsound.com/techniques/stretch-time), search summary | Med | Show numeric local rate in hover/inspector; do not rely on colour alone. |
| F15 | Logic Smart Tempo Editor (Hint vs Beat-Marker modes, half/double shortcuts), Melodyne Tempo Editor (Assign vs Edit, make tempo constant), rekordbox dynamic grid, Serato 128-marker limit. | baseline log (`_baseline_research_log.md`) | Med | Half/double must be one keystroke; separate "interpretation" (what the beat is) from "target" (what tempo to produce). |

## Technical Details

### T1. Peak pyramid

Data per level L (spp_L = 64 * 4^L, L = 0..5 => 64, 256, 1024, 4096, 16384, 65536), per channel, per bin b covering samples [b*spp_L, (b+1)*spp_L):
```
min_L[b] = min(x), max_L[b] = max(x), rms_L[b] = sqrt(mean(x^2))   (float32)
Parent from 4 children: min = min(children.min), max = max(children.max),
rms = sqrt(mean(children.rms^2))   (exact when children are equal size; last bin weighted by sample count)
```
Sizes for a 10-min, 48 kHz, stereo source (N = 28,800,000 frames/ch):

| Scheme | Levels | Entries/ch | int8 min/max | int16 min/max | int16 min/max/rms | f32 min/max/rms |
|---|---|---|---|---|---|---|
| base 64, x4 | 6 | 599,855 | 2.4 MB | 4.8 MB | 7.2 MB | 14.4 MB |
| base 64, x2 | 11 | 899,563 | 3.6 MB | 7.2 MB | 10.8 MB | 21.6 MB |
| base 256, x4 (Audacity-like) | 5 | 149,855 | 0.6 MB | 1.2 MB | 1.8 MB | 3.6 MB |
| base 32, x4 | 6 | 1,199,708 | 4.8 MB | 9.6 MB | 14.4 MB | 28.8 MB |

(stereo totals; raw PCM: float32 230.4 MB, int16 115.2 MB). Corrected render adds the same again. Pyramid build is one linear pass (O(N)); it can be built incrementally during decode and during chunked render.

Choice: base 64 x4 float32 (14.4 MB/10 min stereo per lane). Factor 4 bounds per-column reduction to <=4 bins; float32 avoids quantisation/over-range handling. Below 64 spp, read the decoded PCM directly (min/max per column; at spp < ~2 draw a polyline through samples).

Level selection for a column of span `n` samples: largest L with spp_L <= n. Column boundaries must be computed in integer samples from a view start snapped to whole columns (`view_start_samples = round(view_start / spp_view) * spp_view`) so a scroll by k pixels shifts columns exactly by k, and bin assignments do not "crawl" (shimmer) during scroll.

### T2. Stereo display
Default "combined" envelope: `min = min(minL,minR)`, `max = max(maxL,maxR)`, `rms = sqrt((rmsL^2+rmsR^2)/2)`. Split L/R lanes are Post-MVP. Draw: 1-px vertical line min..max per physical pixel column (HiDPI: use device pixels), plus inner RMS band (+-rms) in a stronger tone.

### T3. Predicted (warped) waveform before render
```
predict_column(t_a, t_b):                      # target-time span of one pixel column
  s_a = W_inv(t_a); s_b = W_inv(t_b)           # monotonic => s_a < s_b
  return pyramid_source.query(s_a*fs, s_b*fs)  # min, max, rms over that source span
r_column = (t_b - t_a) / (s_b - s_a)           # local stretch ratio, reused for colour (T5)
```
- W_inv evaluated with a forward-moving pointer over the anchor table while iterating columns left->right: O(width + anchors_in_view) per frame.
- `query` uses the largest level with spp_L <= span/4 for interior bins, so edge partial-bin error is <=25% of a column; acceptable for display.
- Limits: amplitude peaks of the real render differ slightly (phase-vocoder phase changes alter sample peaks; transients may smear), and at sample-level zoom the predicted *shape* is meaningless. Therefore: draw predictions only as envelope, at reduced opacity, labelled "estimate"; disable sample-level drawing on the corrected lane until rendered.
- Real-render peaks replace predictions per completed render chunk.

### T4. GPU vs CPU
- CPU: rasterise column lines into a lane bitmap (or issue per-column rect fills), composite via the toolkit's accelerated path. Cost ~41k reductions + ~10k line fills per frame: well within budget. Zero shader/driver risk (Windows GPU driver variance is a real support cost).
- GPU: upload pyramid level as texture; fragment shader picks min/max per column. Gains only matter at 4K+ widths with many lanes; adds context-loss handling, driver bugs.
- Decision: CPU for MVP; full redraw from pyramid each frame (no tile cache). Post-MVP: tile cache or GPU only if profiling shows >8 ms/frame on the reference machine (Agent G to define). UI-stack-specific API (JUCE Graphics/OpenGL vs WebView2 Canvas2D/WebGL) is pending the stack decision; this design is stack-neutral.
- 60 fps zoom: because a full redraw is cheap, each wheel/pinch frame recomputes directly; no bitmap scaling trick needed. Anchor zoom at pointer x: `t_anchor` stays under the pointer.

### T5. Visual difference model (quantitative)
Global ratio over the anchored span: `g = (T_last - T_first) / (S_last - S_first)` (overall tempo change; g > 1 output longer = slowed).
Local ratio per display unit (pixel column, beat interval or region): `r = dT/dS` from T3.
Residual (local) ratio: `r_loc = r / g`.
Deviation: `d = |ln r_loc|`; display percent `p = 100 * (exp(d) - 1)` (symmetric: x1.05 and /1.05 both 5%).
Direction: `sign(ln r_loc)`: + = stretched (slowed locally), - = compressed (sped up locally).

Default classes (configurable, see validation). Stored in the warp map as `correctionClass` 0-3 (WARP_MAP.md); "Extreme" is a flag, not a class:

| correctionClass | p range | |log2 r_loc| range | Name | Rationale |
|---|---|---|---|---|
| 0 | p < 0.5% | < 0.00720 | Unchanged (grey) | display noise floor; below typical per-beat detector jitter after smoothing; not a claim of inaudibility (F7) |
| 1 | 0.5-2% | 0.00720-0.02857 | Subtle | below step-tempo JND for multi-interval sequences (F5 1.5-3%) |
| 2 | 2-5% | 0.02857-0.07039 | Moderate | spans step-tempo JND range (F5) up to single-displacement JND (F6 ~5%) |
| 3 | >= 5% | >= 0.07039 | Strong | above single-event JND; clearly audible timing change |
| flag `extreme` | >= 12% | >= 0.16349 | "Check" marker on top of class 3 | rarely legitimate inside an AI-generated song; usually a missed/extra beat (r near 0.5 or 2 is a classic octave error) |

Consistency note: WARP_MAP.md field notes say `|log2 r|`; this research recommends classifying the residual `r_loc = r/g` so a deliberate global tempo change does not paint the whole song "strong". When |ln g| < ln 1.005 the two are identical. Flagged for DECISION_SUMMARY.

Colour by "Timing correction" (`r_loc`, default) or "Total stretch" (`r`, shows the engine's workload, useful for quality risk). When |ln g| < ln 1.005, both are identical and the toggle is hidden.

Important interaction with detection jitter: beat trackers at 50-100 frames/s quantise beats to 10-20 ms unless refined; 10 ms on a 500 ms IBI is 2% per interval. In a fully Strict warp this jitter becomes real class 1-2 correction speckle. This is an argument for a smoothed default (Natural (N_c = 8, WARP_FUNCTION.md)) and for colouring from the actual W (truthful) rather than idealised values. Algorithm agents must state anchor timing uncertainty so the UI floor can be set above it.

### T6. Time-axis model for dual lanes (decision)
Options:
1. Shared absolute seconds (x = t in both lanes). Fails when target != detected tempo: a 120 -> 128 BPM change displaces beat 600 by 18.75 s; lanes become unrelated.
2. Both lanes in target time (original drawn warped). Destroys the "original" view.
3. **Each lane in its own native time, linked through W, with musical (beat-normalised) shared zoom.** Bottom lane: target seconds at Z px/s (target grid is regular, so the master ruler is Bars.Beats). Top lane: source seconds at `g*Z` px/s so one beat has the same pixel width in both lanes on average. Horizontal link: the view centre is the alignment anchor: `s_centre = W_inv(t_centre)`. Connector lines in the gutter between lanes join each detected beat `S_k` (top) to where it lands `W(S_k)` (bottom); their slant shows only residual displacement relative to the anchor.
Pick: option 3 (MVP). Option 1 as a Post-MVP "Absolute time" toggle for users who want literal drift-from-start. Tradeoff: top-lane seconds ruler has non-round tick spacing when g != 1; mitigated by labelled ticks.

### T7. A/B position mapping
Switching corrected -> original at target time t: seek to `s = W_inv(t)`; original -> corrected: `t = W(s)`. Pre-roll before the first anchor and tail after the last are mapped by the engine's boundary policy (pending; UI uses the same W function the renderer uses, never a UI-side approximation). A short equal-power crossfade (default 20 ms, pending audio-engine decision) hides the discontinuity.

## Alternatives Considered

- **Continuous colour ramp vs stepped bins**: continuous ramps are not legend-readable, have no WCAG-checkable steps and imply false precision. Stepped bins + exact numbers on hover chosen.
- **Hue for magnitude (green->red)**: CVD-unsafe and semantically loaded ("red = bad" while stretch is the intended operation). Rejected.
- **Colour the original lane**: the original is by definition unchanged; tinting it confused the "grey = unchanged" rule. Top lane stays neutral; correction is shown on the corrected lane, gutter and strip.
- **Per-sample warped rendering before render** (actually time-stretching for display): expensive and unnecessary; envelope prediction suffices.
- **Discrete zoom levels only (peaks.js style)**: simpler but feels steppy; continuous zoom is cheap with pyramid + PCM fallback.
- **Persisting peaks to disk (.reapeaks style)**: unnecessary when the decoded source is in memory and the pyramid builds in one pass; revisit if decode-on-open is slow (Agent H cache decision).

## Evidence / Sources

- BBC audiowaveform data format: https://github.com/bbc/audiowaveform/blob/master/doc/DataFormat.md
- peaks.js API (Context7 `/bbc/peaks.js`, doc/API.md): zoom in samples-per-pixel, `zoomLevels`, `setZoom`.
- Audacity GetWaveDisplay/summaries: https://doxy.audacityteam.org/_get_wave_display_8cpp_source.html , https://doxy.audacityteam.org/class_sqlite_sample_block.html
- REAPER peaks format: https://www.reaper.fm/sdk/reapeaks.txt
- Drake & Botte (1993): https://link.springer.com/article/10.3758/BF03205262
- Friberg & Sundberg abstract: https://auditory.org/asamtgs/asa93dnv/4pPP/4pPP2.html ; JASA 98(5):2524-2531 (1995)
- Dahl & Granqvist (2003): https://vbn.aau.dk/ws/portalfiles/portal/65338357/DahlGranqvistVenezia_proc3subm_openacc.pdf
- Repp on Weber's law breakdown < 200-300 ms: https://auditory.org/postings/2007/341.html
- WCAG 2.2 new criteria: https://www.w3.org/WAI/standards-guidelines/wcag/new-in-22/
- Reduced motion on Windows: https://chromium.googlesource.com/chromium/src.git/+/HEAD/ui/gfx/animation/animation_win.cc
- Okabe-Ito palette: https://conceptviz.app/blog/okabe-ito-palette-hex-codes-complete-reference
- Ableton Live manual (warping): https://www.ableton.com/en/manual/audio-clips-tempo-and-warping/
- FL Studio NewTime manual: https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Newtime.htm
- REAPER stretch markers (SOS): https://www.soundonsound.com/techniques/stretch-time

## Recommendation

1. Float32 min/max/RMS pyramid, base 64 spp, factor 4; PCM below base; CPU drawing; full redraw per frame; column grid snapped to integer samples.
2. Predicted corrected waveform via W_inv + source pyramid, visually marked "estimate" until replaced by real render peaks.
3. Dual lanes in native times, beat-normalised shared zoom (`top px/s = g * bottom px/s`), centre-anchored link through W, connector gutter.
4. Visual difference model on `r_loc = r/g` with 4 configurable classes (edges 0.5 / 2 / 5 %) plus an "extreme" flag at 12 %, blue = stretched, orange = compressed, redundant signed-height strip and glyphs; numeric values on hover.
5. Separate "beat interpretation" (candidate, ½/x2, bar 1) from "target BPM"; Natural<->Strict as 6 detents mapping to WARP_FUNCTION.md cutoff N_c (Global, 32, 16, 8 = default Natural, 4, Strict) with live readout of the trade-off (max grid error ms vs max stretch %).
6. Accessibility: full keyboard path including marker edits; single-pointer alternatives for drags (WCAG 2.5.7); region list as non-visual equivalent of the canvas; respect reduced motion and Windows contrast themes.

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Bin thresholds feel wrong for music (JND data are click sequences) | High | Med | Configurable bins + listening calibration (UX_BEHAVIOR/WAVEFORM_VIEW validation plan) |
| Strict mode colour speckle from detector jitter | Med | Med | Default Natural (N_c = 8, WARP_FUNCTION.md); algorithm to report anchor uncertainty; floor >= uncertainty |
| Predicted waveform mistaken for final audio | Med | Med | Reduced opacity, "estimate" label, no sample-level shape |
| Top/bottom alignment confuses when g far from 1 | Low | Med | Beat-normalised zoom, readout "Overall: +6.7% faster", connectors |
| CPU drawing insufficient on 4K + low-end iGPU | Low | Low | Benchmark; GPU/tile cache Post-MVP |
| Equal-luminance hues indistinguishable in greyscale/achromatopsia | Med | Med | Signed bar direction + glyph + text |

## Open Questions

1. Does the stretch engine pass r == 1 regions through bit-exact? (affects whether class 0 can claim "bit-identical").
2. Anchor timing uncertainty of the chosen beat tracker after refinement (sets the class-0 floor).
3. Preview render strategy (realtime engine vs chunked offline) and chunk size (affects "rendering" UI states).
4. UI stack (JUCE vs Tauri/WebView2) - affects accessibility API (UIA direct vs ARIA->UIA) and drawing API only, not this design.
5. Engine boundary policy for pre-roll/tail outside the beat span.
6. Whether per-region stretch modes are in MVP (affects mode badges; currently Post-MVP).

## Implementation Implications

- Shared `WarpMap` API used by renderer, playback and UI: `W(s)`, `W_inv(t)`, anchor iteration; UI must not reimplement interpolation.
- `PeakPyramid` module: build incrementally from decode and render chunks; `query(sample_a, sample_b) -> (min,max,rms)`; ~15 MB per 10-min stereo lane.
- Visual model module: pure function `classify(r, g, settings) -> {bin, direction, p}`; unit-test bin edges and symmetry.
- Agent G: frame-time benchmark (zoom sweep, 10-min file, 2560 px), CVD simulation and greyscale checks, listening calibration of bins.
