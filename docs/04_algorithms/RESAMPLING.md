# Resampling (Sample-Rate Conversion)

Status: algorithm spec, 2026-10-08. Research context: `docs/03_research/AUDIO_QUALITY_RESEARCH.md` §4.4–4.5.

## Objective

Convert between sample rates with no audible aliasing or imaging, a flat passband, **zero net time offset** (beat positions in seconds are identical before and after), deterministic output, and exactly one conversion per export.

## Why It Matters

- Beat alignment: a resampler's group delay that is not compensated shifts every beat. A 256-tap linear-phase filter at 48 kHz delays audio by 2.7 ms. A minimum-phase filter adds frequency-dependent delay, which smears transient alignment.
- Quality: libsamplerate's best mode is only 97 dB SNR. That is fine for 16-bit, but below the 24-bit noise floor (−144 dBFS).
- Peaks: SRC converts inter-sample peaks into sample peaks, so peak measurement must happen **after** SRC.

## Key Findings

| Claim | Evidence | Conf. |
|---|---|---|
| Bandlimited windowed-sinc interpolation (Smith/Gossett) or polyphase rational FIR is the standard high-quality method | J.O. Smith, "Digital Audio Resampling Home Page" (CCRMA); libsamplerate/soxr/r8brain designs | H |
| libsamplerate (BSD-2): SINC_BEST 97 dB SNR @ 97 % BW; MEDIUM 97 dB @ 90 %; FASTEST 97 dB @ 80 %; float I/O | libsamplerate docs (Context7 `/libsndfile/libsamplerate`) | H |
| rubato (MIT, Rust): `Fft<T>` synchronous fixed-ratio resampler (default window BlackmanHarris2; sub-chunk count trades delay vs cutoff); `Async::new_sinc` with `SincInterpolationParameters` (sinc_len, f_cutoff or auto `calculate_cutoff`, oversampling_factor, interpolation, window); f32/f64 generic | Context7 `/henquist/rubato` | H |
| r8brain-free-src: MIT, C++, double precision, arbitrary ratios | github.com/avaneev/r8brain-free-src | H (licence) / M (API names `CDSPResampler24` from memory, UNVERIFIED) |
| libsoxr: LGPL-2.1+, very high quality (VHQ) | soxr repo / PyPI | H |
| 44100↔48000 reduces to L/M = 160/147 (gcd 300) | arithmetic | H |

## Technical Details

### When resampling happens (policy)

| Path | Resample? | Quality tier |
|---|---|---|
| Render (TSM) | **Never**. TSM runs at the source SR. Pitch scale = 1.0, so the stretch engine's internal resampler stays bypassed. | — |
| Export, export SR = source SR | No (bit-exact bypass) | — |
| Export, user picks another SR, or codec forces it (MP3 needs ≤48 k; Opus needs 48 k) | **Once**, after render/enhancement, before gain/measure/dither/encode | **Export tier** |
| Analysis (beat tracker input, e.g. 22.05 kHz mono) | Yes, a separate buffer that never feeds the render | **Analysis tier** (cheaper, but must be zero-phase) |
| Waveform display | No SRC; min/max decimation (peaks pyramid) | — |
| Optional future "varispeed / tape" mode (pitch follows tempo) | time-varying ratio | async sinc (post-MVP) |

### Mathematical model

Continuous-time bandlimited reconstruction, evaluated at output instants (zero-phase, centred kernel, so no delay needs compensating):

```
Fin, Fout              input/output rates
r      = Fout / Fin
σ      = min(1, r) · ρ                       cutoff scale; ρ = (f_pass + f_stop)/F_low (≈0.954 for 20 k/22.05 k)
t_m    = m / Fout                            output instant (seconds), m = 0..Nout-1
u_m    = t_m · Fin                           position in input samples (f64! see precision)
y[m]   = Σ_{k = ⌊u_m⌋-Nz/σ+1}^{⌊u_m⌋+Nz/σ}  x[k] · σ · sinc(σ·(u_m − k)) · w((u_m − k)·σ / Nz)
sinc(v) = sin(πv)/(πv),   w(·) = Kaiser window on [-1, 1], zero outside
x[k] = 0 for k < 0 or k ≥ Nin                (zero extension at both ends)
```
- Downsampling (r < 1): the kernel widens by 1/σ, i.e. the low-pass sits at the **output** Nyquist (anti-aliasing).
- Upsampling: cutoff at the **input** Nyquist (anti-imaging).
- Gain: σ·sinc(σ·v) has unity DC gain. Verify with test R7.

**Rational fast path (polyphase).** For integer rates, g = gcd(Fin, Fout), L = Fout/g, M = Fin/g. The fractional offsets `u_m − ⌊u_m⌋ = ((m·M) mod L)/L` take only L distinct values. Precompute L phase filters of length T = 2·⌈Nz/σ⌉:
```
for p in 0..L-1:
    for j in 0..T-1:
        v = p/L + (T/2 - 1) - j               // distance (in input samples) from tap to output instant
        H[p][j] = σ · sinc(σ·v) · kaiser(v·σ/Nz, β)
    normalise H[p] so Σ_j H[p][j] = 1          // removes per-phase DC ripple (optional, keep ≤1e-12 change)

y[m]:  n0 = (m·M) div L ;  p = (m·M) mod L     // exact integer arithmetic, i64
       y[m] = Σ_j H[p][j] · x[n0 - (T/2 - 1) + j]
```
44.1↔48 k: L = 160 or 147 phases. 96→48: L/M = 1/2, so a single phase, i.e. a half-band-like decimator.

**Irrational / time-varying ratios** (varispeed, post-MVP): Smith's table method. Store the kernel oversampled by 2^9–2^12 points per zero-crossing and linearly (or cubically) interpolate between table entries. The error from table interpolation must stay below the stopband target, so for 140 dB use ≥2^12 points with linear interpolation, or 2^9 with cubic. Ratio changes act on the f64 position accumulator `u`.

### Filter design (Kaiser)

```
A   = stopband attenuation (dB)                  target 140, minimum 120 (export tier)
β   = 0.1102·(A − 8.7)                  if A > 50          → A=140: β = 14.47 ; A=120: β = 12.27
Δf  = transition width = f_stop − f_pass (Hz), at the lower of the two rates
Δω  = 2π·Δf / F_low
Ntaps_low ≈ (A − 7.95) / (2.285·Δω)              taps per output (downsampling) / input (upsampling) sample
```
Worked example 48 → 44.1 kHz, f_pass = 20 kHz, f_stop = 22.05 kHz (Nyquist_out), A = 140 dB:
Δω = 2π·2050/44100 = 0.2921 rad, so N ≈ 132.05/(2.285·0.2921) ≈ **198 taps** at the 44.1 k rate. That is ~215 input taps per output sample (Nz ≈ 99–108 zero crossings per side), and roughly 2·215·2 ≈ 860 MACs per stereo output frame. A 10-min stereo song is about 2.3·10^10 MACs, i.e. seconds with SIMD.

### Parameters

| Parameter | Export tier | Analysis tier | Notes |
|---|---|---|---|
| Phase response | **linear (symmetric), zero-phase via centred kernel** | same (mandatory) | minimum phase is rejected: dispersive delay smears transient timing |
| Passband edge | min(20 kHz, 0.4535·F_low) | 0.40·F_low | 0.4535·44.1 k = 20.0 k |
| Stopband edge | 0.5·F_low (Nyquist of the lower rate) | 0.5·F_low | the transition band may alias into itself (above 20 kHz): accepted, inaudible |
| Passband ripple | ≤ ±0.01 dB | ≤ ±0.1 dB | |
| Stopband attenuation | ≥ 120 dB (target 140) | ≥ 80 dB | |
| Window | Kaiser β per A (or BlackmanHarris in rubato) | same | |
| Arithmetic | f64 coefficients and MACs; f32 or f64 I/O | f32 OK | |
| Position / phase accumulator | **integer (m·M) div/mod L in i64**; f64 for irrational ratios | same | never f32 (f32 loses integer precision above 2^24 samples ≈ 349 s @48 k) |
| Output length | Nout = (Nin·L + M/2) div M (round half up, integer math) | same | documented and tested |
| Edges | zero extension both sides; no trimming beyond the length rule | same | zero-phase kernel, so nothing to trim |
| Chunking | stateful streaming with a T-sample history; result must equal whole-buffer processing | same | |

### Library mapping (pick per stack; all must pass the tests below)

| Stack | Export tier | Analysis tier | Caveat |
|---|---|---|---|
| Rust | `rubato` (MIT): `Fft::<f64>::new(fin, fout, chunk, ch, FixedSync::Both)` for fixed ratios, or `Async::<f64>::new_sinc` with `SincInterpolationParameters::new(256, BlackmanHarris2)` + oversampling 256 + Cubic | rubato `Fft::<f32>` | rubato FFT/sinc introduces **latency**: measure it with test R3 and trim it (a delay-query API may exist, UNVERIFIED). Note `sub_chunks` lowers the cutoff. |
| C++ | r8brain-free-src (MIT) high-quality 24-bit preset (UNVERIFIED class name) | same, low-quality preset | measure latency, trim |
| Own implementation | the polyphase pseudocode above (~150 LOC) | same | full control of zero-phase and length rule; recommended if library latency handling is awkward |
| Avoid | libsamplerate (97 dB) for export; Speex (lower quality); LGPL soxr/ffmpeg-swr only if dynamically linked (**for legal review**) | | |

Recommendation: **decided** (DECISION_SUMMARY.md, ADR-009). Use **r8brain-free-src (MIT)** in its 24-bit/high-quality configuration, wrapped in one function `resampleOffline(in, fsIn, fsOut) -> out`. The wrapper enforces the exact length/delay contract (latency trimmed, output length `round(N_in·fs_out/fs_in)`) and the test suite above. Reusing a maintained library is cheaper than writing and maintaining our own polyphase FIR. If r8brain fails the stopband or delay tests, the fallback is our own polyphase zero-phase SRC (~150 lines) checked against rubato as an oracle. libsamplerate (97 dB) is rejected for 24-bit export.

### Pseudocode: offline export SRC

```
fn resample_export(x: &[[f32; C]], fin: u32, fout: u32) -> Vec<[f32; C]>:
    if fin == fout: return x.clone()                       // R10 bypass, bit-exact
    g = gcd(fin, fout); L = fout/g; M = fin/g
    ρ = (f_pass + f_stop) / F_low                         // -6 dB point midway: (20000+22050)/44100 = 0.954
    σ = min(1.0, L as f64 / M as f64) * ρ
    (H, T) = design_polyphase(L, σ, A=140)                  // cached per (fin, fout)
    Nout = (x.len()*L + M/2) / M
    for m in 0..Nout (parallel over chunks; each chunk deterministic):
        n0 = (m*M) / L ; p = (m*M) % L
        acc[c] = 0.0f64 for each channel
        for j in 0..T: k = n0 - (T/2 - 1) + j
                        s = if 0 <= k < len { x[k][c] as f64 } else { 0.0 }
                        acc[c] += H[p][j] * s
        y[m][c] = acc[c] as f32                            // keep f32; no clipping here (gate later)
    return y
```

## Alternatives Considered

- **Minimum-phase SRC** (less pre-ringing): rejected. It introduces frequency-dependent delay and breaks the zero-offset contract.
- **Resample the source before TSM** (render at export SR): rejected as default. It costs more for upsampling and moves analysis grids. It is equivalent in quality only in theory.
- **libsamplerate**: rejected for export (97 dB). Acceptable for the analysis tier if already present.
- **Higher internal rate for TSM**: rejected (no demonstrated benefit; doubles cost).

## Evidence / Sources

- J.O. Smith, Digital Audio Resampling Home Page, CCRMA Stanford: https://ccrma.stanford.edu/~jos/resample/
- Kaiser window design formulas: Oppenheim & Schafer, *Discrete-Time Signal Processing*, §7.6 (β and N formulas)
- libsamplerate converter specs: https://github.com/libsndfile/libsamplerate/blob/master/docs/api_misc.md
- rubato docs (Context7 `/henquist/rubato`): `Fft::new`, `Fft::new_custom`, `Async::new_sinc`, `calculate_cutoff` (≈0.947 for n=256, BlackmanHarris2)
- r8brain-free-src (MIT): https://github.com/avaneev/r8brain-free-src ; libsoxr (LGPL-2.1+)

## Recommendation

1. Zero-phase, linear-phase windowed-sinc / polyphase SRC. Export tier ≥120 dB (target 140) stopband, ±0.01 dB passband to 20 kHz, f64 arithmetic, integer phase accounting. (H)
2. Exactly one SRC per export, after render and before measure/gain/dither/encode. Bypass when the rates are equal. (H)
3. Output length rule `Nout = round(Nin·Fout/Fin)` and a 0-sample timing offset. Both are contractual and tested. (H)
4. A separate analysis-tier resampler that is also zero-phase, so beat times map 1:1 in seconds. (H)

## Risks

- Library latency silently uncompensated → beat offset. Mitigation: test R3 is mandatory for any implementation.
- Gibbs overshoot on full-scale square-like content raises peaks. This is correct behavior, handled by the post-SRC peak gate.
- Long kernels plus 147/160 phases: memory 160 × 216 × 8 B ≈ 276 KB. Fine.

## Open Questions

- Does the chosen stack's library expose exact latency (rubato `output_delay`?). If not, use our own implementation.
- Are 88.2/96/192 kHz sources common in the user base? This decides how much to optimize the 2:1 / 4:1 paths.

## Implementation Implications

- Module `resample` with `design_polyphase(L, σ, A)` cached by rate pair, `resample_export`, `resample_analysis`.
- Golden fixtures: sweeps and clicks at 44.1/48/88.2/96 kHz.

## Tests (acceptance)

| ID | Test | Pass criterion |
|---|---|---|
| R1 | Passband: sine steps 20 Hz–20 kHz (1/12 oct) at −6 dBFS, 44.1↔48, 96→48, 48→96 | gain within ±0.01 dB |
| R2a | Anti-alias 48→44.1: 23.5 kHz sine at −1 dBFS | alias at 20.6 kHz ≤ −120 dBFS (target −140) |
| R2b | Anti-image 44.1→48: 21 kHz sine | image at 23.1 kHz ≤ −120 dBFS |
| R3 | **Timing**: band-limited click centred at input t0 (non-integer output position); locate output peak by sinc/parabolic interpolation | |t_out − t0| ≤ 0.01 output samples; impulse response symmetric (asymmetry energy ≤ −100 dB) |
| R4 | Length | Nout = (Nin·L + M/2) div M for Nin ∈ {0, 1, 2, 147, 1e6+3} |
| R5 | Round-trip 44.1→48→44.1, noise band-limited to 19 kHz, ignore first/last 100 ms | residual ≤ −100 dB re signal |
| R6 | THD+N at 1 kHz, −1 dBFS, f64 path | ≤ −120 dB |
| R7 | DC 0.5 for 1 s | output 0.5 ± 1e-7 (excluding edges) |
| R8 | Chunk invariance: block sizes 1, 64, 1000, 4096, whole | bit-identical outputs |
| R9 | Determinism: 10 runs, 1 vs N threads | bit-identical |
| R10 | Fin = Fout | output bytes == input bytes |
| R11 | All-zero input | all-zero output, no denormals (flush-to-zero enabled or verified) |
| R12 | NaN/Inf guard: full-scale square, 10 min | no NaN/Inf; peak reported (Gibbs overshoot expected ≈ +9 % ≈ +0.8 dB) |
| R-A1 | Analysis tier: click track at 44.1 k → 22.05 k mono; onset times | equal to source times within 0.05 ms |
| R13 | Benchmark (not a gate): 10-min stereo 48→44.1 export tier | record time; target < 10 s on a 4-core 2020 laptop (assumption) |
