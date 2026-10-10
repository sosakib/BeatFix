# ADR-010: Project Storage

**Status:** Accepted, 2026-10-08.

## Context
Projects must be non-destructive, reproducible, robust to crashes, and survive the source being moved. The brief requires that renders are reproducible from source + state.

## Alternatives
| Option | Verdict | Why |
|---|---|---|
| **JSON `.beatfix`, inputs only (source ref + fingerprints, analysis result, edits, settings, UI)** | **Chosen** | human-readable, diffable, easy migrations |
| SQLite project | Rejected | overkill for one document. Harder to diff and inspect |
| Binary format | Rejected | no benefit at this size |
| Embed audio / renders in the project | Rejected | huge files, duplicate source. Caches are rebuildable |
| Store the derived warp map as truth | Rejected | two sources of truth (ADR-005) |

## Decision
As specified in [PROJECT_SESSION_MODEL.md](../05_architecture/PROJECT_SESSION_MODEL.md): integer `schemaVersion` with pure migrations, unknown fields preserved, atomic save via `ReplaceFileW` + `.bak`, autosave every 60 s to `%LOCALAPPDATA%` with a session lock and recovery prompt, a 4-step source relink with file/PCM SHA-256 checks, and analysis results inline (activations in a hash-addressed sidecar).

## Tradeoffs
Moving a project to another machine without its cache means the activations sidecar is missing. Beats and tempo are still inline, so nothing user-visible changes. Re-running post-processing with new parameters then needs re-analysis (D2-level differences possible).

## References
[ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §8, [WARP_MAP.md](../04_algorithms/WARP_MAP.md).
