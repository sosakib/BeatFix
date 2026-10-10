# ADR-007: Rendering Pipeline

**Status:** Accepted, 2026-10-08.

## Context
Edits (target BPM, detent, relabel) must be audible quickly (≤ 500 ms near the playhead), full renders must be ≤ 0.25 × RT, results must be identical for any core count, and nothing may ever render from a previous render.

## Alternatives
| Option | Verdict | Why |
|---|---|---|
| Single-pass whole-file render | Kept as reference/fallback | simple, but one core, so slow for preview |
| Realtime stretch in the audio callback | Rejected | heavy DSP on the RT thread, no lookahead for quality, A/B and validation harder |
| **Chunk-parallel offline render, cuts at low-RMS beats every 8–16 beats, pre-roll + overlap crossfade, content-addressed chunk cache, playhead-first scheduling** | **Chosen** | parallel, incremental, deterministic |
| Per-file cache keyed by settings | Rejected | any local edit re-renders everything |

## Decision
As specified in [RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md): chunk plan as a pure function of WarpMap, a fresh stretcher per chunk, SHA-256 content keys (the key is the invalidation rule), generations with atomic swap, B pausing (buffering) at unrendered chunks, and export only from a complete validated generation.

## Reasoning
Chunk keys make undo and A/B toggles free and local edits cheap. Cutting at beats with minimum short-term RMS keeps seams out of transients. Determinism comes from pure inputs and an index-ordered stitch.

## Tradeoffs
Seams are a new artifact source (mitigated by the null test vs single-pass ≤ −90 dBFS, PROVISIONAL). The disk cache grows (LRU 5 GB).

## Consequences
Phase 0 must measure the chunk-join null and Signalsmith pre-roll semantics. The single-pass path stays in the codebase permanently.

## References
[ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §5, [RENDER_PIPELINE.md](../05_architecture/RENDER_PIPELINE.md), [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md).
