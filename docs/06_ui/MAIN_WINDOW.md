# Main Window (incl. Analysis View)

> **Status:** UI spec v1.0, 2026-10-10, aligned with `docs/10_implementation/DECISION_SUMMARY.md` (authoritative on conflicts). Anything beyond the MVP is marked **Post-MVP**. Related: WAVEFORM_VIEW.md (lanes, colours), PLAYBACK_AND_AB.md, EXPORT_UI.md, UX_BEHAVIOR.md (state machine), KEYBOARD_SHORTCUTS.md.

## 1. Layout

Single window, single document (one song per window). Minimum size 1024×640 px; designed for 1920×1080 at 100-150 % scaling. Regions top to bottom, each a focus group reachable by `F6` / `Shift+F6`:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ ① FILE BAR   [Open…] [Save]   song.mp3 · 3:41.2 · 44.1 kHz · stereo · MP3 320k          │
│                                                   Quality: [✓ Good ▾]   [↶] [↷]  [⚙] [?] │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ ② CONTROL STRIP                                                                         │
│  DETECTED  119.84 BPM   ●●●○ High confidence   Stability: Slight drift (max 84 ms)       │
│            Beat: [½] [×2]  [More candidates ▾]   Bar 1 at 0:00.412 [Set at cursor]  4/4 ▾│
│  TARGET    [ 120.00 ] BPM  (suggested 120 · ↺)      Overall: sped up 0.1 %               │
│  CORRECT   Global ─ 8 bars ─ 4 bars ─[2 bars]─ 1 bar ─ Strict      grid err ≤ 18 ms · max ±2.4 %│
│  MODE      [Smooth ▾]   Preview quality [Fast ▾]                     [Reset corrections] │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ ③ OVERVIEW  ▁▂▅▇▅▃▂▅▇▇▆▅▃▂ (whole song, correction colour, ▲ issues, [viewport box])     │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ ④ WAVEFORM VIEW                                                                          │
│  ORIGINAL   0:40        0:41        0:42        0:43   (source seconds)   ◆ marker rail  │
│   ▁▃▇█▇▃▁ ▁▃▇█▇▃▁ ▁▃▇█▇▃▁ ▁▃▇█▇▃▁   (neutral grey)                                       │
│  ── gutter ──  │    \     \     |     /     │   (connectors detected → landed)            │
│  CORRECTED  21.1   21.2   21.3   21.4   22.1   (Bars.Beats target grid)  ◆ marker rail   │
│   ▁▃▇█▇▃▁ ▁▃▇█▇▃▁ ▁▃▇█▇▃▁ ▁▃▇█▇▃▁   (tinted by class)            [Estimate]             │
│  strip  ▔▔▁▁▁▔▔▔▔  (signed bars: up = slowed, down = sped up)                             │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ ⑤ ADVANCED ▸ (collapsed)  Tempo graph · Warp map · Region list                           │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ ⑥ TRANSPORT  [⏮] [▶] [⏹]  0:42.180 | 21.3.2   [A Original | B Corrected]  [☐ Click]      │
│             [⟲ Loop] [Follow: Page ▾]          Zoom [−] [+] [Fit]           [Export…]     │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

Rationale: decisions flow top→bottom in the order users make them (what is it → what do I want → how hard → see/hear → export). Export is bottom-right (end of reading path, Fitts: corner target). The control strip is horizontal so the waveform keeps maximum width.

## 2. Control inventory (brief §20 required controls → location)

| Required control | Location / widget | Notes |
|---|---|---|
| import/open | File bar `Open…`, drag-drop anywhere, Ctrl+O | UX_BEHAVIOR.md §2 |
| analysis progress | Waveform area overlay + file bar (§4) | cancellable |
| detected BPM | Control strip DETECTED | 2 decimals; tooltip shows uncertainty "±0.03" (BPM_ESTIMATION.md) |
| target BPM | Control strip TARGET numeric field | 2 decimals; Up/Down ±1, Shift ±0.1, Ctrl ±0.01 |
| tempo stability | DETECTED row text + tempo graph | categories from TEMPO_STABILITY_ANALYSIS.md §6; always with a number |
| confidence | DETECTED row: 4-dot meter + word + reason | dots are decorative; the word carries meaning |
| beat/grid controls | ½, ×2, candidates, Set bar 1, meter | beat-level ×2/÷2 and downbeat shift = MVP user edits (WARP_MAP.md) |
| original / corrected waveform | Waveform view lanes | WAVEFORM_VIEW.md |
| stretch mode | MODE dropdown: **Smooth** (default), **Resample** (enabled only when the correction is a single constant ratio, i.e. Global detent; shows pitch change in cents, e.g. "+31.8 cents") | Transient = Phase 1.5, Auto (per-region) = Post-MVP (TIME_STRETCH_ENGINE.md, HYBRID_AUTO_ENGINE.md) |
| quality selection | "Preview quality" Fast / High (preview only); export always uses High | Fast = Signalsmith cheaper preset, High = default preset (preset names per TIME_STRETCH_ENGINE.md §3.1) |
| play/pause, playhead, zoom, scroll | Transport + waveform | PLAYBACK_AND_AB.md, WAVEFORM_VIEW.md |
| A/B comparison | Transport segmented control | PLAYBACK_AND_AB.md |
| undo/redo | File bar ↶ ↷, Ctrl+Z / Ctrl+Y | UX_BEHAVIOR.md §11 |
| reset | "Reset corrections" | resets edit state to analysis defaults; undoable |
| render/export | Transport `Export…` | EXPORT_UI.md |
| Advanced: tempo graph, local BPM, warp map, correction %, per-region method, algorithm params | Advanced panel tabs | WAVEFORM_VIEW.md §7-9 |

## 3. Natural ↔ Strict control

Maps 1:1 to the WARP_FUNCTION.md §4.3 cutoff `N_c` (beats). Six detents, discrete slider (no continuous values; each detent is a different λ and a different render):

| Detent label (4/4) | N_c (beats) | Tooltip copy |
|---|---|---|
| Global | ∞ | One constant tempo change for the whole song. Drift is kept. |
| 8 bars | 32 | Fixes only very slow tempo wander. |
| 4 bars | 16 | Fixes slow ramps; keeps most timing feel. |
| **2 bars (Natural, default)** | **8** | Removes drift, keeps the groove. Recommended. |
| 1 bar | 4 | Tighter; corrects bar-to-bar wobble. |
| Strict | 0 (λ = 0) | Every detected beat is placed exactly on the grid. Can add jitter if detection is imperfect. |

Bar labels are computed as N_c / beatsPerBar (in 3/4: 32 beats = "10.7 bars" → label shows beats instead: "32 beats"). Live readout to the right of the slider, from the derived warp map diagnostics: `grid err ≤ X ms` (max |e_out| of anchors, WARP_FUNCTION.md §4.4) and `max ±Y %` (largest |r_loc| deviation). This makes the trade-off visible without explaining λ. Keyboard: slider focus, Left/Right moves one detent; each detent change is one undo step.

## 4. Analysis view (states of the main window)

The analysis view is not a separate screen: it is the main window in its pre-ready states, so the user never loses context.

### 4.1 Empty state
```
┌──────────────────────────────────────────────────────┐
│                                                      │
│        Drop a song here, or  [Open audio file…]      │
│        WAV · FLAC · MP3 (more formats: see help)      │
│        Ctrl+O                                         │
│                                                      │
└──────────────────────────────────────────────────────┘
```
Control strip, transport and Export are visible but disabled (shapes stay stable; no layout jump after load).

### 4.2 Loading / Analysing
```
 ORIGINAL  ▁▃▇█▇▃▁▁▃▇█▇▃▁▁▃▇█  ░░░░░░░░░░░░░░░░░░░  ← waveform fills left→right during decode
 ┌───────────────────────────────────────────────┐
 │ Analysing song.mp3                    [Cancel] │
 │ ✓ Reading file                                 │
 │ ✓ Decoding audio            3:41 of 3:41       │
 │ ● Finding beats            ▓▓▓▓▓▓▓░░░  68 %    │
 │ ○ Estimating tempo & bars                      │
 │ ○ Building correction                          │
 │ About 6 s left                                 │
 └───────────────────────────────────────────────┘
```
- Stages (ordered, each with its own determinate progress where the engine reports it, otherwise indeterminate bar): Reading file → Decoding audio → Finding beats → Estimating tempo & bars → Building correction → (background) Rendering preview.
- Overall ETA shown only after 2 s of progress data; hidden if the estimate is unstable (variance > 50 %).
- Original playback becomes available as soon as decoding completes (analysis continues in background).
- Cancel (button or Esc while the card has focus) returns to "Decoded, not analysed": waveform visible, original playable, control strip shows `[Analyse]`.
- Screen readers: stage changes announced via polite live region ("Finding beats"), percentage not announced continuously; available on demand (focus the card).

### 4.3 Ready
Control strip populated; beat markers, grid, connectors, predicted corrected lane appear in one frame (no staged animation; with motion enabled, a 150 ms fade is allowed).
Preview render starts in background around the playhead (PLAYBACK_AND_AB.md §4); the corrected lane label shows `Estimate` → `Rendering 34 %` → (label removed when complete).

### 4.4 Confidence presentation

Engine supplies `confidence ∈ {High, Medium, Low}` + reason + ranked candidates (BPM_ESTIMATION.md §6-7; thresholds calibrated there, not here).

| Level | Detected row | Candidate picker | Auto-preview | Export |
|---|---|---|---|---|
| High | "119.84 BPM · High confidence" | collapsed; ½/×2 visible | yes | enabled |
| Medium | same + a "Check:" line (⚠ icon + text) with reason, e.g. "Check: 59.9 BPM (half-time) is also plausible" + one-click chip | collapsed but alternates shown inline as chips | yes | enabled |
| Low | "~120 BPM · Low confidence" + reason | **expanded** automatically; "Play with click to compare" hint | yes, for top candidate | enabled; first export shows a confirm "Tempo wasn't confirmed. Export with 120 BPM?" [Export] [Review]; any interpretation edit or explicit "Confirm" counts as confirmation |
| None (no steady beat) | "No steady beat found" | hidden | no | disabled; message offers manual BPM entry (see UX_BEHAVIOR.md §4.4) |

Per-region low confidence (beat-level `confidence` in the warp map) is shown in the lanes (dotted markers, hatched rail) and in the Issues list, independent of the global level.

### 4.5 Ambiguous BPM candidate picker
```
 Beat interpretation                      ×
 ( ) 59.92  half-time        ███░░░░░  0.31
 (•) 119.84 detected         ████████  0.82
 ( )  79.89  2/3             █░░░░░░░  0.09
 ( ) 179.76  3/2             █░░░░░░░  0.05
 ( ) 239.68  double-time      ░░░░░░░  0.02
 [▶ Play with click]   Keep target: ☐
```
- Candidates exactly as ranked by HALF_DOUBLE_TIME_RESOLUTION.md (`B̂ × {1/3, 1/2, 2/3, 1, 3/2, 2, 3}` within 40–250 BPM), with the margin shown as Green/Amber/Red; the picker auto-expands on Amber/Red. Score bars show relative score (normalised to top = full bar) with the numeric score as text.
- Selecting a candidate is an interpretation edit (beat-level change); preview click track switches immediately so the user can listen while choosing.
- "Keep target" unchecked (default): target follows `round(candidate)`. Checked: target stays; if the resulting overall change > 15 % a warning appears (UX_BEHAVIOR.md §5).
- Radio group semantics (arrow keys move, Space selects). Esc closes.

## 5. Quality status indicator

Chip in the file bar: `✓ Good`, `⚠ Review (n)`, `✕ Problem (n)`. Icon + word + count; colour is secondary.

| Source of issue | Severity | Example text |
|---|---|---|
| Global confidence Low, unconfirmed | Review | "Tempo not confirmed" |
| Beat confidence low in a span | Review | "Uncertain beats 1:52-2:04" |
| per-segment local deviation `|r/g − 1|` ≥ 8 % | Review | "Strong correction at bar 31: listen for artifacts" |
| `extreme` flag segments (`|r/g − 1|` ≥ 12 %) | Review | "Large correction at bar 48 (likely a missed beat)" |
| Overall change `|g − 1|` ≥ 15 % | Review | "Large tempo change: quality may drop" |
| Any segment ratio outside 0.5–2.0 (hard bound) | Problem | "Correction impossible: try ½ or ×2 beat interpretation" (render refused) |
| Decode warnings (corrupt frames) | Review | "3 damaged frames skipped near 2:13" |
| Render peak over 0 dBFS (integer export) or −1 dBTP (lossy export) | Problem | "Corrected audio peaks at +0.6 dBTP": resolved in the export peak gate (EXPORT_UI.md) |
| Render failed for a span | Problem | "Couldn't render 1:20-1:24" |

Clicking opens an Issues popover: list ordered by time, each with `[Go to]` (selects the span, scrolls, sets cursor). `F8` / `Shift+F8` cycle issues from anywhere.

## 6. Region inspection

- Hover card (WAVEFORM_VIEW.md §6) for transient inspection.
- Click on a lane (no drag) sets the cursor; the Inspector line in the Advanced panel ("Region list" tab) pins the details for the beat interval under the cursor.
- Time selection (drag or Shift+arrows) shows a selection summary in the transport: `Selection 21.1-24.4 · 12 beats · slowed max 1.8 % · sped up max 0.9 % · min confidence Medium`.

## 7. Reset vs Re-analyse

- **Reset corrections**: edit state ← defaults derived from the current analysis (target = round(detected), N_c = 8, mode Smooth, interpretation edits cleared). Undoable. No re-analysis.
- **Re-analyse** (menu / ⚙): reruns analysis on the immutable source; previous analysis kept in undo history; user interpretation edits are cleared with a confirm if any exist.

## 8. Window-level behaviour

- Title: `song.mp3 – BeatFix` with `•` when the project has unsaved changes.
- Close with unsaved changes: Save / Don't save / Cancel (PROJECT_SESSION_MODEL.md §4).
- Resizing: lanes share extra height 50/50 between original and corrected; control strip wraps the MODE row under CORRECT below 1280 px width; Advanced panel max 40 % of height.
- Settings (⚙): theme, reduced motion override, correction-class thresholds (WAVEFORM_VIEW.md §5.4), A/B crossfade length, follow mode default, auto-analyse on open, phase policy default (Preserve start / DAW-ready start, TARGET_BEAT_GRID.md §3).
