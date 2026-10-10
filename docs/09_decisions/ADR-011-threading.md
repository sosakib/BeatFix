# ADR-011: Threading Model

**Status:** Accepted, 2026-10-08.

## Context
Playback must never glitch while decode, ONNX inference, rendering and export run, and every long job must cancel within 250 ms.

## Alternatives
| Option | Verdict | Why |
|---|---|---|
| Single worker thread for all background work | Rejected | analysis blocks preview rendering. No parallel render |
| Mutex-protected shared state with the audio thread | Rejected | priority inversion and glitches |
| **Dedicated threads per role + render pool; SPSC lock-free queues and atomics to the RT thread; atomic buffer swap with deferred free; `std::stop_token` cancellation** | **Chosen** | standard realtime-audio practice. Minimal machinery |
| Async/coroutine framework | Rejected | extra dependency, no benefit for a handful of job types |

## Decision
Threads: UI (message thread), realtime audio callback, import/I/O, analysis, peaks, render pool (cores − 1), export. The RT thread never allocates, locks, logs or does I/O. ONNX inference is cancelled via `RunOptions::SetTerminate()`. Full rules and sequence diagrams: [THREADING_MODEL.md](../05_architecture/THREADING_MODEL.md).

## Tradeoffs
Deferred-free and generation bookkeeping add complexity, but this is the minimum for glitch-free buffer swaps.

## Consequences
Debug builds install an allocation/lock detector on the RT thread (REQ-PLAY-005), and CI runs an ASan job.

## References
[ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §4, [THREADING_MODEL.md](../05_architecture/THREADING_MODEL.md).
