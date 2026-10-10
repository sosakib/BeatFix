# Export UI

> **Status:** UI spec v1.0, aligned with `docs/10_implementation/DECISION_SUMMARY.md` §1 and §6 (export defaults, dither, peak gate). Anything beyond the MVP is marked **Post-MVP**.

## 1. Flow

```
[Export…] (Ctrl+E) ─▶ Export dialog ─▶ pre-flight checks ─▶ render from source ─▶ encode ─▶ validate ─▶ result card
                         ▲  │ Cancel                    │ Cancel at any time (partial file removed)
                         └──┘                           ▼
                                                    error card (retry / change settings / copy details)
```
Export always renders from the immutable source + current edit state at export quality (never re-encodes a preview, never reads a previous export). One lossy encode at most, at the very end.

## 2. Dialog

```
┌ Export corrected audio ─────────────────────────────────────────┐
│ Format       (•) WAV   ( ) FLAC   ( ) MP3                        │
│ Sample rate  [Same as source (44.1 kHz) ▾]                       │
│ Bit depth    [24-bit ▾]        Dither [TPDF ▾]   (integer only)  │
│ MP3 quality  [CBR 320 ▾]                         (MP3 only)      │
│ Start        [Preserve start ▾]  (DAW-ready start = first        │
│              downbeat on a bar line; adds silence if needed)     │
│ Metadata     ☑ Copy tags from source   ☑ Write BPM tag (120)     │
│ File name    [{name} ({bpm} BPM)]  → song (120 BPM).wav          │
│ Folder       [C:\Users\…\Music\           ] [Browse…]            │
│ ▸ Advanced: output gain [0.0 dB], noise shaping ☐               │
│                                                                  │
│ Quality: export renders at the highest quality setting; it can  │
│ sound slightly different from a "Fast" preview.                  │
│                                   [Cancel]   [Export]            │
└──────────────────────────────────────────────────────────────────┘
```

| Field | Options (MVP) | Default | Notes |
|---|---|---|---|
| Format | WAV, FLAC, MP3 | WAV | AIFF, AAC/M4A, Ogg, Opus: Post-MVP (pending codec licensing) |
| Sample rate | Same as source, 44.1, 48, 88.2, 96 kHz | Same as source | conversion uses the project resampler (RESAMPLING.md) |
| Bit depth | WAV: 16, 24, 32-float; FLAC: 16, 24 | 24-bit | AUDIO_QUALITY_RESEARCH; DECISION_SUMMARY §6 |
| Dither | TPDF, Off (noise shaping: advanced, off) | TPDF for any integer output (16 or 24-bit); hidden for 32-float WAV and MP3 (never dithered) | fixed per-project seed, so re-exports are byte-identical (DITHERING.md) |
| MP3 quality | CBR 320, VBR V0, VBR V2 | CBR 320 | LAME (LGPL DLL) with LAME/Xing gapless tag; 32/44.1/48 kHz only (higher sources resampled once). Hint: "For DAWs prefer WAV/FLAC (MP3 gapless support varies)" |
| Start (phase policy) | Preserve start, DAW-ready start | from Settings (Preserve start) | TARGET_BEAT_GRID.md §3; changing it is an edit of render inputs, not just an export option, so the dialog shows "Changes the correction" note and the preview re-renders after export |
| Metadata | copy source tags; write BPM tag | both on | BPM tag written as integer when target is integer, else rounded (format limits pending) |
| File name | template tokens `{name}`, `{bpm}`, `{mode}`, `{date}` | `{name} ({bpm} BPM)` | live preview of resulting name |
| Folder | last used, else source folder | — | |
| Output gain | −24 … +6 dB, 0.1 dB steps | 0 dB | |

Settings remembered per user (last used), not per project. The dialog is fully keyboard operable; Enter = Export when no field is in edit mode; Esc = Cancel.

## 3. Pre-flight validation (before any rendering)

| Check | Failure behaviour |
|---|---|
| Output path equals source path (case-insensitive, after resolving) | Blocked inline: "Can't overwrite the original file. Choose another name." Export button disabled. |
| File exists | Inline choice on Export: [Replace] [Keep both → `name (2).wav`] [Cancel] |
| Invalid filename (Windows reserved names CON, PRN, AUX, NUL, COM1-9, LPT1-9; characters `<>:"/\|?*`; trailing dot/space) | Inline error under the field; Export disabled |
| Path length > 260 without long-path support | Inline error with suggestion to shorten |
| Folder not writable | Inline error; [Browse…] focused |
| Free space < estimated size × 1.2 | Warning with estimate; user may continue |
| Low confidence unconfirmed | Confirm dialog (MAIN_WINDOW.md §4.4) |
| Open "Review" issues | Non-blocking note in dialog: "2 items to review" [Show] |
| Format/bit-depth combo invalid (e.g. FLAC 32-float) | Option not offered |

### 3.1 Peak gate (after render, before encode)
Rule ([QUALITY_VALIDATION.md](../04_algorithms/QUALITY_VALIDATION.md) §4): a lossy target with true peak > −1 dBTP, or an integer PCM target with sample peak > 0 dBFS, pauses export with:
```
 ⚠ The corrected audio peaks at +0.6 dBTP (limit for MP3: −1.0 dBTP)
   (•) Lower the level by 1.7 dB  (recommended, keeps the sound unchanged)
   ( ) Use the true-peak limiter (ceiling −1.0 dBTP, may soften loud hits)
   ( ) Export anyway (about 112 samples will clip)
                                         [Back]   [Continue]
```
Float WAV above 0 dBFS gets an informational note only. The decision is shown on the result card and stored in the project's export settings. Nothing is applied without this choice (REQ-EXP-003, REQ-AUDIO-008).

## 4. Progress

Modal-less progress card over the transport (app remains usable for viewing/playing; edits that would change the export are allowed but do not affect the running export, which uses a snapshot of the edit state):
```
 Exporting song (120 BPM).wav        ▓▓▓▓▓▓▓░░░  71 %   About 4 s left   [Cancel]
 Rendering → Encoding → Checking
```
- Stages: Rendering, Encoding, Checking. ETA after 2 s.
- Writes to `name.ext.partial` in the target folder, renamed atomically on success. Cancel or failure deletes the partial file. The source is never opened for writing.
- Screen reader: announces start, completion or failure (polite); not percentages.

## 5. Post-export validation and result

Checks (definitions owned by QUALITY_VALIDATION.md §5): file decodes; channel count and sample rate match; duration matches expected within the codec's tolerance (MP3 encoder delay/padding handled via the LAME tag); sample peak and true peak measured; no NaN/Inf; clipping count.

Result card:
```
 ✓ Exported  song (120 BPM).wav · 3:41.0 · 44.1 kHz · 24-bit · peak −0.8 dBTP
   [Show in folder]  [Play exported file]  [Export again…]
```
Peak handling is **not** a post-export warning: overs are resolved before encoding by the peak gate (§3.1). The result card reports the gate decision (e.g. "gain −0.9 dB applied" or "exported with 112 clipped samples").

Failure card: what failed, at which stage, [Retry] [Change settings…] [Copy details]. Details include stage, error code, path (local only; nothing is sent anywhere).

## 6. Post-MVP
Batch export of several projects; export presets; export the warp map / tempo map (MIDI, CSV, WARP_MAP.md "Interchange"); export selection only; stems.
