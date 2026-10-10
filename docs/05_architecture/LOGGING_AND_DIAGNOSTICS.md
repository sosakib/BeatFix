# Logging, Diagnostics and Observability

Implements REQ-SEC-002…004. Privacy rules: [SECURITY_AND_PRIVACY.md](../02_requirements/SECURITY_AND_PRIVACY.md). This doc also covers "observability" (brief §30 `OBSERVABILITY.md` is merged here; the app has no telemetry).

## 1. Logs
- `juce::FileLogger` behind a thin façade `bf::log(level, event, fields…)`. spdlog is added only if async logging is ever needed.
- Location `%LOCALAPPDATA%\BeatFix\logs\beatfix-YYYYMMDD.log`, kept as 7 files × 10 MB.
- Levels: `error`, `warn`, `info` (default), `debug` (Settings → "Verbose diagnostics", off by default).
- Format: one line per event, `ts level event key=value…`, so it can be grepped.

### What is logged
App/OS/CPU/device info at start, library versions, model hash, stage durations (decode, analysis per stage, render per chunk, export), cache hit/miss counts, error codes with stage, peak-gate decisions, and recovery events.

### What is never logged
Audio samples, analysis arrays, full paths (file names become `sha256[0:8].ext` unless verbose mode is on), user names, project contents. A CI test greps logs from the AT suite for drive letters, `\Users\` and fixture file names.

### Realtime thread
The RT thread never logs. It increments atomics (xruns, callback overruns, max callback µs, B-buffering events). The UI thread emits deltas once per second when they are non-zero.

## 2. Crash dumps
`SetUnhandledExceptionFilter` → `MiniDumpWriteDump` with `MiniDumpWithIndirectlyReferencedMemory` (no full memory, so no audio buffers) into `%LOCALAPPDATA%\BeatFix\crashes\`. On the next launch: "BeatFix closed unexpectedly. Open crash folder?" Dumps are never uploaded. A setting disables them.

## 3. Diagnostics bundle (user-initiated)
Help → "Save diagnostics…" writes a zip containing recent logs, system info, the analysis summary (BPM, confidence, stability class, counts; no beat arrays unless ticked), the warp-map diagnostics block and render timings. A preview of the contents is shown before saving. Nothing is sent anywhere.

## 4. Built-in diagnostics views (developer/advanced)
- **Analysis overlay** (debug builds and a hidden Settings toggle): activation curves, onset function, refined vs raw beat positions, residuals vs smoothed tempo.
- **Render HUD**: per-chunk render time, cache hits, generation number, B-buffering spans.
- **Perf counters** (Help → About → Performance): last analysis/render × realtime, peak memory, xruns. These feed manual REF-MID benchmark reports.

## 5. Development observability
- `bench/` Python harness (mir_eval) writes JSON reports per corpus run. CI stores them as artifacts and compares against the last baseline ([CI_CD.md](../08_engineering/CI_CD.md)).
- A CLI target (post-MVP) reuses the same logging façade, which makes batch reproduction of user issues possible from a diagnostics bundle.
