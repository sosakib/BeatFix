# Security and Privacy

Implements REQ-SEC-001…008 ([SOFTWARE_SYSTEM_REQUIREMENTS.md](SOFTWARE_SYSTEM_REQUIREMENTS.md) §19). Brief §37.

## 1. Position
BeatFix is **offline-first and local-only**. User audio never leaves the machine, and the app has no account, cloud, telemetry or update check. This is a product feature (COMPETITIVE_ANALYSIS: cloud "Suno fixers" are the alternative). It also keeps the attack surface small: the only untrusted inputs are **files the user opens** (audio, projects, model file).

## 2. Data inventory

| Data | Where | Contains personal data? | Lifetime |
|---|---|---|---|
| Source audio | user location, read-only | possibly (user's music) | user-owned |
| PCM / render cache | `%LOCALAPPDATA%\BeatFix\cache` (per user) | derived audio | LRU-evicted, "Clear cache" |
| Project `.beatfix` | user location | source path, file names, edits | user-owned |
| Autosave + lock | `%LOCALAPPDATA%\BeatFix\autosave` | as project | deleted on clean close |
| Logs | `%LOCALAPPDATA%\BeatFix\logs`, rotating 7 files × 10 MB | **no audio, no full paths, no user names** (file names hashed to 8 hex chars) | rotated |
| Crash dumps | `%LOCALAPPDATA%\BeatFix\crashes` | minidump **without** full memory (no audio buffers); never uploaded | on by default, can be disabled in Settings. The user decides whether to share one |
| Diagnostics bundle | created only by explicit user action | logs + analysis summary, preview shown before saving | user-owned |

## 3. Controls

| Control | Mechanism | Verified by |
|---|---|---|
| No network | No HTTP client is linked. ONNX Runtime `DisableTelemetryEvents()` is called before session creation. The installer does not register update tasks | AT-24 (network monitor), SBOM review |
| Log hygiene | One logging façade that accepts only typed fields. Paths go through `redactPath()` | log-grep test in CI |
| Temp files | Created in the cache dir with `FILE_ATTRIBUTE_TEMPORARY`, deleted on success or failure, swept on next start | AT-23 |
| Source protection | Source opened `GENERIC_READ`, share-read. Export refuses any destination whose `FILE_ID_INFO` equals the source's (catches hard links) | AT-20, REQ-SEC-006 |
| Atomic writes | temp file + `ReplaceFileW` / `MoveFileExW(MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)` | AT-21 |
| Model integrity | SHA-256 of the ONNX file checked against the value compiled into the binary. A mismatch falls back to the DSP tracker with a message | REQ-SEC-008 |
| Binary integrity | Installer and exe are code-signed ([RELEASE_STRATEGY.md](../08_engineering/RELEASE_STRATEGY.md)) | release checklist |

## 4. Threat model (STRIDE-lite)

| Threat | Vector | Impact | Mitigation |
|---|---|---|---|
| Malicious audio file | crafted WAV/FLAC/MP3/Ogg exploiting a decoder bug | code execution | Decoders fuzzed in CI (libFuzzer on minimp3/JUCE readers), bounds-checked probe, size and duration limits at probe, ASan job |
| Malicious project file | crafted JSON (huge arrays, paths to system files) | DoS, writing to unexpected paths | Schema validation with size limits. Projects only *reference* sources (read-only). The export path always comes from the dialog, never from the project file alone |
| Tampered model file | replaced `.onnx` | wrong beats, or an ORT parser exploit | Hash check before load (§3) |
| DLL hijacking | planted `lame_enc.dll` / `onnxruntime.dll` next to the exe or in CWD | code execution | `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR|SYSTEM32)`. DLLs loaded by full path from the install dir. Signed DLLs where available |
| Data leakage | logs or crash dumps shared by the user | path or name disclosure | redaction (§3); dumps opt-in and minimal |
| Accidental data loss | overwriting the source or an existing file | user's music lost | source-ID check, overwrite confirmation, atomic writes |

Out of scope: an attacker with local admin. DRM. Content provenance or watermark detection.

## 5. Privacy implications
- There is no processing of personal data by the vendor, because nothing is transmitted. A privacy notice only needs to state this, plus the local crash dumps (never uploaded).
- Suno-generated files may carry provenance tags. BeatFix **keeps** provenance and comment fields on export by default (AUDIO_QUALITY_RESEARCH §metadata) for transparency.
- Diagnostics bundles are created only on request and are previewed before saving. The user decides whether to send one anywhere.

## 6. Open items
- Whether a future opt-in update check is wanted (it would need a privacy notice and REQ-SEC-001 to be amended). Default: no.
- Fuzzing coverage targets for the JUCE FLAC/Ogg readers (vendor code), measured in Phase 1.
