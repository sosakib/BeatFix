# Keyboard Shortcuts

> **Status:** UI spec v1.0, 2026-10-10, aligned with `docs/10_implementation/DECISION_SUMMARY.md` (authoritative on conflicts). Anything beyond the MVP is marked **Post-MVP**. Customisable key bindings are Post-MVP.

## 1. Rules

- Follow DAW/Windows conventions where they exist (Jakob's law): Space transport, Ctrl+Z/Ctrl+Y, Ctrl+wheel zoom, Home/End, F6 pane cycling, F1 help, Alt/F10 menu access.
- **Single-letter shortcuts are suppressed while a text/number field has focus** (BPM field, file name). Esc leaves the field (reverting uncommitted text) and returns focus to the waveform.
- Space is global *except* when a button/checkbox/radio has focus (where Space activates it, per platform convention); Ctrl+Space plays/pauses from anywhere, including fields.
- No shortcut uses Alt+letter (reserved for menu mnemonics on Windows) or Win key.
- Every pointer action has a keyboard path (WCAG 2.1.1); no shortcut is the only way to do anything (menus/buttons exist).
- Shortcuts are listed in tooltips (`Play (Space)`) and in Help → Keyboard shortcuts (`Ctrl+/` opens the list).

## 2. Map

### File / app
| Keys | Action | Precedent |
|---|---|---|
| Ctrl+O | Open audio file | universal |
| Ctrl+S / Ctrl+Shift+S | Save project / Save as (PROJECT_SESSION_MODEL.md) | universal |
| Ctrl+E | Export… | many audio apps use Ctrl+E/Ctrl+Shift+R; chosen for mnemonic |
| Ctrl+Z | Undo | universal |
| Ctrl+Y, Ctrl+Shift+Z | Redo | Windows / DAWs |
| Ctrl+, | Settings | common desktop convention |
| F1 | Help | Windows |
| Ctrl+/ | Shortcut list | |
| F6 / Shift+F6 | Next / previous pane (file bar → control strip → overview → waveform → advanced → transport) | Windows |
| Tab / Shift+Tab | Next / previous control within pane | platform |

### Transport and A/B
| Keys | Action |
|---|---|
| Space | Play / pause (see rules) |
| Ctrl+Space | Play / pause from anywhere |
| Shift+Space | Stop, return to start position |
| Enter (numeric keypad) | Play from cursor (REAPER/Logic-like) |
| Home / End | Cursor to start / end |
| A / B | Hear Original / Corrected |
| T (also `` ` ``) | Toggle A/B |
| L | Loop selection on/off |
| K | Click (metronome) overlay on/off |
| F | Follow playhead: cycle Continuous → Page → Off |
| Ctrl+Shift+T | Speak current position (screen reader) |

### Navigation and selection (waveform focused)
| Keys | Action |
|---|---|
| ← / → | Cursor to previous / next beat |
| Ctrl+← / Ctrl+→ | Previous / next bar (downbeat) |
| Shift + above | Extend selection |
| Page Up / Page Down | Scroll one view width |
| F8 / Shift+F8 | Next / previous issue (low confidence, extreme correction, render problem) |
| Esc | Clear selection (or cancel current drag/grab) |
| Ctrl+A | Select all |
| Double-click / Enter on bar in region list | Select bar |

### Zoom
| Keys | Action |
|---|---|
| Ctrl+= (Ctrl++) / Ctrl+− | Zoom in / out ×2 (anchor: playhead if visible, else centre) |
| Ctrl+0 | Fit whole song |
| Z | Zoom to selection |
| Ctrl+wheel | Zoom at pointer |
| Wheel, Shift+wheel | Horizontal scroll |

### Tempo / correction
| Keys | Action |
|---|---|
| Ctrl+B | Focus target BPM field |
| Up / Down (in BPM field) | ±1 BPM; Shift ±0.1; Ctrl ±0.01; Enter commits |
| H | Interpretation: half tempo (½) |
| D | Interpretation: double tempo (×2) |
| Ctrl+K | Open beat-interpretation candidate picker |
| Ctrl+1 | Set bar 1 at cursor (nearest detected beat) |
| [ / ] | Natural↔Strict: one detent toward Natural / Strict |
| M | Open stretch mode menu |
| Ctrl+Shift+R | Reset corrections (confirm if edits exist; undoable) |
| Ctrl+Alt+R | Re-analyse |

### Post-MVP (manual anchor editing)
| Keys | Action |
|---|---|
| Enter | Grab selected beat marker; Enter again commits |
| ← / → while grabbed | Nudge 1 ms; Shift 10 ms |
| Insert | Insert anchor at cursor |
| Delete | Remove selected anchor (marks beat ignored) |
| O | Snap selected anchor to nearest onset |

## 3. Conflicts reviewed

- `H`/`D` vs typing: suppressed in fields (rule 1).
- `T` vs `Ctrl+Shift+T`: distinct chords; no conflict.
- `Ctrl+E`: some apps use for "centre"/"search"; no conflict inside BeatFix.
- `[`/`]` on non-US layouts may require AltGr; menu and slider remain the primary path.
- `` ` `` is layout-dependent; `T` is the guaranteed toggle.
- Delete is unused in MVP (no destructive single-key action in MVP).
