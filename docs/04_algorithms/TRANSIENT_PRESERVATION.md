# Transient Preservation ("Transient" mode, Phase 1.5)

Status: algorithm spec, 2026-10-08. **Phase 1.5** (after MVP), per [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1 "Transient mode". Research: [TIME_STRETCH_RESEARCH.md](../03_research/TIME_STRETCH_RESEARCH.md) (TSR) Findings 9–11, TD-3, TD-5, TD-10; [DSP_RESEARCH.md](../03_research/DSP_RESEARCH.md) (DSPR) TD-1, TD-3, TD-6. Engine contract: [TIME_STRETCH_ENGINE.md](TIME_STRETCH_ENGINE.md) (TSE).

## Objective
Keep kick/snare/piano attacks sharp when warping dense full mixes, without seams, without a transient detector in the signal path, and without a transient copy-and-reinsert step (patent flag, §5).

## Why It Matters
Smooth (Signalsmith) has no explicit transient handling (TSR Finding 6). A PV disperses an attack over up to one window (120 ms block in `presetDefault`), audible as softened drums and pre-echo into silence. Drums are the material where users judge "tight" timing, and DECISION §3 item 2 accepts this as the MVP's main known compromise.

---

## 1. Method: HPSS hybrid (Driedger, Müller, Ewert 2014)

```
            ┌──────────── shared soft masks from max(|X_L|,|X_R|) ────────────┐
 x (L,R) ─▶ STFT ─▶ M_h⊙X_L, M_h⊙X_R ─▶ ISTFT ─▶ x_h ─▶ SmoothStretcher(W) ─▶ y_h ─┐
               └──▶ M_p⊙X_L, M_p⊙X_R ─▶ ISTFT ─▶ x_p ─▶ WarpedOLA(W, N=11.6 ms) ─▶ y_p ─┴─▶ y = y_h + y_p
```

### 1.1 Equations
STFT `X_c(m,k)` per channel c (Hann, N_s = 2048, hop 512 at 44.1 kHz; keep durations at other rates, DSPR TD-1).
```
A(m,k)   = max(|X_L(m,k)|, |X_R(m,k)|)                       shared magnitude (mono: |X|)
H(m,k)   = median_{|j|≤ℓ_h} A(m+j, k)                          ℓ_h = 8  (17 frames ≈ 200 ms)
P(m,k)   = median_{|j|≤ℓ_p} A(m, k+j)                          ℓ_p = 8  (17 bins ≈ 370 Hz)
M_h      = H^p / (H^p + P^p + ε),   M_p = 1 − M_h               p = 2 (Wiener-type), ε = 1e-20
x_h,c    = ISTFT(M_h ⊙ X_c),   x_p,c = ISTFT(M_p ⊙ X_c)         WOLA-normalised ISTFT
```
Because `M_h + M_p = 1` and the ISTFT is linear with perfect reconstruction, `x_h + x_p = x` (test TR-1: error < −120 dB). Nothing is lost or added by the split; only the stretching of the two parts differs.

**Why `max(|X_L|,|X_R|)` and not `|X_M|`:** the mask must be identical on both channels (otherwise a source is "percussive" on L and "harmonic" on R and smears across the image, TSR TD-5). `|X_M|` vanishes for anti-phase content (L = −R gives M = 0, mask undefined); the max does not.

### 1.2 Harmonic branch
`y_h = SmoothStretcher.render(x_h, W, …)` with the same chunk span, preset and latency compensation as Smooth (TSE §3). Identity phase locking is not needed separately: Signalsmith already does vertical-coherent prediction.

### 1.3 Percussive branch: warped short-window OLA
```
N_p = 512 (11.6 ms @ 44.1 kHz), Hs_p = 256, window w = periodic Hann (COLA at 50 %)
for each output frame m in the render span:
    τ_m  = τ_begin + m·Hs_p                       (f64 output position of the frame centre)
    a_m  = W⁻¹(τ_m)                                (f64 source position of the frame centre)
    s0   = llround(a_m) − N_p/2                    (i64, absolute; never accumulated)
    for c in channels:  y_p,c[τ_m − N_p/2 + n] += w(n) · x_p,c[s0 + n]   (zero-extended source)
    norm[τ_m − N_p/2 + n] += w(n)
y_p = y_p / max(norm, 1e-9)                         (= 1 away from span edges)
```
Same frame position on all channels (time-domain op, image exact). Doubling check: consecutive frames read source positions differing by `Hs_p/r`, so an attack is repeated at an offset of `Hs_p·|1 − 1/r|`, i.e. 13 samples (0.3 ms) at r = 0.95. Far below audibility, which is why the short window works without a transient detector (TSR Finding 9). Output is a non-negative-weight combination of input samples, so the percussive branch cannot raise the sample peak (AUDIO_QUALITY_RESEARCH §4.8); the sum `y_h + y_p` can.

### 1.4 Alignment
Both branches must satisfy TSE contract C1 (zero latency bias). The OLA branch is zero-latency by construction (centred frames); the harmonic branch is compensated in the Smooth adapter. Test TR-3 runs the impulse test on each branch separately and on the sum. A residual inter-branch offset of even 1 ms produces a flam (two attack copies), so this test is a hard gate.

### 1.5 Chunking and cost
HPSS medians are non-causal (±ℓ_h frames ≈ ±100 ms). The chunk's source context grows by `ℓ_h·hop + N_s` on each side; this is added to `P_ctx` (TSE §8.5). HPSS runs per chunk on the chunk's padded source span, so the chunk job stays a pure function (D1). Cost estimate: one STFT + ISTFT pair at hop 512, two median filters of 17 taps over ~1025 bins/frame, OLA (trivial), plus Smooth: about 2× Smooth (TSR TD-9 "similar order"). **UNVERIFIED**; Phase 1.5 perf gate: Transient render ≤ 2× Smooth RTF and within the DECISION §6 render target.

### 1.6 Parameters (starting points, tune by listening, DSPR Open Q1)
| Parameter | Start | Sweep | Note |
|---|---|---|---|
| HPSS N_s / hop | 2048 / 512 | 1024–4096 | 21.5 Hz bins |
| ℓ_h, ℓ_p | 8, 8 | 4, 8, 15 | Toolbox used 10/10 at its own resolution |
| mask power p | 2 | 1, 2 | binary masks rejected (musical noise) |
| N_p / Hs_p | 512 / 256 | 256–1024 | longer = audible doubling |

## 2. Known failure modes
| Failure | Mechanism | Detection | Mitigation |
|---|---|---|---|
| Hollow/flammy attacks | mask leakage: attack partly in x_h (smeared) and partly in x_p (sharp) | O8–O10, LIS-PAIR | soft masks; TR-3 alignment; tune ℓ_p |
| Codec noise classed percussive (AI/MP3 sources) | lossy HF "swirl" is broadband and short → median over frequency favours P | TR-5 on S16 | raise ℓ_h, or limit M_p above the source's effective bandwidth cut-off (AUDIO_QUALITY §4.10 measures it) |
| Sibilants to percussive branch | broadband noise bursts | listening (vocals) | harmless at small ratios (TSR TD-6) |
| Roughness on sustained noise (cymbal wash, reverb) | OLA of stationary noise is fine; but noise split across branches decorrelates | O11c | soft masks |
| True-peak increase | sum of independently processed branches | O14 | post-render peak gate (QUALITY_VALIDATION V5) |

## 3. Alternatives for transient handling
| Option | Mechanism | Status | Why |
|---|---|---|---|
| **HPSS hybrid** | spectral split, two stretch methods, same W⁻¹ | **Chosen for Phase 1.5** | documented benefit (Driedger 2014), no detector, no seams, engine-agnostic wrapper, licence-clean (we implement from the paper; TSM Toolbox is GPL, reference only) |
| Phase reset at onsets (Bonada 2000, Duxbury 2002, Röbel 2003 per-bin) | at onset frames set `φ_s = φ` for transient bins | Phase 2+ (needs own PV) | Signalsmith exposes no phase-reset hook (TSR Finding 6). Risks: reset discontinuities on sustained partials; detector errors. RB R2 `OptionTransientsCrisp` is this family (GPL, R2 only) |
| Hop locking near transients (Ha = Hs, compensate elsewhere) | copy the attack unmodified | **Blocked pending legal review** | same family as the Fraunhofer claim (§5) |
| Slice (inter-onset slices copied to W(onset)) | segment copy | **Post-MVP, blocked pending legal review** | closest to the claim; also chops sustains (TSR TD-6) |
| Shorter Smooth block (`presetCheaper`, 100 ms) | less dispersion window | test in Phase 0 pilot | free knob; trades bass/tonal resolution |
| Multi-resolution PV (RB R3) | short windows for HF | reference only | GPL/commercial (DECISION §1) |
| Fuzzy per-bin classification | memberships steer phase | Post-MVP research ("Auto") | [HYBRID_AUTO_ENGINE.md](HYBRID_AUTO_ENGINE.md) |

## 4. Integration
- Implemented as `HpssHybrid<Inner>` implementing `ITimeStretcher`; `Inner` = `SmoothStretcher`. Engine id `hpss1(p2,l8,l8,N512)+signalsmith@…` goes into the cache key.
- Warp map `mode` field: `transient` (WARP_MAP.md field notes). Whole song uses one mode (no region switching).
- UI: mode menu item "Transient (sharper drums)" appears only after the ship decision in §6. Default stays Smooth.

## 5. Patent flag: US 9,230,558 B2 (**for legal review**)
Facts (TSR Finding 10, Google Patents fetched 2026-10-08; claim read via summary, confidence Med): Fraunhofer, priority 2008-03-10, status active, expected expiry **2029-02-17**; related US 9,236,062 and US 9,275,652 (expiry not checked). Summarised claim: remove a transient time portion from the signal, time-stretch the remainder, reinsert an unprocessed copy of the transient portion.

Engineering differences, recorded **for counsel, not as a legal conclusion**:

| Claim element (as summarised) | HPSS hybrid (planned) | Phase reset (Phase 2+) | Slice / attack copy (not planned) |
|---|---|---|---|
| Identifies a transient *time portion* | No detector; masks are computed for every frame and bin | Yes (onset detector) | Yes |
| Removes that portion from the signal | No; spectral split over the whole duration, `x_h + x_p = x` | No | Yes |
| Time-stretches the remainder | Both components are stretched (PV and OLA) | Whole signal stretched | Remainder stretched |
| Reinserts an *unprocessed* copy | No insertion step. Note for counsel: OLA output frames are windowed copies of input frames at warped positions, everywhere, not only at transients | No (phases re-initialised, magnitudes still processed) | Yes |

Actions: (1) no transient copy/reinsert code before legal sign-off; (2) counsel reviews HPSS hybrid and phase reset too (TSR Risks); (3) re-check the related patents' expiry; (4) if counsel blocks HPSS as well, Transient waits for 2029 or uses a design counsel approves; Smooth remains the product.

## 6. Evaluation plan (ship gate for Phase 1.5)
1. **Objective, vs Smooth and RB R3** (DSP_TEST_STRATEGY DSP-STR-3, GOLD-04): O8 attack-time ratio (target ≤ 1.2× at \|1−r\| ≤ 5 %, DSPR TD-6), O9 pre-echo, O10 sharpness on S08 loops and S16; O6/O7 and O11a on S10/S11 must not regress vs Smooth (tonal parts go through the same engine); O13 all on S12 (shared masks); O15 zero flags.
2. **Mask tests**: TR-4 (pure tones: percussive energy ≤ −40 dB re total, PROVISIONAL), TR-5 (S16 MP3: fraction of energy above the effective-bandwidth cut-off assigned to x_p in sustained passages, reported, threshold set in pilot).
3. **Parameter pilot**: 2 listeners, sweep §1.6, pick one setting (SUBJECTIVE_LISTENING_TESTS §4 critical-material practice).
4. **Listening decision**: LIS-PAIR Transient vs Smooth on drum-heavy clips; ships only with significant preference and no significant loss on vocals/pads (SUBJECTIVE_LISTENING_TESTS §6, TSR Rec. 4). LIS-MUSHRA with RB R3 as reference engine for the record.
5. **Performance**: render RTF ≤ 2× Smooth, memory within PERFORMANCE_BENCHMARKS budgets.
6. **Legal**: §5 sign-off.

## 7. Tests
| ID | Test | Pass |
|---|---|---|
| TR-1 | Split reconstruction: `x_h + x_p` vs x on S13, S16 | ≤ −120 dB residual |
| TR-2 | Channel-shared mask: S12(a) L≡R and (b) L=−R through Transient | O13 a/b exact |
| TR-3 | Impulse alignment on harmonic branch, percussive branch, and sum (S01, S08) at the TSE-1 ratio set | each branch O1; inter-branch offset ≤ 0.1 ms |
| TR-4 | Leakage: S10 sines → x_p energy | ≤ −40 dB (PROVISIONAL) |
| TR-5 | Codec-noise classification on S16 MP3 | reported; threshold from pilot |
| TR-6 | Chunk determinism with HPSS context | D1 bit-identical across thread counts |
| TR-7 | Peak: S16 limited master | O14 reported; gate behaves as for Smooth |

## Open Questions
1. HPSS parameters for AI masters at 44.1/48 kHz (DSPR Open Q1).
2. Does limiting M_p above the effective bandwidth help on MP3-origin material?
3. Counsel's view on HPSS hybrid and phase reset relative to the US 9,230,558 family.
