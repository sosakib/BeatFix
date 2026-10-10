# Project / Session Model

Implements REQ-PROJ-001…007. Brief §19, §38. Decision record: [ADR-010](../09_decisions/ADR-010-project-storage.md).

## 1. Principle
A project stores **inputs only**: what the user opened, what the analysis found, what the user decided. Audio, warp maps, renders and peaks are **derived** and live in a rebuildable cache ([WARP_MAP.md](../04_algorithms/WARP_MAP.md), [RENDER_PIPELINE.md](RENDER_PIPELINE.md) §6). Deleting the cache never loses work.

## 2. `.beatfix` file (JSON, UTF-8, schemaVersion 1)
```json
{
  "format": "beatfix.project",
  "schemaVersion": 1,
  "app": { "version": "1.0.0", "algoVersion": "warp-1.0.0" },
  "projectId": "uuid",
  "source": {
    "pathAbsolute": "D:\\Music\\song.mp3",
    "pathRelative": "..\\Music\\song.mp3",
    "sizeBytes": 8123456, "mtimeUtc": "2026-10-08T10:00:00Z",
    "fileSha256": "…",
    "decoder": "minimp3@<commit>",
    "pcm": { "sampleRate": 44100, "channels": 2, "frames": 26460000, "pcmSha256": "…" }
  },
  "analysis": {
    "analyzer": "beatthis-full-onnx", "modelSha256": "…", "ortVersion": "1.30.0",
    "params": { },
    "activationsRef": "sha256:…",
    "beats":     { "detectedSec": [ ], "confidence": [ ], "inferred": [ ] },
    "downbeats": { "index": [ ], "meter": 4 },
    "tempo":     { "bpmGlobal": 109.96, "candidates": [ ], "margin": "green" },
    "stability": { "class": "drifting", "changePoints": [ ] }
  },
  "edits": {
    "beatLevel": "detected", "downbeatShift": 0, "meter": 4,
    "changePointDecisions": [ { "atAnchor": 128, "decision": "flatten" } ]
  },
  "settings": {
    "targetBpm": 110.0, "targetMode": "auto",
    "phasePolicy": "preserveStart", "cutoffBeats": 8,
    "mode": "smooth", "engine": "signalsmith@1.4.0",
    "export": { "format": "wav", "bitDepth": 24, "sampleRate": "source", "dither": "tpdf", "ditherSeed": 1234 }
  },
  "ui": { "zoom": 1.0, "scrollSec": 0.0, "lane": "corrected", "advancedOpen": false }
}
```
- Times are float64 seconds, which survive re-decoding. Sample positions are derived at render time.
- `activationsRef` points to a sidecar in the analysis cache (activations are about 30 k × 2 floats). The **beats, confidences and tempo results are inline**, so the project reproduces without the sidecar (REQ-PROJ-002). The sidecar is needed only to re-run post-processing with different parameters.
- `enhancement` is absent in the MVP. Readers must tolerate unknown keys.

## 3. Versioning
- `schemaVersion` is an integer. Loaders run a chain of pure migrations `vN → vN+1`. Each historical version has golden fixtures in `tests/fixtures/project/vN/`.
- Unknown fields are preserved on save (kept in their original location). A file with a future `schemaVersion` opens **read-only** with a warning.
- A changed `algoVersion` means the derived warp map is recomputed, with the visible notice from WARP_MAP.md.

## 4. Saving
- Atomic: write `name.beatfix.tmp` in the same directory → `FlushFileBuffers` → `ReplaceFileW` (keeps ACLs). If the target doesn't exist, use `MoveFileExW(…REPLACE_EXISTING|WRITE_THROUGH)` instead. One `name.beatfix.bak` (the previous version) is kept.
- The window title shows `•` when dirty. Closing asks Save / Don't save / Cancel.

## 5. Autosave and crash recovery
- When the project is dirty, autosave every **60 s** to `%LOCALAPPDATA%\BeatFix\autosave\<projectId>.beatfix` using the same atomic routine. The user's file is never overwritten implicitly.
- The session lock `%LOCALAPPDATA%\BeatFix\sessions\<projectId>.lock` holds the PID and start time.
- On launch, a dead-PID lock plus an autosave newer than the project produces "Recover unsaved changes?" with a diff summary (target BPM, detent, edits). Accept loads the autosave as a dirty project. Discard deletes it.
- Untitled sessions (no project saved yet) autosave too. Recovery reopens them as untitled.

## 6. Source moved, renamed or changed (brief §19)
Resolution order on open:
1. absolute path + size/mtime match → accept, verify `fileSha256` lazily in the background;
2. relative path from the project folder;
3. same file name in the project folder;
4. prompt "Locate source…" (browse), with automatic search of the chosen folder by name.

Acceptance of a candidate:
| Check | Result |
|---|---|
| `fileSha256` equal | accept |
| file differs but `pcmSha256` equal (re-tagged or re-wrapped) | accept, update the stored file hash |
| duration equal within 1 ms, PCM differs | warn "different audio; beats may not fit" → [Re-analyse] [Use anyway] [Cancel] |
| otherwise | reject candidate |

While the source is missing, the project opens read-only (edits viewable, no playback, render or export). A banner offers Locate.

During a session, size and mtime are rechecked at playback start and before export. Export re-verifies the hash and refuses on mismatch (AT-17).

## 7. What lives outside the project

| Item | Location | Rebuildable |
|---|---|---|
| PCM cache, render chunks | `%LOCALAPPDATA%\BeatFix\cache` | yes |
| Analysis sidecars (activations) | same cache | yes (re-analysis, which may differ at D2) |
| Peaks | same cache | yes |
| Autosave, locks | `%LOCALAPPDATA%\BeatFix\autosave`, `sessions` | n/a |
| Settings | `%APPDATA%\BeatFix\settings.json` | defaults |

## Risks / Open Questions
- Should the analysis sidecar be embeddable in the project (an "archive project" for moving between machines)? Proposed post-MVP option "Save with analysis data".
