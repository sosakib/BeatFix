# Dithering & Word-Length Reduction

Status: algorithm spec, 2026-10-08. Research context: `docs/03_research/AUDIO_QUALITY_RESEARCH.md` §4.6.

## Objective

Convert the f32 render to 16/24-bit integer output so that the quantization error is signal-independent, benign, white (or gently shaped) noise. Do not dither where it is pointless or harmful. Output must be deterministic.

## Why It Matters

Truncating or rounding without dither creates error that is correlated with the signal. That error shows up as harmonic distortion on quiet material (fades, reverb tails, which AI songs often end with) and as "granular" low-level noise modulation. TPDF dither turns this error into a constant noise floor whose first two moments do not depend on the signal. Dithering float output, or the input to a lossy encoder, only adds noise.

## Key Findings

| Claim | Evidence | Conf. |
|---|---|---|
| TPDF dither of ±1 LSB peak (sum of two independent uniform ±½ LSB variables) makes the mean and variance of the total error independent of the signal. Total error variance = Δ²/12 + Δ²/6 = **Δ²/4**, so RMS = Δ/2. | Lipshitz, Wannamaker & Vanderkooy, "Quantization and Dither: A Theoretical Survey", JAES 40(5), 1992; **MEASURED** (numpy, 2·10⁶ samples): error variance 0.2495–0.2504 LSB² at input offsets 0…1 LSB; mean ≈ 0 | H |
| 16-bit TPDF noise = **−96.33 dBFS RMS** (re full-scale peak 1.0). Undithered rounding noise −101.1 dBFS. The TPDF penalty is 10·log10(3) = 4.77 dB. 24-bit TPDF = **−144.49 dBFS**. | arithmetic + MEASURED (−96.331 vs −96.330 expected) | H |
| RPDF (one uniform) removes distortion but leaves noise modulation (the variance depends on the signal). Gaussian works but needs more power for the same effect. | Lipshitz et al. 1992 | H |
| Noise shaping (error feedback) moves noise into less audible bands (>~15 kHz). Total power rises but perceived noise falls. It is only meaningful at 44.1/48 kHz and 16-bit. | Lipshitz/Wannamaker "minimally audible noise shaping" (JAES 1991); SoX `dither -s` implementation | H (concept) / M (coefficients below, from SoX source memory, **UNVERIFIED**: verify against `sox/src/dither.h`) |
| FLAC stores integers only (4–32 bit). A float render going to FLAC must be quantized, hence dithered. | RFC 9639 | H |

## Technical Details

### When to dither (decision table)

| Output | Dither? | Noise shaping? |
|---|---|---|
| WAV/AIFF 32-bit float | **No** (write f32 as-is) | No |
| WAV/AIFF/FLAC 24-bit | Yes, TPDF (−144.5 dBFS, effectively free) | No |
| WAV/AIFF/FLAC 16-bit | Yes, TPDF | Optional (user setting; default **off**); only when SR ∈ {44.1, 48} kHz |
| MP3 / AAC / Opus / Vorbis | **No**: feed f32 to the encoder | No |
| Any integer target where the signal is already on the target grid (requantization bypass) | **No**: write bit-exact | No |
| Digital silence (all samples exactly 0) | Bypass (keeps true digital silence; optional "dither silence" = off) | — |

Only one dither stage exists per export, and it is the last DSP step before integer write.

### Conventions

```
N        target bits (16 or 24)
S        = 2^(N-1)                         scale: float ±1.0 ↔ int ±S
Δ        = 1/S                             LSB size in float units (16-bit: 3.0518e-5 ; 24-bit: 1.1921e-7)
qmin,qmax = -S, S-1                         clamp range (+1.0 → S overflows → clamp to S-1, counted as clip)
```
Scale S = 2^(N−1) (not 2^(N−1)−1) makes int→float→int exactly lossless. Readers and writers must use the same convention.

### TPDF quantizer (flat)

```
d[n]  = (r1[n] − r2[n]) · Δ          r1, r2 ~ U[0,1) independent, per channel   → d ∈ (−Δ, Δ), triangular pdf
q[n]  = round_half_even( (x[n] + d[n]) · S )     computed in f64
y[n]  = clamp(q[n], qmin, qmax)      clip_count += (q[n] != y[n])
error e[n] = y[n]/S − x[n]  →  E[e] = 0,  Var[e] = Δ²/4  (independent of x)
```

### Noise-shaped quantizer (error feedback, optional, 16-bit)

```
v[n]  = x[n] − Σ_{k=1..K} c_k · e[n−k]          // subtract filtered past total error
q[n]  = round((v[n] + d[n]) · S);  y[n] = clamp(q[n], qmin, qmax)
e[n]  = y[n]/S − v[n]                            // total error incl. dither (and clipping)
⇒ Y(z) = X(z) + E(z)·(1 − C(z)),  NTF(z) = 1 − Σ c_k z^-k
```
- First-order reference: c = [1], so NTF = 1 − z⁻¹ (0 at DC, +6 dB at Nyquist). Used for testing only.
- Candidate psychoacoustic filters (from SoX `dither.h`, **UNVERIFIED, must be verified against the source before use**):
  - "lipshitz" 44.1 kHz, 5 taps: c = [2.033, −2.165, 1.959, −1.590, 0.6149]
  - "f-weighted" 44.1 kHz, 9 taps (Wannamaker): c = [2.412, −3.370, 3.937, −4.174, 3.353, −2.205, 1.281, −0.569, 0.0847]
- Stability guard: high-gain NTFs can run away when clipping occurs (e[n] grows without bound). Clamp the fed-back error to |e| ≤ 4Δ (assumption, tune in test D7). If clip_count > 0 in a block, reset the error history and fall back to flat TPDF for that block, and log it.
- 48 kHz needs its own coefficients (the 44.1 k filters are tuned to its band). If none are verified, offer shaping at 44.1 kHz only.

### Requantization bypass

```
already_quantized = all(n, c): x[n][c]·S is an exact integer and within [qmin, qmax]
if already_quantized: y = x·S (exact), skip dither
```
This catches identity renders of integer sources (e.g. a section with warp ratio exactly 1.0 and passthrough engine). Bit-exact output proves "no degradation" in that case. Cost is one pass, and it can run fused with the peak meter.

### PRNG and determinism

- Generator: PCG32 or xoshiro256** (well tested, fast, tiny). One independent stream per channel, so L/R dither is uncorrelated. Correlated dither would collapse to a mono noise image.
- Seed = hash(project_id, render_settings_hash, channel_index). Same project and settings give byte-identical exports, which enables golden-file tests and reproducible bug reports.
- Uniform: r = (u32 >> 8) · 2^-24 → [0,1) with 24-bit resolution, enough for 16/24-bit targets. Use u64 → 53-bit for 24-bit if paranoid (not required: dither resolution ≪ Δ).

### Pseudocode (block-wise, streaming-safe)

```
struct Quantizer { N, S, Δ, shaping: Option<[f64; K]>, err_hist[c][K], rng[c], clip_count }

fn quantize_block(q: &mut Quantizer, x: &[[f32; C]], out: &mut [[i32; C]]):
    for n, c:
        xv = x[n][c] as f64
        if !xv.is_finite(): panic/raise ExportError::NaN        // validated earlier; defensive
        fb = Σ_k q.shaping[k] * q.err_hist[c][k]  (0 if no shaping)
        v  = xv − fb
        d  = (q.rng[c].next_f64() − q.rng[c].next_f64()) * q.Δ
        qi = round_half_even((v + d) * q.S)
        yi = clamp(qi, −q.S, q.S − 1); if yi != qi { q.clip_count += 1 }
        e  = clamp(yi / q.S − v, −4q.Δ, 4q.Δ)
        shift err_hist[c]; err_hist[c][0] = e
        out[n][c] = yi as i32
```

### Parameters

| Parameter | Value | Configurable |
|---|---|---|
| Dither type | TPDF ±1 LSB | advanced: TPDF / none (none only with a warning) |
| Noise shaping | off; options: lipshitz-44.1 (after verification) | yes, 16-bit only |
| Rounding | round-half-even in f64 | no |
| Clamp | [−2^(N−1), 2^(N−1)−1], counted | no (counts go into the export report) |
| PRNG | PCG32 / xoshiro, seeded per channel | no |

## Alternatives Considered

- **RPDF**: removes distortion but leaves noise modulation. Rejected.
- **Gaussian dither**: more noise for equal benefit. Rejected.
- **High-pass TPDF** (difference of successive uniforms, r[n] − r[n−1]): a cheap, slightly blue spectrum, an alternative to flat TPDF. Not needed for MVP.
- **Always 16-bit with heavy noise shaping**: rejected. 24-bit is the default export, which makes shaping irrelevant.
- **No dither at 24-bit**: defensible (rounding error at −144 dBFS is below any DAC). Kept TPDF for uniformity and correctness; cost is nil.

## Evidence / Sources

- S.P. Lipshitz, R.A. Wannamaker, J. Vanderkooy, "Quantization and Dither: A Theoretical Survey", JAES 40(5):355–375, 1992.
- R.A. Wannamaker, "Psychoacoustically Optimal Noise Shaping", JAES 40(7/8), 1992; Lipshitz, Vanderkooy, Wannamaker, "Minimally Audible Noise Shaping", JAES 39(11), 1991.
- SoX `dither` effect source (`src/dither.c`, `src/dither.h`), filter coefficients (to verify).
- Local numeric check (numpy): TPDF error RMS −96.331 dBFS (expected −96.330); variance 0.2495–0.2504 LSB² across input offsets 0–1 LSB; undithered error = deterministic −0.3 LSB at offset 0.3 LSB (variance 0).

## Recommendation

Flat TPDF on every float→int conversion. Never dither float or lossy-encoder input. Bypass when the signal is already quantized. Deterministic per-channel PRNG. Noise shaping optional, 16-bit at 44.1 kHz only, after its coefficients are verified. Clip count reported. (H)

## Risks

- Mismatched float↔int scale conventions between our reader and writer, which breaks bit-exact round trips. Mitigation: test D6.
- Noise-shaper instability on clipped material. Mitigation: error clamp plus block fallback (D7).
- Users comparing nulls of two exports with different seeds. Mitigation: seed is derived from settings; documented.

## Open Questions

- Verify the SoX coefficients, and obtain/validate 48 kHz shaping filters, or drop shaping at 48 kHz.
- Should the export report display the dither noise floor (e.g. "16-bit TPDF: −96.3 dBFS")? Probably yes, in the advanced view.

## Tests (acceptance)

| ID | Test | Pass criterion |
|---|---|---|
| D1 | Silence + forced dither (bypass disabled), 16-bit, 10 s | error RMS = −96.33 dBFS ± 0.05 dB; 24-bit: −144.49 ± 0.05 dB |
| D2 | Moment independence: DC inputs at 0, ¼, ½, ¾, 1 LSB (2·10⁶ samples each) | mean error ≤ 0.002 LSB; variance 0.25 ± 0.002 LSB² for every offset |
| D3 | −100 dBFS 1 kHz sine → 16-bit; FFT (Blackman-Harris, 2^16, 64 averages) | fundamental within ±0.5 dB of −100 dBFS; no harmonic spur > 6 dB above the local noise floor. Undithered control shows harmonics (proves the test is sensitive). |
| D4 | Float output | bytes == f32 render (no dither applied) |
| D5 | Determinism | same seed → identical bytes; L/R dither cross-correlation |ρ| < 0.005 |
| D6 | Bypass / round-trip: 16-bit source → decode → (identity) → 16-bit export | output samples == source samples bit-exact (also 24-bit) |
| D7 | Noise shaping (when enabled): white noise −20 dBFS; full-scale square with 3 dB overs | error spectrum follows the NTF within ±1 dB (1–18 kHz); no runaway (|e| ≤ 4Δ always); fallback logged on clip blocks |
| D8 | Clamp | +1.0 → 32767, −1.0 → −32768, +1.5 → 32767 with clip_count incremented; no wraparound |
| D9 | NaN/Inf input | export aborts with a typed error before any byte is written to the final path |
