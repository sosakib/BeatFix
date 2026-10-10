# Coding Standards

Status: 2026-10-08, v1.0. Short on purpose: every rule here is either enforced by a tool or checked in review. Sources: [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §4, §13; [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1; brief §39 (deterministic, testable, no undocumented heuristics, no over-engineering).

## 1. Language level
- **C++20**, `/std:c++20`. No C++23 features (`std::expected`, `std::print`, deducing `this`, `std::mdspan`). Reason: DECISION_SUMMARY §1; keeps the MSVC `/std:c++latest` churn out of the build.
- Allowed and encouraged: `std::span`, `std::jthread` / `std::stop_token`, `std::atomic` (+ `is_always_lock_free` static asserts), `std::bit_cast`, `<numbers>`, designated initialisers, `constexpr` everywhere it fits, concepts for the few templates.
- Not used: modules (tooling maturity with JUCE/CMake), coroutines (no async framework needed), `std::format` in `[RT]` code (allocates).
- Third-party headers are included as `SYSTEM` so their warnings don't count.

## 2. Ponytail principle (no speculative abstractions)
- **Exactly two interfaces** exist: `ITimeStretcher` (Signalsmith now, Resample now, Bungee/Rubber Band behind flags: a real licence-driven swap) and `IBeatAnalyzer` (Beat This ONNX vs DSP tracker: a real fallback). Adding a third virtual interface needs an ADR.
- Resampler = one free function. Codec = JUCE's `AudioFormatReader`/`AudioFormatWriter` plus one `LameWriter` subclass. Warp map = one struct, one serialiser, one validator ([WARP_MAP.md](../04_algorithms/WARP_MAP.md)).
- No factories for one product, no config for values that never change, no plugin systems, no "manager" classes, no event bus. Free functions over classes when there is no invariant to protect.
- No code "for later". Multi-track, stems, per-section BPM, batch mode are Post-MVP; the module boundaries already allow them ([ADR-001](../09_decisions/ADR-001-desktop-framework-and-language.md)).
- A deliberate shortcut with a known ceiling gets a `ponytail:` comment naming the ceiling and the upgrade path, e.g. `// ponytail: O(n) scan over regions, binary search if regions > 1000`. These are harvested into a debt list before each release.

## 3. Realtime-safe rules (`[RT]` code)

`[RT]` code = anything reachable from the audio device callback. It lives only in `bf_engine/playback.cpp` and `bf_dsp` mixers/crossfades. Each such function carries `// [RT]` and may call only other `[RT]` functions or the allowed primitives below.

| MUST NOT | MAY |
|---|---|
| allocate or free (`new`, container growth, `std::string`, `juce::String`, creating `std::function` with captures, `shared_ptr` refcount reaching zero) | read immutable data reached through an atomically loaded pointer |
| lock (`std::mutex`, `CriticalSection`, `MessageManagerLock`, condition variables) | `std::atomic` load/store/exchange on lock-free types |
| file, network or console I/O; logging of any kind (`Logger`, `printf`, `OutputDebugString`) | pop from the bounded, preallocated SPSC command queue (POD commands) |
| throw, or call anything that may throw | write counters (xruns, max callback µs) as relaxed atomics |
| call ONNX Runtime, the stretcher, the decoder, or the OS for anything that can block | O(log n) searches (e.g. W⁻¹ segment lookup by binary search) |
| loop unboundedly; linear scans over beats/segments | bounded loops over the current block |
| first-touch freshly mapped or allocated memory (page faults) | read memory a worker has pre-touched |

Enforcement:
1. Review: any diff touching `[RT]` functions needs a second look against this table (PR template checkbox).
2. Debug/RelWithDebInfo allocation guard: the callback sets `thread_local bool tl_inRtCallback`; the replaced global `operator new`/`delete` assert if it is set ([BUILD_AND_DEPENDENCIES.md](BUILD_AND_DEPENDENCIES.md) §7). Playback tests run with it.
3. All `[RT]` functions are `noexcept`.
4. Diagnostics: xrun counter and max-callback-time are visible in the debug overlay and logged once per second by the UI thread ([THREADING_MODEL.md](../05_architecture/THREADING_MODEL.md)).

Buffer publication pattern is fixed: `std::atomic<const RenderGeneration*>` (release store on UI thread, acquire load on audio thread), retirement only after the audio thread's acknowledged generation id passes it, deletion on the UI thread. No `shared_ptr` reachable from `[RT]` code ([ADR-011](../09_decisions/ADR-011-threading.md)).

## 4. Errors: `Result<T>`, no exceptions across the module C API

```cpp
// bf_dsp/result.h  (the whole thing)
enum class ErrorCode : std::uint16_t { Ok = 0, Cancelled, InvalidInput, UnsupportedFormat,
    CorruptData, IoError, OutOfMemory, ModelUnavailable, AnalysisUnreliable, Internal };
struct Error { ErrorCode code = ErrorCode::Ok; std::string detail; };   // detail: English, for logs
template <class T> struct Result {
    T value{};  Error err{};
    bool ok() const noexcept { return err.code == ErrorCode::Ok; }
};
```
- Every fallible function in `bf_*` returns `Result<T>` (or `Error` for `void`). DECISION_SUMMARY fixes this shape; `T` must be default-constructible (wrap in `std::optional<T>` if not).
- **No exception crosses a module boundary or the C API** (`beatfix_c.h` functions are `noexcept` and return an error code + message buffer). JUCE, nlohmann/json and the C++ standard library may throw: the module that calls them catches at its boundary and converts to `Result`. `catch (...)` is allowed only at those boundaries and at thread entry points, and it must log and return `ErrorCode::Internal`.
- `bf_dsp` itself never throws; its functions are `noexcept` unless they allocate (allocation failure → `std::bad_alloc` caught by the caller's boundary).
- `ErrorCode` drives user messages; `detail` is for logs only (no paths beyond `fileHash[0:8]`, see [LOGGING_AND_DIAGNOSTICS.md](../05_architecture/LOGGING_AND_DIAGNOSTICS.md)). User-facing text lives in the UI layer ([ERROR_HANDLING.md](../05_architecture/ERROR_HANDLING.md)).
- Cancellation is a normal outcome (`ErrorCode::Cancelled`), not an error to log at warning level.
- `assert` is for programmer errors (violated preconditions inside a module) and stays enabled in RelWithDebInfo test builds. Data errors (corrupt file, NaN in input) are `Result` errors, never asserts.

## 5. Numbers, time and units

| Rule | Why |
|---|---|
| **All time and position values are `double` (seconds) or `std::int64_t` (sample frames).** Never `float`, never `int` | float32 cannot represent sample indices exactly above 2²⁴ ≈ 6.3 min at 44.1 kHz ([ADR-002](../09_decisions/ADR-002-internal-precision-and-sample-rate.md)) |
| Sample buffers are `float` (32-bit); accumulators (sums, filters, loudness, regression) are `double` | Memory/bandwidth vs precision split per DECISION_SUMMARY |
| Sample counts derive from **absolute** positions (`round(Winv(τ)·fs)` per block), never from accumulated rounded deltas | Accumulated rounding drifts tens of ms per song ([WARP_FUNCTION.md](../04_algorithms/WARP_FUNCTION.md) §7) |
| Units in names: `…Sec`, `…Frames`, `…Hz`, `…Db`, `…Bpm`, ratios `r…` documented as output/input | Unit bugs are the commonest DSP bug |
| Conversions seconds ↔ frames go through `bf::dsp::toFrames(double sec, double fs)` / `toSec(...)` with one rounding policy (round half away from zero) | Single-sourced rounding (QA_STRATEGY L1 "sample/time conversion") |
| Deterministic reductions: no `std::reduce` with parallel policies, no float atomics, fixed summation order, ties broken by lowest index | Determinism D1 |
| Denormals: set FTZ/DAZ on render and audio threads at thread start | Performance on decaying tails; deterministic per thread |
| Serialise doubles with shortest round-trip formatting (`std::to_chars`) | Byte-stable JSON round-trip ([WARP_MAP.md](../04_algorithms/WARP_MAP.md)) |

## 6. Naming and structure

| Item | Style | Example |
|---|---|---|
| Types, enums | `PascalCase` | `WarpMap`, `RenderGeneration` |
| Interfaces | `I` + PascalCase (only the two) | `ITimeStretcher` |
| Functions, variables | `camelCase` | `buildWarp`, `srcStartFrames` |
| Members | `camelCase`, no prefix; `m_` not used | `chunkReady` |
| Constants | `kPascalCase` | `kDefaultCutoffBeats` |
| Macros | `BF_UPPER_SNAKE` (avoid; only for build options) | `BF_WITH_RUBBERBAND` |
| Namespaces | `bf::dsp`, `bf::analysis`, `bf::io`, `bf::engine`, `bf::app` | |
| Files | `snake_case.{h,cpp}`; one main type or topic per file | `render_plan.cpp` |

- Ownership: `std::unique_ptr` by default; raw pointers/references are non-owning; `shared_ptr` only where ownership is genuinely shared and never on `[RT]` paths.
- `const` by default; pass `std::span<const float>` for read-only audio; output buffers as `std::span<float>`.
- Headers include what they use; no `using namespace` in headers.
- Every heuristic constant (thresholds, window lengths, chunk sizes, crossfade ms, confidence weights) lives in the module's `params.h` as `constexpr` with a comment: source doc § or test ID, and `PROVISIONAL` if not yet calibrated. Brief §39 "no undocumented heuristics".

## 7. Formatting (`.clang-format`)

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 110
PointerAlignment: Left
AllowShortFunctionsOnASingleLine: Inline
BreakBeforeBraces: Attach
IncludeBlocks: Regroup
SortIncludes: CaseSensitive
```
CI runs `clang-format --dry-run --Werror` on changed files. No manual formatting debates: the file wins.

## 8. Static analysis (`.clang-tidy`)

```yaml
Checks: >
  -*,
  bugprone-*, -bugprone-easily-swappable-parameters,
  performance-*,
  concurrency-mt-unsafe,
  cppcoreguidelines-pro-type-member-init, cppcoreguidelines-narrowing-conversions,
  cppcoreguidelines-init-variables, cppcoreguidelines-slicing,
  modernize-use-nullptr, modernize-use-override, modernize-use-using,
  readability-implicit-bool-conversion, misc-redundant-expression, misc-unused-parameters
WarningsAsErrors: 'bugprone-*,cppcoreguidelines-narrowing-conversions,performance-*'
HeaderFilterRegex: 'src/.*'
```
- `bugprone-easily-swappable-parameters` is disabled because DSP signatures legitimately take several numbers; unit-suffixed names cover the risk.
- `cppcoreguidelines-narrowing-conversions` is an error because `double → float` / `int64 → int` narrowing is exactly the time-precision bug class.
- Runs on `src/` only (not third-party), on changed files in PRs and on the whole tree nightly. MSVC `/W4 /WX` is the first line; clang-tidy is the second.

## 9. Testing rules
- Every pure function in `bf_dsp`/`bf_analysis` has Catch2 tests in `tests/unit` or `tests/dsp` ([QA_STRATEGY.md](../07_quality/QA_STRATEGY.md) §2).
- Every bug fix adds a regression test that fails without the fix.
- Tests are deterministic: fixed seeds in CI (random seed logged in nightly property runs), no wall-clock sleeps, no network, no dependence on thread scheduling.
- DSP tests use synthetic signals with exact ground truth; assertions use the tolerances in [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md), never "looks fine".
- Hard invariants (no NaN, exact length, source immutability, monotonic W, export round-trip) are never re-baselined.
- A test that is flaky is a bug: fix or delete it within the week, never retry-until-green.

## 10. Comments policy
- Comment **why**, not what. A comment that restates the code is deleted in review.
- Every non-obvious algorithm cites its doc and section (`// WARP_FUNCTION.md §4.2: Huber IRLS, k = 1.345`) or paper.
- `// [RT]` marks realtime functions; `// ponytail:` marks deliberate shortcuts; `// PROVISIONAL:` marks uncalibrated constants. `TODO` without an owner/issue is not merged.
- No commented-out code. No changelogs in comments (git history is the changelog).
- Public headers of `bf_*` document units, ownership, thread-safety ("UI thread only", "any thread", "[RT]") and error codes per function.

## 11. Review checklist (PR template)
- [ ] No JUCE types in `bf_*` public headers.
- [ ] `[RT]` changes checked against §3.
- [ ] Time values `double`/`int64_t`; no new narrowing.
- [ ] New heuristic constant in `params.h` with source.
- [ ] Tests added/updated; goldens re-baselined only with metric diff + changelog note.
- [ ] New dependency? Licence gate passes, `third_party/manifest.json` updated.
