# Waveform View (dual lanes, visual difference model, tempo view, warp-map view)

> **Status:** UI spec v1.0, 2026-10-10, aligned with `docs/10_implementation/DECISION_SUMMARY.md` (authoritative on conflicts). Anything beyond the MVP is marked **Post-MVP**. Evidence and alternatives: `docs/03_research/UI_UX_RESEARCH.md` (T1-T6). Warp maths: `docs/04_algorithms/WARP_FUNCTION.md`; data: `docs/04_algorithms/WARP_MAP.md`.

Notation: `s` source seconds, `t` target (output) seconds, `W(s) = t`, `W⁻¹(t) = s` (monotonic, provided by the shared WarpMap module; the UI never re-implements interpolation). `S_i` detected beat (`srcDetectedSec`), `x_i` anchor used (`srcAnchorSec`), `T_k` grid slot (`tgtSec`). `g` global ratio, `r` local ratio, `r_loc = r/g`.

## 1. Structure

```
 ┌ lane header ┐┌──────────────────── canvas ─────────────────────────────┐
 │ ORIGINAL     ││ ruler (source seconds)                       16 px     │
 │ source time  ││ marker rail (detected beats ◆, downbeats ▼)  12 px     │
 │              ││ waveform (neutral)                           ≥ 96 px   │
 │              ││ gutter: connectors                           28 px     │
 │ CORRECTED    ││ marker rail (landed beats, grid labels)      12 px     │
 │ target time  ││ waveform (class-tinted)                      ≥ 96 px   │
 │ [Estimate]   ││ correction strip (signed bars)               16 px     │
 │              ││ ruler (Bars.Beats + target seconds)          16 px     │
 └──────────────┘└────────────────────────────────────────────────────────┘
 Overview strip (separate widget above): whole song, 40 px.
```

## 2. Shared timeline model (decision)

Each lane is drawn in its **own native time**; they are linked through `W` with a **beat-normalised shared zoom** and a **centre anchor**.

- Corrected lane (master): `x = (t − t_view0) · Z` with `Z` px per target second. Its ruler shows Bars.Beats (the target grid is regular) and target seconds.
- Original lane: `x = (s − s_view0) · g·Z`. Using `g·Z` makes one beat the same width in both lanes on average, so only *local* deviation shows as connector slant even when the target tempo differs from the detected one.
- Anchor: the view centre `x_c`. `t_c = t_view0 + x_c/Z`, `s_c = W⁻¹(t_c)`, `s_view0 = s_c − x_c/(g·Z)`. Both lanes therefore show the same musical moment at the centre. Deterministic: the anchor never switches (no jumps when playback starts/stops).
- `g = (T_last − T_first)/(x_last − x_first)` over anchored span (WARP_MAP.md anchors). When |ln g| < ln 1.005 the lanes are drawn at identical scale.
- Out-of-range: original lane shows background beyond source ends; no extrapolated audio.
- **Post-MVP:** "Absolute time" toggle (x = seconds in both lanes) for literal drift-from-start inspection.

Why not shared absolute seconds: a 120 → 128 BPM target displaces beat 600 by 18.75 s, so the lanes become unrelated (research T6).

## 3. Markers, grid and connectors

| Element | Lane | Visual | Encodes |
|---|---|---|---|
| Detected beat | Original rail + 1 px line over waveform | ◆ head in rail; line style by beat confidence: solid ≥ High, dashed Medium, dotted Low | `srcDetectedSec`, `confidence` |
| Inferred beat (gap repair) | Original | hollow ◇, dotted | `inferred = true` |
| Downbeat | both rails | ▼ head, 2 px line, bar number label | `isDownbeat`, `barIndex` |
| Anchor used (if ≠ detected by > 2 px) | Original | small tick at `x_i` joined to ◆ by a 1 px horizontal line | Natural smoothing offset `s_i − x_i` |
| Landed beat | Corrected rail | ◆ at `W(S_i)` | where the beat lands |
| Target grid | Corrected lane full height | beat lines 10 % alpha, bar lines 22 % alpha; labels in ruler | `T_k` |
| Grid residual | Corrected rail | when landed ◆ is ≥ 2 px from grid line, a short bracket between them | `e_out,i` |
| Connector | Gutter | line from (x_top(S_i), gutter top) to (x_bot(W(S_i)), gutter bottom); colour = max correctionClass of the two adjacent segments; width 1 px beats, 2 px downbeats | residual displacement |
| Pre-roll / tail (outside anchors) | Corrected | 45° light hatch, label "Outside beat grid" on hover | edge ratios (TARGET_BEAT_GRID.md §5) |
| Phase reference (bar 1) | both | ▼ with "1" in a filled tag | downbeat shift edit |

Level of detail (by pixels per beat `ppb = Z · 60/B`):

| ppb | Beats | Bars | Labels | Connectors |
|---|---|---|---|---|
| ≥ 24 | all | all | `bar.beat` at downbeats, beat numbers on hover | all beats |
| 6 - 24 | all, no heads | all | bar numbers | downbeats only |
| < 6 and px/bar ≥ 6 | hidden | lines | every 2ⁿ-th bar so labels ≥ 48 px apart | hidden; gutter becomes an aggregated colour band (max class per column) |
| px/bar < 6 | hidden | hidden | every 2ⁿ-th bar | aggregated band |

Marker heads live in rails on `bg` so they meet 3:1 regardless of the waveform under them (UI_UX_PRINCIPLES.md §4).

**Interaction (MVP):** markers are not dragged in MVP (manual anchors are Post-MVP, WARP_MAP.md). MVP edits: Set bar 1 at cursor (moves phase reference to the nearest detected beat at the cursor), ½/×2 interpretation, meter. Hovering near a detected beat (± 6 px) highlights it and its connector.

**Post-MVP warp-marker editing:** select beat (click / arrows), drag horizontally on the original rail to move the anchor (snaps to nearest onset when within 8 px, Alt disables snap), double-click empty rail to insert, Delete to remove, "Pseudo-markers" at strong onsets shown on hover (Ableton-like). WCAG 2.5.7 single-pointer alternative: inspector buttons `◀ 1 ms ▶`, `◀ 10 ms ▶`, `Snap to onset`, `Move to cursor`. Keyboard: Enter grabs the selected marker, ←/→ nudge 1 ms (Shift 10 ms), Enter commits, Esc cancels. Each committed move = one undo step.

## 4. Waveform rendering

- Data: per-lane float32 `(min, max, rms)` pyramid, base 64 samples/px, factor 4 (64 … 65536); ~14.4 MB for a 10-min 48 kHz stereo lane. Built incrementally during decode (original) and per completed render chunk (corrected). Below 64 spp the lane reads decoded PCM; below 2 spp it draws a sample polyline (original lane only, and corrected lane only once rendered).
- Column computation: integer sample ranges from a view start snapped to whole columns (`round(view_start/spp)·spp`) so scrolling never makes peaks shimmer.
- Draw per device-pixel column: min..max line in lane colour, ±rms band in a 20 % darker/lighter tone. Stereo: combined envelope (min of mins, max of maxes, rms = √((rmsL²+rmsR²)/2)); split L/R is Post-MVP.
- Full redraw per frame from the pyramid (CPU); budget target ≤ 4 ms per lane at 2560 px (to be benchmarked, PERFORMANCE_BENCHMARKS.md). GPU/tile cache only if profiling fails the budget (Post-MVP).
- Predicted corrected lane: per column, query the source pyramid over `[W⁻¹(t_a), W⁻¹(t_b)]` (research T3); drawn at 60 % opacity with an `Estimate` lane badge; envelope only (no sample-level drawing). Replaced chunk-by-chunk by real render peaks; spans being re-rendered show a static 45° hatch overlay (no animated shimmer).
- Amplitude: linear, lane-fitted to 0 dBFS = lane half-height. dB scale is Post-MVP.

### Zoom and scroll

| Input | Action |
|---|---|
| Ctrl + wheel / pinch | zoom ×1.25 per notch (continuous for precision devices), anchored at pointer x |
| wheel (vertical) or Shift + wheel, trackpad horizontal | scroll horizontally, 10 % of view width per notch |
| `Ctrl +` / `Ctrl −` | zoom ×2 / ÷2 anchored at playhead if visible else centre |
| `Ctrl 0` / Fit button | fit whole song |
| `Z` | zoom to selection |
| Drag viewport box in overview | scroll; drag its edges = zoom |
Zoom limits: from fit-whole-song to 4 samples/px. Zoom and scroll are not undoable.

## 5. Visual difference model

### 5.1 Quantities
For any display unit (pixel column, beat segment, region) covering target span `[t_a, t_b]`:
```
s_a = W⁻¹(t_a), s_b = W⁻¹(t_b)
r     = (t_b − t_a)/(s_b − s_a)            # >1 slowed (stretched), <1 sped up (compressed)
r_loc = r / g                              # remove deliberate global tempo change
d     = |ln r_loc| ;  p = 100·(e^d − 1)    # symmetric percent
dir   = sign(ln r_loc)                     # + slowed, − sped up
```
"Color by" (lane header menu): **Timing correction** (`r_loc`, default) or **Total stretch** (`r`, engine workload). The toggle is hidden when |ln g| < ln 1.005.

### 5.2 Classes (default thresholds; stored as `correctionClass` in WARP_MAP.md)

| Class | p | `|log2 r_loc|` | Corrected-lane waveform | Strip bar height | Name |
|---|---|---|---|---|---|
| 0 | < 0.5 % | < 0.00720 | `wave-neutral` grey | 0 (baseline tick only) | Unchanged |
| 1 | 0.5 - 2 % | 0.00720 - 0.02857 | 33 % mix toward hue | 25 % | Subtle |
| 2 | 2 - 5 % | 0.02857 - 0.07039 | 66 % mix | 55 % | Moderate |
| 3 | ≥ 5 % | ≥ 0.07039 | 100 % hue | 85 % | Strong |
| flag `extreme` | ≥ 12 % | ≥ 0.16349 | 100 % hue + ⚠ glyph in rail | 100 % + cap | Strong + "Check" (issue list entry) |

Hue: slowed `stretch` blue, sped up `compress` orange (tokens in UI_UX_PRINCIPLES.md §4). The strip draws bars **upward for slowed, downward for sped up**, so direction and magnitude survive greyscale, colour-blindness and Windows contrast themes. Within the waveform, the original lane stays neutral (it is unchanged by definition).

Note for WARP_MAP.md consistency: its field note says `|log2 r|`; this spec classifies `r_loc = r/g`. Identical when g ≈ 1; flagged for DECISION_SUMMARY.

Aggregation when one pixel spans several segments (zoomed out, overview): use the **max** class and the direction of the max-|d| segment, so isolated spikes stay visible; hover reports the range.

"Bit-exact passthrough" (r == 1 rendered without processing) is shown as class 0 plus an `=` glyph in the hover card only if the engine guarantees it (open question).

### 5.3 Processing mode
If the resolved segment `mode` differs across the song (**Post-MVP**: Auto mode or Transient), a 6 px mode band under the strip shows the mode by pattern: Smooth = solid, Transient = vertical ticks (|||), others Post-MVP; a text badge ("Transient") at each mode change when space ≥ 64 px. MVP renders a single engine with smooth/transient only (WARP_MAP.md), so the band is hidden when only one mode is used. Per-region mode override is Post-MVP.

### 5.4 Why configurable, and how to validate

Why: (a) the JND literature is on click sequences, not music: step tempo JND ≈ 1.5-6 % depending on number of intervals (Drake & Botte 1993); single-event displacement ≈ 5 % above 240 ms IOI, ≈ 10 ms below (Friberg & Sundberg 1995); continuous drift JND ≈ 0.27 %/interval (Dahl & Granqvist 2003); (b) individual sensitivity varies (musicians lower); (c) Weber's law breaks down below ~240 ms IOI, so at fast tempi or subdivisions a ms rule may be needed; (d) user goal differs (DAW alignment vs casual listening). Class 0 is a display floor, not an audibility claim; cumulative displacement is reported separately in ms.

Settings → Display → Correction classes: presets *Sensitive* (0.3 / 1 / 3 %), *Default* (0.5 / 2 / 5 %), *Relaxed* (1 / 3 / 8 %), *Custom* (three numeric edges, validated strictly increasing, 0.1-20 %), extreme flag edge (default 12 %). Per-user setting, not per project (it changes display only, never audio).

Validation plan (owner: SUBJECTIVE_LISTENING_TESTS.md):
1. **Perceptual calibration.** Stimuli: 20 AI-generated songs (benchmark corpus), insert local tempo deviations rendered with the chosen engine at p ∈ {0.25, 0.5, 1, 2, 3, 5, 8, 12} % over spans of 1, 2, 4 and 8 beats. Task: 2AFC "which version has a timing irregularity", adaptive 2-down/1-up staircase (70.7 % point), ≥ 12 listeners split producers / non-musicians. Set class 1/2 edge = median threshold for 4-beat spans; class 2/3 edge = 75th-percentile threshold for 1-beat spans. Report tempo dependence (≤ 90, 90-130, ≥ 130 BPM).
2. **Comprehension test.** 8 participants, 5 tasks ("find the most-changed bar", "is bar 32 slowed or sped up", "which parts are unchanged") with the real UI; success ≥ 90 %, median time ≤ 10 s; also run with CVD simulation (protan, deutan, tritan) and greyscale screenshots; all tasks must remain solvable via strip direction/height and hover text.
3. **Noise-floor check.** On stable-tempo tracks in Strict mode, class 1+ coverage must be explained by detector timing uncertainty; class-0 edge must be ≥ the analyser's reported anchor uncertainty (BEAT_TRACKING.md).

## 6. Hover card (region inspection)

Delay 300 ms, hide 100 ms after leave, never covers the pointer column; click pins it into the Inspector. Content for the beat segment under the pointer:
```
 Bar 21 · beat 3 → 4                 Natural (2 bars)
 Original  0:41.226 → 0:41.731  (505 ms)
 Corrected 0:41.250 → 0:41.750  (500 ms)
 Sped up 1.0 %  ▸◂   (Total stretch 1.1 %)
 Beat lands 2 ms after grid · moved +24 ms
 Local tempo 118.8 → 120.0 BPM
 Confidence: High · Mode: Smooth
```
Accessible: the same text is exposed as the accessible description of the focused beat when navigating by keyboard (§10).

## 7. Tempo view (Advanced panel, tab "Tempo")

- Shares the corrected lane's horizontal mapping (target time), 120 px high; a vertical cursor line follows the hover/playhead.
- Series: raw per-beat BPM `60/(S_{i+1}−S_i)` as dots at `W(S_i)` (opacity by confidence; inferred beats hollow); smoothed local tempo (`localBpm`, line); target BPM (dashed horizontal line); detected global BPM (thin line, label); ±2 % band around target as a faint reference ("about the tempo-change JND", tooltip).
- Y range: fit 2nd-98th percentile of raw BPM, minimum span ±5 % of target; outliers clamped to the edge with ▲/▼ glyphs (octave errors would otherwise flatten the plot).
- Stability readout in the panel header: category + `max drift X ms vs constant fit`, `IBI CV Y %` (definitions in TEMPO_STABILITY_ANALYSIS.md, pending).
- Hover on a dot: beat index, raw BPM, smoothed BPM, confidence.

## 8. Warp-map view (Advanced panel, tab "Warp")

Plotting `W` itself is useless (near-diagonal line). Plot the **displacement** instead:
```
D(t) = t − g·W⁻¹(t)     (ms; time shift vs a pure global tempo change)
```
- x: target time (shared mapping), y: D in ms, symmetric auto range, zero line.
- Dots at anchors: detected (`S_i` mapped) vs used (`x_i`), showing what Natural smoothing kept as groove.
- Shaded area under the curve coloured by class of the local slope (same classes as §5).
- Header: `N_c`, λ, `maxAbsShiftSec`, `ratioMin`/`ratioMax` from `diagnostics` (WARP_MAP.md).

## 9. Region list (Advanced panel, tab "Regions") — non-visual equivalent

A table (keyboard-navigable grid) with one row per bar (default) or per beat (toggle): Bar, Start (orig), Start (corrected), max slowed %, max sped up %, class, min confidence, mode, flags. Sortable by any column; Enter on a row jumps there and selects the bar. This is the screen-reader equivalent of the canvas and also the fastest "worst regions first" view (sort by max %).

## 10. Accessibility of the canvas

- The waveform view is one focusable composite (role: application/"group" with a label "Waveform view, original and corrected"). Inside, a virtual cursor moves by beat (←/→), bar (Ctrl+←/→), issue (F8). Each move updates a polite live region with the hover-card text in one sentence ("Bar 21 beat 3, sped up 1 percent, high confidence"). Continuous playback position is never auto-announced; `Ctrl+Shift+T` (or the time display) speaks it on demand.
- Focus ring around the whole canvas (2 px, ≥ 3:1) and a visible cursor line (2 px) for the virtual cursor (WCAG 2.4.7, 2.4.11).
- All pointer-only gestures have keyboard equivalents (KEYBOARD_SHORTCUTS.md) and single-pointer equivalents (buttons) per WCAG 2.5.7.
- Reduced motion: follow mode defaults to Page; no fades; render progress shown as static hatch + percentage.
- Forced colours: waveform in `CanvasText`, grid in `GrayText`, classes via strip height + hatch density, markers in `Highlight`.
