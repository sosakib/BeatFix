# Time-Stretch Engine

Status: algorithm/architecture spec, 2026-10-08. Authoritative decisions: [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1 (rows Time-stretch, Resample, Transient mode, Quality reference, Rendering). Research: [TIME_STRETCH_RESEARCH.md](../03_research/TIME_STRETCH_RESEARCH.md) (TSR), [DSP_RESEARCH.md](../03_research/DSP_RESEARCH.md) (DSPR), [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) §5 (render/cache). Warp maths: [WARP_FUNCTION.md](WARP_FUNCTION.md). Covers brief §7.10, §8, §9 and §26 Q9–18.

## Objective
Specify the render engine that turns (source PCM, warp map W) into the corrected audio: the `ITimeStretcher` contract, the Signalsmith "Smooth" adapter, the constant-ratio "Resample" mode, the identity bypass, ratio bounds, the chunked parallel render contract, the reference/alternate engines, and the algorithm-family comparison that justifies the choice.

## Why It Matters
The stretch engine is the only stage whose errors the user cannot fix in the UI. Two failure classes matter: **timing** (latency not compensated, rounding drift, chunk misplacement: silently breaks the product promise) and **quality** (phasiness, smear, seams, stereo collapse). Timing is fully testable and must be exact. Quality is partly testable and finally decided by listening ([SUBJECTIVE_LISTENING_TESTS.md](../07_quality/SUBJECTIVE_LISTENING_TESTS.md)).

---

## 1. Architecture

```
                       immutable inputs                             derived, cached
 ┌──────────────┐   ┌──────────────┐   ┌───────────────────┐   ┌───────────────────────────┐
 │ PCM cache    │   │ WarpMap      │   │ RenderPlan        │   │ RenderGeneration          │
 │ f32 planar,  │──▶│ W, W⁻¹ (f64) │──▶│ chunks c_k (i64), │──▶│ prealloc dst + chunkReady │
 │ native fs    │   │ ratios, mode │   │ H, O, preset      │   │ (atomics)                 │
 └──────────────┘   └──────────────┘   └─────────┬─────────┘   └─────────────▲─────────────┘
                                                 │ one job per chunk         │ assemble (ordered,
                                                 ▼                           │ ρ-adaptive xfade)
                          ┌──────────────── render pool (N workers) ─────────┴───────┐
                          │  worker i owns one prepared ITimeStretcher instance      │
                          │   ├─ SmoothStretcher   (Signalsmith 1.4.0)   MVP default │
                          │   ├─ ResampleStretcher (r8brain, const r)    MVP option  │
                          │   ├─ HpssHybrid<Smooth> ("Transient")        Phase 1.5   │
                          │   ├─ BungeeStretcher                         fallback    │
                          │   └─ RubberBandR3Stretcher                   dev/QA only │
                          └──────────────────────────────────────────────────────────┘
                                                 │
                                                 ▼
                      post-render validation (QUALITY_VALIDATION.md) → export chain
```

Rules (all High confidence, TSR Recommendation 1, 10):
1. Engines never see beats. They see `W⁻¹` sampled on their own fixed output hop.
2. All time and position values are f64 seconds/samples or i64 sample indices. Audio is f32. (DECISION §1 "Internal precision".)
3. No engine processes channels independently (§7 and [STEREO_PHASE_PRESERVATION.md](STEREO_PHASE_PRESERVATION.md)).
4. Render at the native source rate. The engines never resample for SRC purposes ([RESAMPLING.md](RESAMPLING.md) policy table).

## 2. `ITimeStretcher` contract

This supersedes the sketch in ARCHITECTURE_RESEARCH §3.2 in one point: warp segment endpoints are **f64 samples**, not i64, so anchors keep sub-sample precision until the adapter rounds absolute positions.

```cpp
// bf_dsp/stretch.h  (std only, no JUCE types)
struct WarpView {                         // immutable view of WarpMap, units: samples at source fs (f64)
    double tauOrigin;                     // output-timeline position of output sample 0 (see §2.3)
    double W(double s) const;             // source → output, piecewise linear, edges extrapolated (WARP_FUNCTION §6)
    double Winv(double tau) const;        // output → source, O(log n) binary search
    double ratioAt(double tau) const;     // r = dW/ds (out/in) at W⁻¹(tau)
    bool   isConstantRatio(double* r) const;   // all segment + edge ratios equal (rel. 1e-12)
};
struct StretchParams { int channels; double fs; Preset preset; /* Fast | High */ };
enum class RenderStatus { Ok, Cancelled, InvalidWarp, EngineError };

class ITimeStretcher {
public:
    virtual ~ITimeStretcher() = default;
    virtual void prepare(const StretchParams&) = 0;          // all allocation here
    virtual int  outputHop() const = 0;                       // fixed synthesis hop Hs (samples)
    virtual int  contextBefore() const = 0;                   // source samples needed before W⁻¹(τ_begin)
    virtual int  contextAfter() const = 0;                    // source samples needed after W⁻¹(τ_end)
    // Render output samples [nBegin, nEnd) (output-timeline indices). Source is zero-extended
    // outside [0, srcFrames). Output sample n corresponds to τ = tauOrigin + n.
    // Must be a pure function of (src, W, params, nBegin, nEnd): no hidden state across calls.
    virtual RenderStatus render(std::span<const float* const> src, int64_t srcFrames,
                                const WarpView& W, int64_t nBegin, int64_t nEnd,
                                std::span<float* const> dst, const std::stop_token&) = 0;
    virtual std::string_view id() const = 0;                  // "signalsmith@1.4.0+<sha8>/High" → cache key
};
```

### 2.1 Contract invariants (tested, §11)
| # | Invariant | Why |
|---|---|---|
| C1 | Output sample n is placed at τ = tauOrigin + n with **zero** latency bias (|mean e| ≤ 0.5 ms, O1) | Latency errors shift every beat |
| C2 | `dst` length = nEnd − nBegin exactly | Assembly and O17 length check |
| C3 | Input positions computed from absolute `round(W⁻¹(τ))`, never accumulated | WARP_FUNCTION §7: accumulated rounding drifts tens of ms |
| C4 | Same inputs → bit-identical output (same build/CPU path) | D1 determinism, chunk cache ([DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) §6) |
| C5 | Stop token checked at least once per output hop | Cancel ≤ 250 ms (DECISION §6, performance row) |
| C6 | No NaN/Inf output for any valid W (ratios within hard bounds, §5) | Hard gate O16 |
| C7 | Channels linked: `src` L≡R ⇒ `dst` L≡R; L=−R ⇒ dst L=−R | O13 a/b hard requirements |

### 2.2 Ratio conventions (convert only at the adapter)

| System | Quantity | Relation to BeatFix r = out/in | Verified |
|---|---|---|---|
| BeatFix | r_i = (T_{i+1}−T_i)/(x_{i+1}−x_i) | r | WARP_FUNCTION §1 |
| Signalsmith | per-call `inputSamples`, `outputSamples` | out/in of each call ≈ local r | Context7 `/signalsmith-audio/signalsmith-stretch` |
| Signalsmith | `playbackRate` argument (`outputSeekLength`, `seek`, `flush`, inferred in `exact`) | **1/r** (`playbackRate = inputSamples/outputSamples`) | same |
| Rubber Band | `setTimeRatio` | r (must equal total out/in) | TSR Finding 8 |
| Rubber Band | `setKeyFrameMap` | source frame → output frame (`size_t`) | TSR Finding 8 |
| Bungee | `Request.speed`; `Request.position` | 1/r; source frame of grain centre | TSR Finding 7 (field semantics re-verify, LIBRARY_EVALUATION Open Q6) |
| Varispeed / r8brain | SRC with fs_in = fs, fs_out = fs·r, played back at fs | pitch factor 1/r | arithmetic |

### 2.3 Output origin (gap flagged)
W(0) is generally not 0 (Preserve-start keeps the first downbeat fixed, so the pre-roll is warped by r_0 and `W(0) = T_0 − r_0·x_0`). The engine renders the output interval `[tauOrigin, W(L_src))` with **`tauOrigin = min(0, W(0))`**: output samples whose `W⁻¹` falls before the source start read zeros; no source audio is ever trimmed. Consequence: when W(0) < 0 (pre-roll stretched) the first downbeat lands |W(0)| later than in the source (|1 − r_0|·x_0, e.g. 20 ms for x_0 = 1 s, r_0 = 1.02). TARGET_BEAT_GRID.md §3 does not define this case; proposal reported in the hand-back. Leading silence for DAW-ready start is prepended after rendering and never changes W (WARP_FUNCTION §6).

Expected length: `N_out = round((W(L_src) − tauOrigin)·fs)`, plus any prepended silence (used by QUALITY_VALIDATION V2).

## 3. "Smooth": Signalsmith Stretch 1.4.0 adapter

### 3.1 Configuration
| Item | Value | Basis |
|---|---|---|
| Instance | one multi-channel instance per render worker, `channels` = 1 or 2 | linking is per instance (TSR Finding 5) |
| Preset `High` (default for B playback when quality = High, always for export) | `presetDefault(channels, fs)`: block 120 ms, interval 30 ms (5292/1323 at 44.1 kHz) | TSR TD-3, DSPR TD-1 (verified in source) |
| Preset `Fast` (preview while editing) | `presetCheaper(channels, fs)`: 100 ms / 40 ms | TSR TD-3. Separate cache key. Lane shows "Preview: Fast" ([PLAYBACK_AND_AB.md](../06_ui/PLAYBACK_AND_AB.md) §4) |
| Longer "HQ" block (160–200 ms / 40 ms) | **not exposed**. Experiment in the Phase 0 listening pilot only | Benefit UNVERIFIED (TSR Rec. 2). Longer blocks also lengthen smear |
| Transpose / formant | library defaults, never set (pitch = 1) | Pitch preservation is the default behaviour |
| `splitComputation` | off | It adds one interval of output latency and only helps realtime callbacks |
| `exact()` | **not used** | Constant ratio only (TSR TD-4) |
| Build | `/O2`, `/fp:precise`, no fast-math. Debug builds ≈10× slower: benchmark Release only | LIBRARY_EVALUATION §B; ARCHITECTURE §coding standards |
| Version id | git tag + commit SHA, not the header `version` constant (reads 1.3.2 at tag 1.4.0, UNVERIFIED lag) | TSR "UNVERIFIED" list |

### 3.2 Latency model and drive equation (verified API, derived drive)
Verified (Context7): `inputLatency()` = analysis latency (input must be supplied this far ahead of the processing time); `outputLatency()` = synthesis latency (output arrives this far behind the processing time); `outputSeekLength(rate) = inputLatency() + rate·outputLatency()`; `outputSeek(inputs, len)` performs `reset()`, consumes `len` input frames, and aligns the **next `process()` output with `inputs[0]`**; `process(in, nIn, out, nOut)` with differing counts; input and output buffers must be distinct.

Derived drive equation. Let L_in = `inputLatency()`, L_out = `outputLatency()`. The engine's processing head runs L_out output samples ahead of what it emits, so when the emitted output has reached output position τ, the supplied input must end at

```
feedEnd(τ) = round( W⁻¹(τ + L_out) ) + L_in            (i64, absolute, never accumulated)
```

At a constant rate this equals `W⁻¹(τ) + L_in + (1/r)·L_out` = `W⁻¹(τ) + outputSeekLength(1/r)`, i.e. exactly the library's own seek rule, so the start-up seek length is `feedEnd(τ0) − round(W⁻¹(τ0))`. Under a varying ratio, evaluating W⁻¹ at `τ + L_out` makes the engine see the ratio of the material it is actually synthesising. TSR TD-4 omitted the `+ L_out` look-ahead; at piecewise ratio steps of ≤ 1 % its effect is ≤ 0.01·L_out (≈ 0.5 ms), but it is free to do correctly. **The drive equation is a derivation; test TSE-1 (impulse alignment at constant and piecewise ratios) is the acceptance criterion** (TSR Open Q2).

### 3.3 Pseudocode
```cpp
RenderStatus SmoothStretcher::render(src, srcFrames, W, nBegin, nEnd, dst, st) {
    const int Lin = eng.inputLatency(), Lout = eng.outputLatency(), Hs = eng.intervalSamples();
    auto tau     = [&](int64_t n) { return W.tauOrigin + double(n); };
    auto feedEnd = [&](int64_t n) { return llround(W.Winv(tau(n) + Lout)) + Lin; };

    const int64_t seekStart = llround(W.Winv(tau(nBegin)));
    int64_t fed = feedEnd(nBegin);
    copyZeroExtended(src, srcFrames, seekStart, fed - seekStart, scratchIn);   // prealloc; zero outside [0, srcFrames)
    eng.outputSeek(scratchIn, int(fed - seekStart));                          // reset + pre-roll; next out ↔ seekStart

    for (int64_t n = nBegin; n < nEnd; ) {
        if (st.stop_requested()) return RenderStatus::Cancelled;
        const int nOut = int(std::min<int64_t>(Hs, nEnd - n));
        const int64_t next = feedEnd(n + nOut);
        const int nIn = int(next - fed);                      // ≥ nOut/2 because r ≤ 2 (hard bound)
        copyZeroExtended(src, srcFrames, fed, nIn, scratchIn);
        eng.process(scratchIn, nIn, outView(dst, n - nBegin), nOut);
        fed = next; n += nOut;
    }
    return RenderStatus::Ok;
}
// prepare(): scratchIn sized for max(seek length at r = 0.5, 2·Hs + 2) frames per channel.
// End of file: the source is zero-extended, so the last calls simply feed zeros. flush() is not
// needed for an exact-length render. If TSE-12 finds a tail artefact, switch to the README recipe
// (process inputLatency() of silence, then flush()).
```

### 3.4 Known engine properties that shape the design
- Stereo linked per bin to the max-energy channel, input IPD re-applied (TSR Finding 5, verified in source).
- No explicit transient detector; vertical coherence from neighbour-bin prediction (TSR Finding 6). Transient work happens in a wrapper ([TRANSIENT_PRESERVATION.md](TRANSIENT_PRESERVATION.md)).
- Above 2× stretch it randomises per-bin time factors (`maxCleanStretch = 2`). Hard bounds (§5) keep us at ≤ 2, so no RNG is reached.
- **Phase history (derived, verify in Phase 0):** each output bin's phase is predicted from the previous *output* frame, so the absolute synthesis phase carries the whole render history. Two renders of the same span that started at different points (a chunk vs the single-pass render) produce the same magnitudes but different per-partial phase offsets. Consequences: (a) a chunked render will **not** null against the single-pass render; (b) chunk crossfades mix partially decorrelated signals. §8 is designed around this.

## 4. "Resample" mode (varispeed, constant ratio only)

| Rule | Spec | Why |
|---|---|---|
| Availability | Enabled only when `W.isConstantRatio(&r)` (in practice the **Global** correction detent with no honoured change points). Otherwise the menu item is disabled with the tooltip "Resample needs one constant tempo ratio. Choose Global correction." | A per-beat varying ratio becomes pitch vibrato (DSPR TD-4) |
| Algorithm | `resampleOffline(src, fs, fs·r)` (r8brain-free-src, [RESAMPLING.md](RESAMPLING.md) wrapper: zero delay, exact length). Output reinterpreted at fs | One bandlimited SRC, no TSM artefacts |
| Wrapper requirement | accepts f64 rates (fs·r is not an integer). r8brain takes arbitrary ratios (RESAMPLING Key Findings); exact class/ctor signature UNVERIFIED | — |
| Length / timing | `N_out = round(N_in·r)`; output sample 0 ↔ source 0 (`tauOrigin = W(0) = b` for `W(s) = r·s + b`). Verified by TSE-10 click test | C1, C2 |
| Pitch display | `cents = 1200·log2(1/r)`, signed, 1 decimal: "Pitch +31.8 cents (higher)" for 108→110 BPM | TSR TD-10 table: 1 % ≈ 17.2 c |
| Chunking | none; single pass (cost ≪ Smooth) | Avoids seams entirely |
| Stereo | identical filter on all channels → exact image | DSPR TD-4 |
| Peaks | SRC Gibbs overshoot possible → post-render gate as for every mode | RESAMPLING R12 |

## 5. Ratio bounds

| Bound | Value | Where enforced | User sees |
|---|---|---|---|
| Hard per segment | 0.5 ≤ r_i ≤ 2.0 | `WarpMap::validate()` before any render (WARP_FUNCTION §3) | Render refused for that map: "Correction too large at bar n (likely half/double-time). Fix the beat interpretation." with [Go to] |
| Warn per segment | 0.92 ≤ r_i ≤ 1.08 | validate → `diagnostics.warnSegments` | Segment highlighted; Review issue |
| Global tempo change | \|log2 g\| > 0.2 (g < 0.871 or > 1.149), g = B_target/B_detected | engine comfort check (TSR TD-9; Signalsmith sweet spot 0.75–1.5×) | Review chip "Large tempo change: quality may drop" (fills MAIN_WINDOW.md §5 "pending, e.g. > 15 %") |
| Engine | Signalsmith clean ≤ 2×; Bungee any; RB R3 tested range not stated | adapters assume hard bounds hold | — |

Note: METRICS.md O5 uses "e.g. [0.8, 1.25]" as an example clamp; the values above (WARP_FUNCTION §3) are the binding ones.

## 6. r = 1: bypass and passthrough

**Whole-song identity bypass (MVP).** If the warp is a pure shift within tolerance,
```
Δ = median_i(T_i − x_i);   dev = max over t ∈ {0, x_0..x_n, L_src} of |W(t) − t − Δ|
if dev ≤ ε_bypass (0.5 ms, PROVISIONAL = O1 latency-bias bound):   output = source delayed by round(Δ·fs) samples
```
the output is a sample-exact copy of the source (shifted, zero-padded at the start if Δ > 0). The engine is not run. This makes the brief §44 "stable song stays effectively unchanged" test bit-exact (O18, GOLD-01) at zero risk (no seams: the whole file is one region). UI: "No tempo correction needed. Output is sample-identical to the source." ε_bypass is set at the O1 bias bound so the bypass can never fail the alignment gates.

**Regional r = 1 (inside a warped song, incl. "keep section unwarped" for intentional tempo changes, DECISION §6).** MVP renders these spans through Smooth at ratio 1. A textbook PV with Ha = Hs is the identity (TSR TD-3); whether Signalsmith at rate 1 nulls is **UNVERIFIED**: O18 measures it (PROVISIONAL ≤ −60 dB). The research's regional **Passthrough** (copy + ρ-adaptive crossfades, TSR TD-8) is **Phase 1.5, behind a setting**, and ships only if DSP-REN-3 shows no seam flags and LIS-ABX shows its seams are no more audible than near-identity PV (TSR Open Q5). Reason: a seam on a dense mix is a new artefact, and near-identity PV may already be transparent.

## 7. Linked stereo, mono, > 2 channels
One multi-channel engine instance per render; never two mono instances (TSR TD-5: independent L/R drifts IPD and widens the image). Mono sources render with `channels = 1` and export mono (never upmixed). > 2 channels are rejected at import in the MVP ([UX_BEHAVIOR.md](../06_ui/UX_BEHAVIOR.md) §2, §15). Details and metrics: [STEREO_PHASE_PRESERVATION.md](STEREO_PHASE_PRESERVATION.md).

## 8. Chunked parallel rendering contract

DECISION §1 "Rendering": chunked parallel render at quiet beat boundaries (8–16 beats), pre-roll + crossfade, deterministic regardless of core count, content-addressed chunk cache, single-pass fallback. ARCHITECTURE_RESEARCH §5 gives the scheduling and cache. This section fixes the numeric contract.

### 8.1 Plan (pure function of source PCM and W)
1. **Cut candidates (source time):** for each anchor interval [x_i, x_{i+1}), the minimum of short-term RMS (10 ms window, hop 1 ms) inside the middle half of the interval. Refinement of ARCHITECTURE §5 step 1 ("cut at the beat"): beats are where transients are, so the cut goes between them.
2. **Cut choice:** walk the song in windows of 8–16 beats; in each, pick the candidate with minimum RMS whose crossfade span `[c, c+O)` (mapped through W) has no detected onset within 5 ms before its end (TSR TD-8 rule 2). Ties → earliest. The search is local to the window, so a local warp edit moves only local cuts (needed for local cache invalidation).
3. Output cut index `c_k = round((W(t_cut,k) − tauOrigin)·fs)` (i64). c_0 = 0, c_K = N_out.

### 8.2 Per-chunk job
| Parameter | Default | Basis |
|---|---|---|
| Warm-up handle H (rendered, discarded) | max(blockSamples + intervalSamples, 250 ms) | TSR TD-8 rule 1. Lets phase/vertical prediction settle before kept output |
| Overlap O (rendered past the cut) | 80 ms, **PROVISIONAL** (range 50–100 ms) | ARCHITECTURE §5 step 2; tuned by TSE-6 |
| Render span | output `[c_k − H, c_{k+1} + O)`, clamped to `[−H, N_out)` | outputs before 0 discarded; source zero-extended |
| Instance | the worker's own prepared instance; `outputSeek()` inside `render()` resets it | no state across chunks (C4) |

### 8.3 Assembly (single thread, chunk order)
```
for k in 0..K-1:
    keep chunk k on [c_k, c_{k+1})
    if k > 0: crossfade over [c_k, c_k + O):  prev = chunk k-1 tail, next = chunk k head
        ρ = Σ prev·next / sqrt(Σprev² Σnext²)      (f64, all channels pooled, over the overlap)
        ρ = clamp(ρ, 0, 1)
        for t in [0,1):  g_b = shape(t, ρ); g_a = solve(g_a² + g_b² + 2ρ g_a g_b = 1, g_a ≥ 0)
            with g_b(t) = (1−ρ)·sin(πt/2) + ρ·t            # equal-power at ρ=0, linear at ρ=1
        same gains on every channel
```
ρ-adaptive gains (TSR TD-8 rule 4) are needed because chunk renders are partially decorrelated (§3.4); a fixed linear fade would dip by up to 3 dB on tonal material. Within the rules of §2.1, assembly in a fixed order with f64 sums makes the result independent of thread count and scheduling (D1).

### 8.4 Determinism and the single-pass reference
- Plan, H, O, preset and instance state are pure functions of the inputs → **D1 bit-identical** for 1, 2, 4, 8 threads (TSE-5).
- Single-pass render = the same driver with one chunk `[−H, N_out)`. Because of phase history (§3.4) the chunked render **does not null** against it. DSP_TEST_STRATEGY DSP-REN-1's "null vs single-pass ≤ −90 dBFS" is therefore expected to fail for Smooth; replace it by the seam-quality test TSE-6 (flagged in hand-back). Null tests remain valid for Resample and for the bypass.
- **Fallback:** if post-render validation flags a discontinuity within ±O of a seam (QUALITY_VALIDATION V7), re-render chunks k−1 and k as one merged chunk and re-check. If it persists, render the export single-pass (DECISION §1 fallback).

### 8.5 Cache and invalidation
Chunk key = ARCHITECTURE §5 `chunkKey` plus `H`, `O`, preset, plan-algorithm version, and the chunk's W⁻¹ segment list over its span (f64, canonical). The context a chunk depends on is its render span plus engine context: `P_ctx = H + O + ⌈L_out/r_min⌉ + L_in` source samples on each side. An edit invalidates every chunk whose padded source span intersects a changed segment (fills [UX_BEHAVIOR.md](../06_ui/UX_BEHAVIOR.md) §9 "pending P_ctx"). Live (realtime) preview of unrendered chunks is **Post-MVP**; MVP uses the buffering rule of PLAYBACK_AND_AB.md §4 rule 4.

## 9. Reference and alternate engines

**Rubber Band R3 (dev/QA builds only; never distributed, GPL).** Build option `BEATFIX_DEV_RUBBERBAND=ON`, excluded from release presets; the CI licence gate fails if `rubberband` appears in a release artefact's dependency list. Offline whole file: options `OptionProcessOffline | OptionEngineFiner | OptionChannelsTogether`; `setTimeRatio(N_out/N_in)`; `setKeyFrameMap({round(x_i·fs) → round((T_i − tauOrigin)·fs)})` (must follow `setTimeRatio`, precede first `process`); `study()` the whole file; `process()`/`retrieve()`. Single-threaded, no chunking. Uses: listening-test reference (SUBJECTIVE_LISTENING_TESTS §3), relative transient metrics (DSP-STR-3), Phase-0 threshold calibration (DSP_TEST_STRATEGY §7). Keyframe hit accuracy is **UNVERIFIED** (TSR Open Q4): run TSE-1 on it; if it misses anchors by > 1 ms, compare it only on quality metrics, not timing.

**Bungee (MPL-2.0, unmodified) – fallback.** Adapter: for each grain, `position = W⁻¹(output grain centre)`, `speed = 1/r` there, `pitch = 1`, `reset = false`; loop `specifyGrain → analyseGrain → synthesiseGrain`. Output-chunk ↔ grain-centre bookkeeping and Basic-edition stereo linking are **UNVERIFIED**. Bungee becomes eligible only after it passes TSE-1 and TSE-4 (O13 a/b exact). MPL obligations: ship unmodified files or publish modified ones (for legal review, LICENSING_RESEARCH).

## 10. TSM method comparison (brief §7.10)

Notation (TSR TD-1): N window, Ha analysis hop, Hs synthesis hop, α = Hs/Ha = r. BeatFix regime: |1 − r| ≤ 5 % locally, piecewise per beat, dense lossy full mixes.

| Method | Assumption | Core equation | Cost (offline) | Characteristic artefacts | Full-mix fit at ±5 % | BeatFix use |
|---|---|---|---|---|---|---|
| OLA | none (no alignment) | `y(t)=Σ_m w(t−mHs)x(t−mHs+mHa)/Σ_m w(t−mHs)` | trivial | warble/phase jumps on periodic content; fine on noise/percussion with N ≈ 10 ms | Poor (tonal) | Percussive branch of Transient mode only (N 512, Hs 256 @ 44.1 kHz) |
| SOLA | local periodicity | synthesis frame shifted by `k_m = argmax xcorr(y, frame_m)`; length becomes data-dependent | low | as WSOLA plus length correction | Poor | — |
| WSOLA | one lag aligns the frame (quasi-periodic, few sources) | `δ_m = argmax_δ Σ_n x̃_{m−1}(n)·x(n+mHa+δ)·w(n)`, N ≈ 50 ms, Δmax ≈ 25 ms | low–med (O(N·Δmax) per frame, or FFT xcorr) | transient doubling/skipping within ±Δmax; warble on polyphony; stereo needs one shared δ | Poor–fair on dense mixes | Rejected as default (TSR Alternatives) |
| TD-PSOLA | single f0 with pitch marks | 2-period frames centred on pitch marks, periods duplicated/dropped | low | breaks on polyphony, unvoiced/noisy content | Unsuitable | Not in scope |
| Granular | none; diffuse is acceptable | `y(t)=Σ_g w(t−t_g)·x(t−t_g+W⁻¹(t_g))`, 20–100 ms grains, overlap 2–8× | low | cloudy/smeared, flams on rhythm | Unsuitable for rhythm/vocals | "Texture" deferred (brief §9 Granular; extreme ratios only) |
| Phase vocoder (textbook) | locally stationary sinusoids | `ω̂=ω_k+princarg(φ(m,k)−φ(m−1,k)−ω_k·Ha)/Ha`; `φ_s(m,k)=φ_s(m−1,k)+ω̂·Hs` | med: 2 FFT per hop, O((N/Hs)·log N) per sample | phasiness, transient smear up to N (vertical coherence lost even at α = 0.98: 1.19 rad/frame drift at 440 Hz, TSR TD-10) | Fair | Never shipped; low anchor in listening tests |
| Phase-locked PV (identity / scaled, Laroche–Dolson) | partials dominate their peak regions | `φ_s(m,k)=φ_s(m,k_p)+φ(m,k)−φ(m,k_p)` | PV + peak picking | reduced phasiness; residual smear on attacks | Good | Concept inside Signalsmith-class engines |
| Phase-gradient PV (PGHI, "Done Right"; Signalsmith's multi-prediction is conceptually close) | phase derivative estimable in time and frequency | heap integration of `Δtφ`, `Δfφ`, seeded at max magnitude | PV + O(K log K) per frame | small; noise tonalisation at large α | Good–best | **Smooth = Signalsmith (MVP default)**; own PGHI = Phase 2+ option |
| Transient-aware PV (phase reset, Röbel/Bonada/Duxbury) | onsets detectable per bin | at onset frames `φ_s(m,k)=φ(m,k)` for transient bins | PV + detector | reset clicks/interruptions on sustained partials; detector errors | Good on drums | Needs own PV; [TRANSIENT_PRESERVATION.md](TRANSIENT_PRESERVATION.md) alternative |
| Multi-resolution PV (RB R3) | different bands need different N | long N for LF tonal, short for HF/transients | high ("much more CPU") | few | Best documented on mixes/vocals/bass | Dev-only reference (§9) |
| HPSS hybrid (Driedger 2014) | harmonic and percussive parts separable by median-filter masks | `y = PV(M_h⊙X) + OLA_short(M_p⊙X)`, same W⁻¹ | ≈ 2× PV (HPSS STFT + PV + OLA; UNVERIFIED, benchmark) | hollow attacks on mask leakage; codec noise misclassified | Good–better on drums | **"Transient", Phase 1.5** |
| Fuzzy per-bin hybrid (Damskägg & Välimäki 2017) | each bin is a mix of tonal/noise/transient | per-bin memberships steer phase propagation | ≈ PV + classification | low (no seams) | Good on mixed material | Long-term "Auto" ([HYBRID_AUTO_ENGINE.md](HYBRID_AUTO_ENGINE.md)) |

**Architecture recommendation** (consistent with DECISION §1–2): one W⁻¹-driven `ITimeStretcher` seam; MVP ships **Smooth** (Signalsmith, linked stereo) as default and **Resample** for constant-ratio material; **Transient** is an engine-agnostic HPSS wrapper around Smooth in Phase 1.5; **no region switching** in the MVP; Auto evolves toward soft per-bin classification inside one engine. Why one PV-class engine: at our small ratios the quality spread between good PVs is narrow (TSR Finding 3), a full mix contains every source type at once (so region classification is coarse), and every engine switch is a seam (TSR TD-7). Time-domain methods lose on tonal density; TD-PSOLA and granular violate their assumptions on full mixes.

## 11. Source-type behaviour (brief §8, MVP Smooth at |1 − r| ≤ 5 %)

| Material | Dominant risk | Smooth (MVP) | Transient (1.5) | Watch metric | Mitigation now |
|---|---|---|---|---|---|
| Full mixed song | mild phasiness, kick/snare softening | Good | Good–better on drums | O8–O10, listening | — |
| Electronic / 4-on-floor | kick attack smear, sub phase | Good | Best | O8, O9 | Transient in 1.5 |
| Drum-heavy | smear, softened snare | Fair | Best | O8–O10 vs RB R3 | Transient in 1.5 |
| Transient-heavy drops | smear + true-peak rise | Fair | Best | O9, O14 | peak gate (QUALITY_VALIDATION V5) |
| Vocals | phasiness/"chorus"; formants kept (pitch = 1) | Best | Good (sibilants may go percussive: harmless) | O7, listening | RB R3 comparison; LIS-MUSHRA vocals category |
| Sustained pads / synths | warble only in time-domain methods | Best | = Smooth (all harmonic) | O11a/b | — |
| Strings | vibrato phase coherence; bow noise | Good | = Smooth | O7 (S11 proxy) | — |
| Piano / plucked guitar | attack softening + sustain phasiness | Good | Best | O8, O11a | — |
| Guitars (strummed, distorted) | dense HF partials; strum attacks | Good | Good | O8 | — |
| Bass / sub | needs long N; phase cancellation with kick | Best (120 ms block) | Good if LF stays harmonic | O11a, mono fold-down | keep High preset for export |
| Ambient / reverberant sections | metallic tails, noise tonalisation | Fair–good | = Smooth | O11c, listening | — |
| Highly stereo / Haas / chorus | image collapse if unlinked | Good (linked) | Good (shared masks) | O13 | linked instance (§7) |
| Dense limited masters | inter-sample overs after resynthesis (+1.25 dBFS measured with RB R2) | Good | Good | O14 | peak gate |
| Sparse intros / silence | pre-echo of smear into silence | Good | Good | O9 | — |
| AI/Suno lossy masters (assumption) | pre-existing codec smear; HPSS may mis-split codec noise | as full mix | test | corpus tier U | DSP_RESEARCH Open Q1 |

**Should source classification choose the mode automatically?** Not in the MVP. Reasons and the future design: [HYBRID_AUTO_ENGINE.md](HYBRID_AUTO_ENGINE.md). The MVP mode menu is **Smooth (default)** and **Resample** (gated, §4). MAIN_WINDOW.md §2 and UX_BEHAVIOR.md §8 list "Auto, Smooth, Transient" with Auto as default; that conflicts with DECISION §1/§4 and is reported in the hand-back.

## 12. Brief §26 answers (Q9–18)

| Q | Answer | Detail |
|---|---|---|
| 9 Which algorithms | Smooth (linked phase-gradient-style PV, Signalsmith), Resample (bandlimited varispeed, constant ratio), Transient (HPSS hybrid, Phase 1.5). Slice/Texture/region Auto post-MVP | §10 |
| 10 Default | Smooth, `presetDefault` | §3 |
| 11 How Auto chooses | MVP: it doesn't. Future: soft per-bin classification in one engine; region rules only if listening tests demand | HYBRID_AUTO_ENGINE.md |
| 12 Transients | Phase 1.5 HPSS hybrid (no transient detection, no transient copy); patent flag on copy/reinsert designs | TRANSIENT_PRESERVATION.md |
| 13 Vocals | PV-class engine with pitch = 1 keeps formants; 120 ms block; vocals category gates release in LIS-MUSHRA vs RB R3; never Resample on variable maps | §3, §4, §11 |
| 14 Stereo image | one linked multi-channel instance; IPD preserved by construction; HPSS masks shared | STEREO_PHASE_PRESERVATION.md |
| 15 Phase problems | phase-locked/gradient PV (vertical coherence), identity bypass for unchanged songs, linked stereo, latency compensated in the adapter, chunk crossfades ρ-adaptive | §3, §6, §8 |
| 16 Large corrections | hard 0.5–2.0 (refuse + point to half/double fix), warn 0.92–1.08 per segment, global > ±15 % comfort warning; large steps are a warp-smoothing problem, not an engine one | §5, WARP_FUNCTION §4 |
| 17 Render efficiently | chunk-parallel render, playhead-first, per-chunk content-addressed cache, Fast preset for preview, Resample single-pass | §8, ARCHITECTURE §5 |
| 18 Verify correction | post-render validation: length, NaN, onset alignment (Path K′/R′), residual drift, peaks, clicks, stereo, silence; re-decode at export | QUALITY_VALIDATION.md |

## 13. Tests

| ID | Test | Pass | Links |
|---|---|---|---|
| TSE-1 | Impulse/kick trains (S01a–c) through Smooth at r ∈ {0.90, 0.95, 0.98, 1.00, 1.02, 1.05, 1.10} and an S03-derived piecewise W; locate output events vs W(s_j) | O1: median ≤ 1 ms, P95 ≤ 3 ms, max ≤ 5 ms, \|mean e\| ≤ 0.5 ms (PROVISIONAL) | DSP-STR-1; validates §3.2 |
| TSE-2 | Length | N_out = round((W(L) − tauOrigin)·fs) exactly | O17 |
| TSE-3 | Convention: constant r = 0.98, Signalsmith per-call counts and `playbackRate = 1/r` | duration and click positions match W | §2.2 |
| TSE-4 | Stereo set S12 at r = 1.05 | O13 a, b exact; c–f within thresholds | DSP-STR-4 |
| TSE-5 | Chunked render with 1, 2, 4, 8 threads | SHA-256 identical (D1) | §8.4 |
| TSE-6 | Seams: S16, S10 pad chords, S15 tails; cuts forced onto transients and onto sustained chords | O15 zero new flags at seams; short-term loudness deviation across seam ≤ 0.5 dB; report ρ distribution | replaces DSP-REN-1 null for Smooth |
| TSE-7 | Identity bypass: S01(c) at its own tempo, GOLD-01 | output == source (shifted), bit-exact | §6, O18 |
| TSE-8 | Smooth forced at r = 1 (bypass disabled) | null residual measured; PROVISIONAL ≤ −60 dB | O18 |
| TSE-9 | Bounds: maps with r = 0.49 / 2.01 rejected by `validate()`; r = 0.5 and 2.0 render | rejection with segment list; no NaN, length exact | DSP-STR-6 |
| TSE-10 | Resample: non-constant map → unavailable; constant r: click positions and pitch | offset 0 ± 0.01 sample (R3 logic); pitch = 1200·log2(1/r) ± 0.5 cent | GOLD-12, DSP-RES-1 |
| TSE-11 | Cancel at random hops (100 runs) | returns Cancelled ≤ 250 ms; no partial chunk marked ready | DSP-REN-2 |
| TSE-12 | File edges: first/last chunk, W(0) ≠ 0 both signs | no click at start/end (O15); zeros only where W⁻¹ < 0 | §2.3 |
| TSE-13 | RB R3 reference via keyframe map on S01 | report O1; reference accepted for timing only if max ≤ 1 ms | §9 |
| TSE-14 | Fast vs High presets | both pass TSE-1; separate cache keys | §3.1 |

## Risks
| Risk | Mitigation |
|---|---|
| Drive equation off by a latency term → constant offset | TSE-1 per preset and ratio; the adapter is the only place latency exists |
| Chunk seams audible on pads/tails (phase history) | quiet cut search, H ≥ 250 ms, ρ-adaptive fades, merge-chunk and single-pass fallbacks |
| Signalsmith softens drums on dense masters | Transient mode (1.5); RB R3 comparison; escalation rule in SUBJECTIVE_LISTENING_TESTS §6 |
| Smooth at r = 1 not transparent | whole-song bypass covers the stable-song case; regional Passthrough evaluated in 1.5 |
| Preview (Fast) differs from export (High) | lane label; export always High |

## Open Questions
1. Does `outputSeek` with a non-unity initial rate align exactly (TSR Open Q2)? Answered by TSE-1.
2. Seam audibility with O = 80 ms; does a longer H reduce decorrelation (ρ) measurably? TSE-6 sweep.
3. Output origin when W(0) < 0 (§2.3): accept the |W(0)| shift or define a different Preserve-start rule? Owner: TARGET_BEAT_GRID.
4. r8brain wrapper with non-integer fs_out: confirm API (UNVERIFIED class names).

## Implementation Implications
- `bf_dsp/stretch/`: `SmoothStretcher`, `ResampleStretcher`, `RenderPlan` (cut search), `assemble()` (ρ-adaptive), `identityBypass()`. `RubberBandR3Stretcher` under the dev build flag. Bungee only after TSE-1/4 pass.
- Every constant here (H, O, ε_bypass, cut windows) lives in the single `constexpr` config header with a pointer to the test that validates it (ARCHITECTURE §coding standards).
