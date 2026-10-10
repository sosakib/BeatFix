# UI/UX Principles

> **Status:** UI spec v1.0, 2026-10-10, aligned with `docs/10_implementation/DECISION_SUMMARY.md` (authoritative on conflicts). Anything beyond the MVP is marked **Post-MVP** and must not be read as an engine commitment. Research basis: `docs/03_research/UI_UX_RESEARCH.md`.

## 1. Product stance

BeatFix does one job: **make a song's beat match a fixed BPM, and show exactly what was changed.** It is not a DAW: no tracks, no mixer, no clip arrangement, no plug-ins. Every element on screen must serve one of four questions:

1. What tempo is this song, and how sure are we? (analysis)
2. What tempo do I want? (target)
3. What will change, where, and by how much? (visual difference)
4. Does it sound right, and can I get the file out? (A/B, export)

## 2. Principles (with the UX law each one applies)

| # | Principle | Law / source | Concrete rule |
|---|---|---|---|
| P1 | **One primary path, zero required settings.** Open -> (auto) analyse -> listen -> export must work with no decisions when confidence is High. | Tesler's law (complexity lives in the engine), Hick's law | Defaults: target = round(detected BPM) (BPM_ESTIMATION.md §5), correction = Natural (N_c = 8), mode = Smooth, export = WAV 24-bit TPDF at source rate (DECISION_SUMMARY §6). |
| P2 | **Progressive disclosure.** Primary strip shows BPM, target, correction strength, play/A-B, export. Tempo graph, warp-map plot, per-region data, algorithm parameters live in a collapsible Advanced panel. | Progressive disclosure, Miller (chunking) | Advanced panel collapsed by default; state remembered per user. Never more than ~7 primary controls in the control strip. |
| P3 | **Interpretation is separate from target.** "What the beat is" (candidate, ½/×2, bar 1, meter) is edited in the Detected row; "what tempo to output" in the Target row. | Mental-model clarity (Logic Smart Tempo "Adapt vs Keep", Melodyne Assign vs Edit) | ½/×2 never silently stretches audio 2×; it relabels beats. Target follows interpretation unless the user typed a target. |
| P4 | **Truthful visuals.** Colours and lines are computed from the same `W(s)`/`W⁻¹(t)` the renderer uses. Estimates are labelled as estimates. | Trust; aesthetic-usability effect cuts both ways | Predicted corrected waveform is drawn at reduced opacity with an "Estimate" label until real render peaks exist. |
| P5 | **Never colour alone.** Every colour-coded fact has a second channel: position/height, pattern, glyph or text. | WCAG 2.2 SC 1.4.1, 1.4.11 | Stretch vs compress: hue + signed strip direction + ◂▸ / ▸◂ glyph in hover. Confidence: line style (solid/dashed/dotted). |
| P6 | **Respond within 100 ms, show progress after 400 ms.** | Doherty threshold | Any edit updates grid, connectors and predicted waveform within one frame; renders run in background with visible state. Progress UI appears only if a task exceeds 400 ms and then stays ≥ 500 ms (no flicker). |
| P7 | **Familiar conventions.** Space play/stop, Ctrl+Z/Ctrl+Y, Ctrl+wheel zoom, Home/End, dark theme default. | Jakob's law | See KEYBOARD_SHORTCUTS.md. |
| P8 | **Make errors cheap.** Every edit undoable; source never modified; export never overwrites the source. | Error prevention / forgiveness | Undo/redo over all edit state (UX_BEHAVIOR.md §11). |
| P9 | **Point to problems, don't hide them.** Low-confidence and extreme-correction places are listed and navigable. | Von Restorff (isolation effect) | Quality status chip -> Issues list with "Go to" per item; `F8`/`Shift+F8` jump next/previous issue. |
| P10 | **Large, forgiving targets.** | Fitts's law, WCAG 2.5.8 | ≥ 24×24 px hit area for all controls; toolbar buttons 32 px; dense beat markers have an equivalent keyboard/inspector path. |
| P11 | **Calm motion.** Motion only to show causality (e.g. playhead), never decorative. | WCAG 2.3.3 (AAA, adopted), reduced motion | Respect Windows "Animation effects" (SPI_GETCLIENTAREAANIMATION) / `prefers-reduced-motion`: page-follow instead of continuous scroll, no shimmer. |
| P12 | **Plain language first, numbers on demand.** | Recognition over recall | "Slowed 3.1 %" in primary UI; ratio `r = 1.031`, `N_c`, λ only in Advanced. |

## 3. MVP UI scope

**In MVP:** open (dialog + drag-drop), progressive waveform during decode, analysis progress with cancel, detected BPM + confidence + stability, candidate picker (½/×2 and ranked alternates), set bar 1 (downbeat shift), meter 3/4 or 4/4 (pending algorithm), target BPM, Natural↔Strict (6 detents), stretch mode (Smooth / Resample; Transient Phase 1.5, Auto Post-MVP), dual waveform view with connectors and correction strip, overview strip, beat/downbeat/grid markers, hover cards, selection + loop, A/B playback with position mapping, click (metronome) overlay, collapsible tempo graph, quality status + issues list, undo/redo, reset, export dialog (WAV/FLAC/MP3), keyboard-only operation, screen-reader labels, light/dark/contrast themes.

**Post-MVP:** manual per-beat anchor drag/insert/delete (WARP_MAP.md: manual anchors post-MVP), per-region mode override and mode badges editing, section labels/structure detection, absolute-time lane mode, split L/R lanes, dB amplitude scale, tap tempo, tempo-range hint for re-analysis, blind A/B, loudness-matched A/B, batch export, customisable shortcuts, warp-map interchange export (MIDI/CSV).

## 4. Visual language (tokens)

Dark theme is default (audio-tool convention); light theme and Windows contrast themes supported. Contrast ratios computed with the WCAG relative-luminance formula.

| Token | Dark | Light | Use | Contrast vs bg |
|---|---|---|---|---|
| `bg` | `#121417` | `#FAFAFB` | canvas background | — |
| `surface` | `#1B1E23` | `#FFFFFF` | panels | — |
| `text` | `#E6E8EB` | `#16181C` | primary text | 15.0 / 17.0 |
| `text-muted` | `#A0A6AE` | `#5B616B` | labels | 7.5 / 6.0 |
| `wave-neutral` | `#7D848E` | `#8A919C` | original lane, class 0 | 4.9 / 3.05 |
| `stretch` (slowed, r_loc > 1) | `#56B4E9` | `#0072B2` | Okabe-Ito blue | 8.0 / 5.0 |
| `compress` (sped up, r_loc < 1) | `#E69F00` | `#D55E00` | Okabe-Ito orange / vermillion | 8.2 / 3.7 |
| `grid` | `#FFFFFF` @ 10 % (beats), 22 % (bars) | `#000000` @ 10 / 22 % | target grid | decorative; bars ≥ 3:1 not required (grid also labelled) |
| `beat-marker` | `#F0E442` | `#7A6F00` | detected beats | 13.95 / 4.91 vs bg; only 2.85 / 1.61 vs waveform, so marker heads sit in a marker rail on `bg` above each lane (WAVEFORM_VIEW.md §3) |
| `focus` | `#FFFFFF` 2 px + `#000` 1 px outer | inverse | focus ring | ≥ 3:1 both themes |

Class colours (correctionClass 1-3) are mixes of `wave-neutral` → hue at 33 / 66 / 100 %. Adjacent classes are only ~1.2:1 apart in luminance, so class magnitude is also encoded by strip bar height (WAVEFORM_VIEW.md §5). Stretch and compress hues have equal luminance in the dark theme (1.02:1); direction is always also encoded by strip direction and glyph.

Typography: system UI font (Segoe UI Variable on Windows 11), tabular numerals for all times/BPM/percentages; min 12 px labels, 14 px body, BPM readouts 24-28 px.

Windows contrast themes (forced colours): drop custom colours, use system colours; correction classes rendered as strip height + hatch density (class 1 sparse, 2 medium, 3 dense hatch), which survives colour removal.

## 5. Copy rules

- Units always shown: "120 BPM", "+3.1 %", "42 ms".
- Direction words: **"slowed"** (stretched, longer) and **"sped up"** (compressed, shorter). Never "stretched" alone in primary UI; it is ambiguous to non-engineers.
- Confidence: "High / Medium / Low confidence" + a one-line reason from the analyser (e.g. "half-time is also plausible").
- Errors: what happened, why (if known), what to do; never a raw code alone (code in "Copy details").
