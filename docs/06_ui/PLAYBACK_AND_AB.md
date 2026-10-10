# Playback and A/B Comparison

> **Status:** UI spec v1.0, 2026-10-10, aligned with `docs/10_implementation/DECISION_SUMMARY.md` (authoritative on conflicts). Preview is chunked offline rendering ([RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md)); cache 5 GB LRU. Anything beyond the MVP is marked **Post-MVP**.

## 1. Concepts

- **Source A = Original**: the decoded immutable source, played in source time `s`.
- **Source B = Corrected**: the current render of the current edit state, played in target time `t`.
- **Musical position**: the pair `(s, t)` with `t = W(s)`. The app keeps one musical position; each source plays its own coordinate. Switching A/B never changes the musical position.
- **Playhead**: drawn in both lanes at `x_top(s)` and `x_bot(t)`. With the centre-anchored view (WAVEFORM_VIEW.md §2) they coincide when the playhead is at the view centre (continuous follow).

## 2. Transport

| Control | Behaviour |
|---|---|
| Play/Pause (Space) | Toggles playback of the active source from the musical position. Space while stopped starts from cursor. |
| Stop (Shift+Space or ⏹) | Stops and returns the playhead to where playback last started (DAW convention; setting "Stop returns to start position": on). |
| ⏮ Home / End | Move cursor to start / end of the active source. |
| Click on lane | Sets cursor (musical position) from that lane's coordinate: click on top lane at `s` → `t = W(s)`; on bottom at `t` → `s = W⁻¹(t)`. While playing, click = seek (setting; default on). |
| Time display | `m:ss.mmm` of the active source + `Bar.Beat.Sixteenth` from the target grid; click toggles which is primary; Ctrl+Shift+T speaks it. |
| Loop (L) | Loops the selection. Selection stored in target time `[t_a, t_b]`; in A it plays `[W⁻¹(t_a), W⁻¹(t_b)]`. No selection → button disabled with tooltip "Select a range to loop". |
| Follow | `Page` (default when reduced motion), `Continuous` (playhead centred; default otherwise), `Off`. Any manual scroll during playback suspends follow until the playhead re-enters the view or F is pressed. |
| Click (metronome) overlay (K) | Mixes a click at beats: in A at detected beats `S_i`, in B at grid slots `T_k`; downbeats accented (+6 dB, higher pitch). Click level slider in its popover (default −12 dBFS peak). Click is never part of export. This is the primary tool for verifying detection (MVP, recommended). |

## 3. A/B switching semantics

| Action | Result |
|---|---|
| `A` | Activate Original. |
| `B` | Activate Corrected. |
| `` ` `` or `T` (toggle) | Swap active source. (`` ` `` is layout-dependent; `T` always works.) |
| Segmented control `[A Original | B Corrected]` | Same as A/B keys. |

While **playing**, switching from B at target time `t` to A seeks A to `s = W⁻¹(t)` (and A→B to `t = W(s)`), then crossfades:
- Equal-power crossfade, default 20 ms (setting 5-50 ms; DECISION_SUMMARY §6). Both voices run during the fade; the outgoing voice fades out at its own position.
- Latency target: switch audible within ≤ 50 ms of the key press (one audio buffer + fade; to be measured, PERFORMANCE_BENCHMARKS.md).
- The positions are computed from the same `W`/`W⁻¹` as the renderer, including pre-roll/tail edge ratios (TARGET_BEAT_GRID.md §5), so switching inside an intro is still musically consistent.

While **stopped**, switching only changes which source Play will use; cursor stays at the same musical position.

Level: no automatic gain change between A and B (time-stretching preserves level closely; any automatic gain would bias the comparison). **Post-MVP:** loudness-matched A/B (integrated loudness of the looped range) and blind A/B ("X is A or B?") mode.

Visual state: active lane header is highlighted (bold label + 3 px left accent bar + "▶ Playing"), inactive lane dimmed to 70 % opacity. Screen readers: switching announces "Playing corrected" / "Playing original".

## 4. Preview of the corrected source

The corrected source can be partially rendered (after any edit). Rules:

1. Every edit computes the new warp map synchronously (≈ ms, WARP_FUNCTION.md §4) and invalidates affected render chunks (UX_BEHAVIOR.md §9).
2. Rendering is prioritised: chunk containing the playhead first, then forward from playhead, then backward, then the rest.
3. Render cache key = hash(source fingerprint, warp-map `inputsHash`, mode, preview quality, engine id + version). Toggling back to a previous state (undo, or re-selecting the previous mode) reuses cached renders if still present (cache: 5 GB LRU, [RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md) §6).
4. If playback of B reaches an unrendered chunk:
   - Pause B output (MVP has no realtime stretch path, ADR-007) at the chunk boundary, show a buffering indicator on the playhead ("Rendering…"), resume automatically when ready; A remains instantly available. Never play silence or stale audio from an older edit state without marking it.
5. Stale-audio rule: audio from a render of a previous edit state is never played as B. A chunk is either current or not played.

Preview quality: `Fast` (default during editing, if the engine offers a cheaper preset) vs `High`. Export always renders at the highest quality from source; EXPORT_UI.md states this. If preview quality is `Fast`, the corrected lane header shows "Preview: Fast".

## 5. Selection

- Drag on either lane creates a selection, stored in target time. Shift+click extends. Shift+←/→ extends by beat, Ctrl+Shift+←/→ by bar. Esc clears.
- Double-click selects the bar under the pointer; triple-click selects all.
- Selection shown on both lanes (top mapped through `W⁻¹`) as a 20 % alpha highlight with 1 px edges.
- Selection is view state: not undoable, but saved with the project as UI state (PROJECT_SESSION_MODEL.md §2 `ui`).

## 6. Edge cases

| Case | Behaviour |
|---|---|
| Corrected file shorter than original (sped up) and playhead in original beyond W's range end | Musical position clamps to end of B; switching to B there plays the tail end; A unaffected. |
| Playing B while user changes target BPM | Warp recomputed; B's playhead re-mapped via the *new* W from the current `s`; B pauses at most for the playhead chunk render (rule 4). |
| Audio device lost / changed | Playback stops, toast "Audio device disconnected"; device picker in Settings; position kept. |
| Exporting while playing | Allowed; export runs on its own worker; playback continues (priority to realtime thread is an architecture requirement). |
