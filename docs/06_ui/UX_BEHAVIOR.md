# UX Behaviour Specification (deterministic)

> **Status:** UI spec v1.0, 2026-10-10, aligned with `docs/10_implementation/DECISION_SUMMARY.md` (authoritative on conflicts). Preview is chunked offline rendering ([RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md)); cache 5 GB LRU. Anything beyond the MVP is marked **Post-MVP**.

## 1. Application states

```
EMPTY ──open──▶ LOADING ──decoded──▶ DECODED ──(auto)──▶ ANALYSING ──done──▶ READY
  ▲               │ fail/cancel          │  ▲ cancel/fail     │                 │ ▲
  │               ▼                      │  └────────────────┘                 │ │ edit → re-warp (sync)
  └──────────── ERROR(load)              └──[Analyse]──▶ ANALYSING             │ │ + preview render (async)
                                                                               ▼ │
                                                                          EXPORTING (parallel sub-state)
```
- READY has sub-states for the corrected source: `estimate` (no render chunks current), `rendering(n %)`, `rendered`. Export is a parallel job; READY editing continues during export (export uses a snapshot).
- Only one document per window. Opening a new file in non-EMPTY state → §2 step 0.

## 2. Import

Triggers: Open dialog (Ctrl+O), drag-drop of one file onto the window, (Post-MVP) command-line/file association.

0. If a document is open with unsaved edits: dialog "Replace song.mp3? Unsaved changes will be lost." [Save] [Don't save] [Cancel]. Dropping > 1 file: use the first audio file, toast "Only one song at a time; opened first file."
1. Validate path exists and is readable. Fail → ERROR(load) card: "Can't open file: <reason>" [Choose another…].
2. **Probe** container/codec/sample rate/channels/duration without full decode (target ≤ 200 ms). Unsupported → §15. More than 2 channels → MVP: unsupported-layout message (§15 variant; multichannel Post-MVP). Duration > supported max (REQ-PERF-014: 60 min, PROVISIONAL) → message with limit.
3. Enter LOADING: file bar shows name and probed metadata immediately; control strip disabled.
4. **Decode** full file to float on a worker; peak pyramid built incrementally; waveform fills left→right at ≥ 10 updates/s. Fingerprint computed from decoded PCM (WARP_MAP.md `sha256-pcm`) during decode.
5. Decode warnings (damaged frames) → §14.
6. Decode complete → DECODED: original playback enabled. If Settings "Analyse on open" (default on) → §3 automatically.

Cancel during LOADING (Esc / Cancel) → EMPTY (the previous song was already released at step 3; step 0 is the only point of return).

## 3. Analyse

1. Enter ANALYSING; progress card (MAIN_WINDOW.md §4.2) shown only if not finished within 400 ms; once shown, minimum 500 ms.
2. Stages run in order; each reports progress or indeterminate. Original playback and zoom/scroll stay available.
3. Cancel → DECODED, card replaced by `[Analyse]` button in the control strip; no partial results kept.
4. Failure (engine error, out of memory) → DECODED + error card "Analysis failed: <plain reason>" [Retry] [Copy details]; Copy details contains engine/version/stage/error code; local only.
5. Success → compute defaults: interpretation = top candidate; target = `round(bpmGlobal)` (BPM_ESTIMATION.md §5) and `targetMode = auto` (follows interpretation); N_c = 8; mode = Smooth; phase policy from Settings. Build warp map synchronously; enter READY with `estimate`; start preview render (§9).
6. Analysis results are immutable; a re-analysis (Ctrl+Alt+R) produces a new analysis and is an undoable command; if interpretation edits exist, confirm first ("Re-analysing clears your beat edits").

## 4. BPM detection confidence

Engine supplies global `High | Medium | Low | None` + reason + candidates (BPM_ESTIMATION.md §6-7).

4.1 **High**: apply defaults (§3.5), no prompt. Quality chip unaffected.
4.2 **Medium**: apply defaults; show "Check:" line with the strongest alternative as a chip; add Issues entry "Tempo: alternative plausible" (severity Review). Any interpretation interaction (choosing a chip, ½/×2, picker, Confirm) marks the tempo **confirmed** and removes the issue.
4.3 **Low**: apply defaults; open the candidate picker expanded (non-modal, docked under the control strip); Issues entry "Tempo not confirmed" (Review). First export while unconfirmed → confirm dialog [Export] [Review]. Choosing Export counts as confirmation.
4.4 **None** (no steady beat): no warp map; corrected lane shows "No beat grid. Correction unavailable." Target field enabled for manual entry: the user may type a BPM to set the grid for display and the click overlay, but with no detected anchors there is nothing to map, so Export stays disabled with explanation "Nothing to correct: no beats were detected." (Manual anchors are Post-MVP; tap tempo Post-MVP.)
4.5 Beat-level low confidence (per-anchor `confidence`): rendered as dotted markers; contiguous low-confidence spans ≥ 2 bars become one Issues entry each.

## 5. Ambiguous BPM / interpretation edits

1. Candidates shown as defined in MAIN_WINDOW.md §4.5; none hidden if within the engine's ambiguity margin.
2. Selecting a candidate, ½ or ×2 = one undoable command "Change beat interpretation to 59.92 BPM". It changes the beat level (WARP_MAP.md user edit "beat level ×2/÷2") and re-runs the warp build (not the analysis).
3. If `targetMode = auto`: target ← round(new interpretation BPM). If `targetMode = manual` (user typed a target): target unchanged; if |ln g| > ln 1.15 show inline warning under target: "Target is 15 %+ away from the song's tempo (59.9 BPM). This will stretch the whole song strongly." [Use 60] [Keep 120].
4. Set bar 1 (Ctrl+1 / button) at cursor: snaps to nearest detected beat within ± half a beat; none → message "No beat near the cursor". Undoable "Set bar 1 at 0:00.412".
5. Meter change (3/4, 4/4; DOWNBEAT_ESTIMATION.md) relabels bars; undoable.

## 6. Target BPM change

1. Field accepts 40.00–250.00 (REQ-GRID-001); values producing segment ratios outside WARP_FUNCTION.md §3 bounds are rejected with the computed allowed range shown). Up to 2 decimals; comma or dot accepted.
2. Commit on Enter, on blur, or after 600 ms of no typing if the value parses (debounced). Spinner arrows commit each step; repeated steps within 500 ms coalesce into one undo step.
3. On commit: `targetMode ← manual` (unless the value equals the suggestion, then it stays auto); warp map rebuilt synchronously; grid, connectors, classes, predicted waveform update in the same frame; all render chunks invalidated (every T_k after the phase reference moves); preview render restarts from playhead (§9).
4. Overall change readout updates ("Overall: sped up 6.7 %"). If |ln g| > ln 1.15 → Issues entry "Large tempo change" (Review; DECISION_SUMMARY §6).
5. The ↺ button next to the field restores the suggestion and `targetMode = auto` (undoable).

## 7. Natural↔Strict change

Detent change (N_c ∈ {∞, 32, 16, 8, 4, Strict}, WARP_FUNCTION.md §4.3) → undoable command, warp rebuilt synchronously (≈ 1 ms per 1000 beats), visuals update same frame, readout (grid err ms / max %) updates, render chunks whose segments changed are invalidated (in practice most of the song).

## 8. Stretch mode change

1. Mode menu (MVP): **Smooth** (default) and **Resample** (enabled only at the Global detent, i.e. one constant ratio; shows the pitch shift in cents). Transient = Phase 1.5; Auto = Post-MVP. Change = undoable command.
2. Warp map timing unchanged → grid, connectors, classes and predicted waveform do not change; only the segment `mode` and render output change.
3. Cache: renders are keyed by mode (PLAYBACK_AND_AB.md §4.3). Switching to a mode whose render for the current warp is still cached is instant (no re-render). Otherwise all chunks are invalidated and re-rendered playhead-first.
4. While re-rendering, playback of B follows the stale-audio rule (never plays the previous mode's audio as current).
5. Preview quality change (Fast/High) behaves the same way (separate cache key).

## 9. Region changes and render invalidation

1. Every edit produces a new warp map; diff old vs new per segment (`srcStart/srcEnd/tgtStart/tgtEnd/ratio/mode` exact compare).
2. Changed target span = union of changed segments, padded by the engine pre-roll and overlap on each side (RENDER_PIPELINE.md §3).
3. Chunks overlapping the padded span are marked invalid (static hatch on the corrected lane); others stay valid: chunks are rendered independently with deterministic seams and content-addressed keys (RENDER_PIPELINE.md §4, §6).
4. Render queue order: playhead chunk, forward, backward, rest. Newer edits cancel queued work for older edit states immediately; an in-flight chunk for an obsolete state is discarded on completion.
5. UI shows `Rendering n %` in the corrected lane header; Quality chip waits for full render before reporting render-derived issues (clipping).
6. Post-MVP manual anchor edits use the same path; only local segments change, so local invalidation becomes significant.

## 10. Playback switching

As PLAYBACK_AND_AB.md §3. Deterministic rules:
1. One musical position `(s, t = W(s))`. A plays `s`, B plays `t`.
2. Switch while playing: seek target source to the mapped position using the current W; equal-power crossfade (default 20 ms).
3. Switch while stopped: no seek audible; cursor unchanged.
4. Edit while playing B: re-map from current `s` with the new W; if playhead chunk not ready → buffering (or live preview if engine supports).
5. A is always instantly playable once decoded.

## 11. Undo / redo

- **Undoable** (edit state E): interpretation (candidate / beat level / bar 1 / meter), target BPM and targetMode, N_c detent, stretch mode, preview quality, phase policy, re-analysis, reset corrections, (Post-MVP) anchor edits and per-region modes.
- **Not undoable** (view state): zoom, scroll, selection, cursor, A/B, loop, follow, click overlay, panel layout, display thresholds; export (an export is a side effect, not an edit).
- Implementation model: stack of immutable E snapshots (E is small; analysis results referenced by id), depth 200. Labels shown in Edit menu and tooltips ("Undo Change target to 120 BPM").
- Coalescing: BPM spinner steps within 500 ms; repeated `[`/`]` detent presses within 500 ms; typing in the field (only the commit counts).
- Undo/redo re-applies E: warp rebuilt synchronously, render served from cache when the key matches, else re-rendered (§9).
- New edit after undo clears the redo stack.
- History is not persisted in the project file (MVP). Saving does not clear history.

## 12. Export

As EXPORT_UI.md. Deterministic rules: pre-flight validation completes before any rendering; export uses a snapshot of E taken when Export is pressed; output written to `.partial` then atomically renamed; cancel/failure removes the partial file; the source file is never opened for writing; never overwrites the source path; post-export validation always runs and its result is shown.

## 13. Source missing (project open)

Trigger: opening a project whose source path doesn't exist, or whose file fingerprint differs.
1. Path missing → dialog: "The original audio for this project was moved or renamed. Expected: <path>." [Locate…] [Search folder…] [Open without audio] [Cancel].
   - Search folder: scans the project folder and the original folder (non-recursive) for files with the same name or the same size and duration, then verifies fingerprint; a single match is proposed for confirmation.
   - Open without audio: project opens read-only (analysis and warp visible from stored data; playback and export disabled; banner "Audio missing [Locate…]").
2. Located file fingerprint matches → path updated (project marked dirty), continue normally.
3. Fingerprint differs → "This file is different from the one analysed (content changed)." [Re-analyse with this file] [Choose another] [Cancel]. Re-analyse discards stored analysis and edits that reference beat indices (interpretation edits are re-applied only if still valid; user told which were dropped).
4. Fingerprint matches but `algoVersion` of stored warp differs → recompute with notice (WARP_MAP.md versioning).

## 14. Corrupt audio

1. Probe fails (header unreadable) → ERROR(load): "This file looks damaged and can't be read." [Choose another…] [Copy details].
2. Decode errors mid-stream that the decoder can skip (e.g. MP3 frame CRC/sync errors): continue; record each damaged span (time, length); replace with silence of the exact skipped length so timing is preserved (pending decoder capability; if the length is unknown, mark position only).
   - After decode: Issues entry per span cluster "Damaged audio near 2:13 (silence inserted)" (Review); a warning band in the original lane's ruler at each span.
   - Analysis proceeds; beats inside damaged spans get confidence capped at Low (BEAT_TRACKING.md §5).
   - Export allowed; result card repeats the warning.
3. Truncated file (decoded duration < header duration by > 1 s): continue with decoded audio; Issues entry "File is shorter than its header says (3:12 of 3:41)".
4. Decode produces NaN/Inf samples → replaced by 0 with an Issues entry (Review).
5. No decodable audio at all → ERROR(load).

## 15. Unsupported codec / format

1. Determined at probe (§2.2), never after a long decode.
2. Message: "BeatFix can't open <container>/<codec> files yet. Supported: WAV, FLAC, MP3 (+ list per COMPATIBILITY_MATRIX.md). Convert the file to WAV or FLAC with a lossless converter and open it again." [Choose another…] [Copy details].
3. Variants: > 2 channels (MVP) → "Multichannel audio (6 ch) isn't supported yet; use a stereo mixdown."; unsupported sample format/rate → same pattern with the specific value.
4. No silent transcoding, no partial opening.

## 16. Timing constants (defaults; all configurable in code, not UI unless noted)

| Constant | Value | Where |
|---|---|---|
| Progress UI show delay / min visible | 400 ms / 500 ms | §3 |
| ETA shown after | 2 s of progress | MAIN_WINDOW.md §4.2 |
| BPM field idle commit | 600 ms | §6 |
| Undo coalescing window | 500 ms | §11 |
| Hover card show / hide | 300 / 100 ms | WAVEFORM_VIEW.md §6 |
| A/B crossfade | 20 ms (Settings 5-50) | PLAYBACK_AND_AB.md |
| A/B switch latency target | ≤ 50 ms | to be measured |
| Edit → visual update | same frame (≤ 16.7 ms target) | P6 |
| Large-change warning | |ln g| > ln 1.15 (DECISION_SUMMARY §6) | §5, §6 |
| Extreme correction flag | p ≥ 12 % | WAVEFORM_VIEW.md §5.2 |
