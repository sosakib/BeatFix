# Error Handling and Failure Modes

Implements REQ-REL-001…005. Brief §21–22. User-facing copy and state machine: [UX_BEHAVIOR.md](../06_ui/UX_BEHAVIOR.md).

## 1. Error model
- The core (`bf_dsp`, `bf_analysis`, `bf_io`, `bf_engine`) returns `Result<T>`, a small struct `{T value; Error err;}`, where `Error = {Code code; std::string detail; Severity sev;}` ([CODING_STANDARDS.md](../08_engineering/CODING_STANDARDS.md)).
- **No exceptions cross a module boundary or the C API.** Third-party exceptions (JUCE, ORT C++ API) are caught at the adapter and converted.
- **Cancellation is not an error.** Jobs return `Cancelled`, which the UI treats as "back to the previous valid state" and never shows as a failure.
- Each error has: a **detection point**, a **safe state**, a **user message** (plain language, what happened + what to do), a **recovery action**, and **logging** (code + context, never audio or full paths).

## 2. Failure-mode table

| # | Failure | Detected by | Safe state | User sees | Recovery |
|---|---|---|---|---|---|
| F1 | No detectable beat | confidence = None (silence, noise, ambient) | Decoded, analysed, no warp | "No steady beat found" | enter BPM manually (post-MVP: tap); export disabled |
| F2 | Weak rhythm / low confidence | global Low, or spans with low beat confidence | Ready, Low badge | "Tempo not confirmed" + candidates + click | confirm/relabel; first export asks for confirmation |
| F3 | Tempo ambiguity | candidate margin Amber/Red | Ready, picker open | ranked candidates | user picks |
| F4 | Highly variable tempo / unreliable | robust residual scale > 0.15·P | Ready, no auto-render | "Beat analysis unreliable here" + spans | relabel, re-analyse, or proceed with warning |
| F5 | Intentional tempo change | change-point detector | Ready, choice pending | "Tempo changes at 2:13" | keep section (r = 1) / flatten |
| F6 | Ratio outside [0.5, 2] | `WarpMap::validate` | Render refused | "Correction impossible at this target" | half/double offer |
| F7 | Transient/onset detector failure (no salient onsets for snap) | refinement stage | interpolated anchors used | none (logged); Issues entry if span is large | — |
| F8 | Corrupted file / damaged frames | decoder errors, CRC, length mismatch | frames skipped with exact-length silence fill (MP3) or refusal (lossless) | "3 damaged frames skipped near 2:13" | continue, or choose another file |
| F9 | Unsupported codec / >2 ch / over-limit duration | probe | nothing created | "This file is M4A/AAC (not supported yet)…" | convert losslessly; choose another file |
| F10 | Out-of-memory risk | pre-decode estimate vs budget | memory-mapped mode, or refusal | "Long file: using disk-backed mode" / "Too long (max 60 min)" | — |
| F11 | Model missing / hash mismatch / ORT init fails | load-time check | DSP fallback tracker | "Using basic beat detection (model unavailable)" | reinstall |
| F12 | Render failure (engine error, NaN, validation fail) | chunk validator | chunk falls back to single-pass re-render once; else marked failed | "Couldn't render 1:20–1:24" | retry; export blocked until fixed |
| F13 | Encoder failure (LAME DLL missing or error) | export worker | temp deleted, no target file | "MP3 export failed: encoder unavailable" | export WAV/FLAC; reinstall |
| F14 | Output path unavailable / no space / permission | pre-check (free space ≥ estimate × 1.2) + write errors | temp deleted | "Not enough space on D: (needs 210 MB)" | choose another folder |
| F15 | Destination equals source (or hard link) | file-ID check | refused before writing | "Can't overwrite the original" | choose another name |
| F16 | Source deleted / moved during session | size/mtime check at playback/export | read-only project | "Source file missing" banner | Locate… (PROJECT_SESSION_MODEL §6) |
| F17 | Source changed on disk | hash mismatch at export | export refused | "The source file changed since analysis" | re-analyse / relink |
| F18 | Cancellation (any job) | `stop_token` | previous valid state | nothing alarming ("Cancelled") | — |
| F19 | Application crash | next launch: dead lock + autosave | recovery prompt | "Recover unsaved changes?" | accept / discard |
| F20 | Incomplete render / export (crash, power loss) | temp files without commit; CRC on cache | temp swept at start; target never partial | nothing (export simply absent) | re-export |
| F21 | Audio device lost / format change | JUCE device callback | playback stopped, editing continues | "Audio device disconnected" | pick a device |
| F22 | Project file corrupt / unparsable | JSON/schema validation | not opened; `.bak` offered | "Project damaged. Open the backup?" | open `.bak` or autosave |
| F23 | Cache corruption | CRC / size check | entry deleted | none | transparent re-render |
| F24 | Peak overs on export | peak gate | export paused at gate | three choices (EXPORT_UI) | user choice |

## 3. Rules
1. **Never lose user data.** The source is never written. Projects and exports are written atomically. Autosave limits loss to ≤ 60 s.
2. **Never fail silently.** Every degradation (fallback tracker, skipped frames, B buffering while rendering) is visible in the quality chip or Issues list.
3. **No modal dead ends.** Every error message has at least one action.
4. **Fail before work.** Probe-time checks (codec, channels, duration, free space, destination) run before any long job.
5. Errors are logged once at the boundary where they are handled, with code, stage and redacted context ([LOGGING_AND_DIAGNOSTICS.md](LOGGING_AND_DIAGNOSTICS.md)).

## 4. Tests
Every row F1–F24 has at least one fault-injection or fixture test (AT-17…23 plus unit tests). The decoder fuzzing job covers F8. A table-driven test asserts that every `Error::Code` has user copy and a recovery action.
