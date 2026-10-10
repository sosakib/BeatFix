# Onset Detection (DSP Novelty Functions)

Status: algorithm spec, 2026-10-08. Sources: [BEAT_TRACKING_RESEARCH.md](../03_research/BEAT_TRACKING_RESEARCH.md) §1, §5, §13; [DSP_RESEARCH.md](../03_research/DSP_RESEARCH.md) TD-1, TD-3; [TEMPO_ESTIMATION_RESEARCH.md](../03_research/TEMPO_ESTIMATION_RESEARCH.md) §1, §3.

## Objective
Specify the deterministic DSP onset/novelty layer of `bf_analysis`: which onset detection functions (ODFs) are computed, with what parameters, how peaks are picked, and what each output is used for.

## Why It Matters
In the main path, beats come from Beat This! activations plus our decoder ([BEAT_TRACKING.md](BEAT_TRACKING.md)). **Beats are not onsets.** Treating every transient as a beat is the failure the brief warns against (§7.9). The DSP layer still has five jobs that ML at 50 fps cannot do:

| # | Use | Consumer | Output used |
|---|---|---|---|
| U1 | Sub-frame anchor refinement (transient snap, ±1–3 ms) | [WARP_FUNCTION.md](WARP_FUNCTION.md) §5 | `odf_hr` (high-res, one band per song) |
| U2 | Kick/snare evidence for octave choice and bar phase | [HALF_DOUBLE_TIME_RESOLUTION.md](HALF_DOUBLE_TIME_RESOLUTION.md), [DOWNBEAT_ESTIMATION.md](DOWNBEAT_ESTIMATION.md) | `odf_low`, `odf_mid` |
| U3 | Fallback beat tracker (model missing or failed) | [BEAT_TRACKING.md](BEAT_TRACKING.md) §6 | `obs_dsp` (combined, normalised) |
| U4 | Independent cross-check tracker for confidence | [BEAT_TRACKING.md](BEAT_TRACKING.md) §5 | `obs_dsp` → `B_dsp` |
| U5 | Fill / ghost-note / flam detection, `transientStrength` per anchor | [OUTLIER_REJECTION.md](OUTLIER_REJECTION.md), [WARP_MAP.md](WARP_MAP.md) | band ODFs + peak lists |

The DSP layer is always computed, even when the model succeeds. It is cheap (well under 1 s for a 4-min song, UNVERIFIED until the Phase 0 benchmark).

## 1. Input conditioning
- Input: decoded float32 PCM at the **native** sample rate `sr` (never the 22.05 kHz model buffer). Mono = (L+R)/2 for analysis only. Stereo is never altered.
- Remove DC with a 1-pole high-pass at 20 Hz (analysis buffer only).
- Damaged spans reported by the decoder (UX_BEHAVIOR.md §14) are zero-filled. Peaks inside them are discarded and flagged.
- All STFT parameters are defined as **durations** and converted per `sr` (rounded to the nearest power of two for N, nearest integer for hop). All time stamps are float64 seconds: `t = frame · hop / sr`, frame centred (STFT with half-window padding).

## 2. STFT configurations (from DSP_RESEARCH.md TD-1)

| Name | Window | N @44.1/48 kHz | Hop | Rate | Purpose |
|---|---|---|---|---|---|
| `stft_main` | Hann | 2048 | sr/100 (441 / 480) | 100 fps | HPSS, SuperFlux, band ODFs, DSP tracker |
| `stft_hr` | Hann | 512 | 128 | ≈ 345–375 fps (2.7–2.9 ms) | High-resolution refinement (U1) |

The 100 fps lag resolution is too coarse for final BPM (ΔBPM per lag = BPM²/(60·fps) = 2.4 BPM at 120 BPM, TEMPO research T6). Final BPM never comes from ODF lags ([BPM_ESTIMATION.md](BPM_ESTIMATION.md) §3).

## 3. HPSS pre-step (Fitzgerald 2010, median filtering)
On `|X(n,k)|` of `stft_main`:
```
H(n,k) = median_{|j| ≤ Lt/2} |X(n+j, k)|        time median, Lt ≈ 0.2 s  → 21 frames @100 fps
P(n,k) = median_{|j| ≤ Lf/2} |X(n, k+j)|        freq median, Lf ≈ 370 Hz → 17 bins @ N=2048, 44.1 kHz
M_p(n,k) = P^p / (P^p + H^p + ε),  p = 2       soft (Wiener-like) percussive mask, ε = 1e-12
X_p(n,k) = M_p(n,k) · |X(n,k)|
```
- Why: sustained pads, vocals and bass produce slow flux and vibrato peaks that mislead every ODF. HPSS removes most of them at the cost of two median filters. It is deterministic and needs no model.
- Non-causal medians are fine (offline). Median lengths are odd and fixed in samples per `sr`.
- This is the **analysis** HPSS. The Phase 1.5 Transient render mode uses its own HPSS instance with render parameters (TRANSIENT_PRESERVATION.md). The code is shared and the parameters are not.
- Percussive ratio per bar, `Pr = Σ|X_p|² / Σ|X|²`, is exported as a feature (Auto/Transient research, Post-MVP consumer).

## 4. Novelty functions

### 4.1 Log-filtered representation
```
Y(n,b) = log10(1 + γ · Σ_k F(b,k) · X_p(n,k))      γ = 1
```
F = triangular filterbank, 24 bands/octave, 30 Hz–17 kHz (clipped at 0.45·sr), unnormalised (madmom `LogarithmicFilteredSpectrogramProcessor` defaults, BEAT_TRACKING_RESEARCH §1.3). Log compression equalises loud and quiet sources.

### 4.2 SuperFlux (Böck & Widmer, DAFx-13). Main ODF
```
Ymax(n−μ, b) = max_{|j| ≤ 1} Y(n−μ, b+j)                     3-band frequency max filter
SF*(n)       = Σ_b H( Y(n,b) − Ymax(n−μ, b) ),   H(x) = max(x, 0)
μ            = max(1, round(0.25 · N / hop))                  diff_ratio 0.5 for Hann ⇒ μ = 1 at 100 fps
```
Why SuperFlux over plain spectral flux: the max filter suppresses false onsets from vibrato and tremolo, which AI vocals and pads produce constantly (BEAT_TRACKING_RESEARCH §1.3). Plain SF (`Σ_k H(|X(n,k)| − |X(n−1,k)|)`) is kept only as a test baseline.

### 4.3 Band ODFs (kick / body / snare / air)
Same SuperFlux formula restricted to filterbank bands whose centre lies in:

| ODF | Band | Musical target |
|---|---|---|
| `odf_low` | 30–150 Hz | kick |
| `odf_lowmid` | 150–500 Hz | toms, snare body (fill detection) |
| `odf_mid` | 1–5 kHz | snare crack, claps |
| `odf_air` | 5–12 kHz | hi-hats (used only to *down-weight* hat-driven periodicity) |

Band edges follow BEAT_TRACKING_RESEARCH §1.5. The low band overlaps the bass. HPSS removes sustained bass but not plucked bass notes, which is one reason kick evidence is weighted, never trusted alone.

### 4.4 High-resolution refinement ODF (U1)
`odf_hr` = SuperFlux on `stft_hr` (HPSS mask from `stft_main` interpolated to the hr frame grid), restricted to **one band per song**: `low` if kick evidence is strong (median `odf_low` peak salience at decoded beats ≥ θ_kick, placeholder 0.5 of the 95th percentile), otherwise broadband 30 Hz–12 kHz. The band is chosen once and frozen, so the reference never switches between kick and snare from beat to beat ([WARP_FUNCTION.md](WARP_FUNCTION.md) §5).

### 4.5 Complex domain (optional, not MVP-critical)
`CD(n) = Σ_k |X(n,k) − |X(n−1,k)|·e^{j(2φ(n−1,k) − φ(n−2,k))}|`, rectified to magnitude increases. Detects soft tonal onsets in drumless intros. Phase is fragile after MP3 coding. Computed only when the intro has no percussive pulse (Pr < 0.1 for ≥ 4 s) and used only as a weak extra observation for the fallback tracker. **Post-MVP** unless Phase 1 tests show benefit.

### 4.6 Normalisation for tracking (`obs_dsp`)
```
o(n)     = w_low·odf_low(n) + w_mid·odf_mid(n) + w_all·SF*(n)     initial 0.4 / 0.4 / 0.2 (placeholder)
o_hp(n)  = H( o(n) − movmean_{1 s}(o)(n) )                         remove slow loudness trend
obs_dsp  = o_hp / max(P95(o_hp), ε)                                 robust scale, same for every song length
```
Hats are excluded on purpose: 16th-note hats create periodicity at 2× or 4× the beat (octave bias toward fast levels).

## 5. Peak picking (only for refinement, cross-checking and event lists)
The main beat path decodes continuous activations and never uses a peak list (BEAT_TRACKING_RESEARCH §1.6).
```
n is an onset peak  ⇔  ODF(n) = max ODF[n − pre_max .. n + post_max]
                    ∧  ODF(n) ≥ mean ODF[n − pre_avg .. n + post_avg] + δ
merge peaks closer than `combine`: keep the higher, ties → lower index
```
| Param | Value (100 fps) | hr ODF |
|---|---|---|
| pre_max / post_max | 30 ms / 30 ms | 10 ms / 10 ms |
| pre_avg / post_avg | 100 ms / 70 ms | 50 ms / 30 ms |
| combine | 30 ms | 8 ms (keeps flam pairs visible for the flam rule) |
| δ | 1.1 on log ODF (SuperFlux paper, Med confidence) | calibrated, placeholder 1.1 |

Sub-frame position of a peak: parabolic interpolation `δn = 0.5(o[n−1] − o[n+1]) / (o[n−1] − 2o[n] + o[n+1])`, clamped to |δn| ≤ 0.5. Salience of a peak = `ODF(n) − mean window` normalised by the song P95 (0..1, clipped). This is `transientStrength` in [WARP_MAP.md](WARP_MAP.md).

## 6. Derived features handed to other stages
| Feature | Definition | Consumer |
|---|---|---|
| `kickAt(t)`, `snareAt(t)` | max of `odf_low` / `odf_mid` within ±35 ms of t, normalised by song P95 | backbeat score `bb(c)`, mid-beat ratio `mid(c)` (HALF_DOUBLE §3); bar phase (DOWNBEAT §3) |
| `fillScore(bar)` | onset density of `odf_lowmid` + `odf_mid` peaks per beat in the bar, divided by the song median | OUTLIER_REJECTION §4 (fill down-weighting, no snapping) |
| `ghostRatio(peak)` | `odf_mid` peak salience / median salience of backbeat-position peaks | OUTLIER_REJECTION §4 (ghost notes never become anchors) |
| `pulsePresence(t)` | 4 s windowed max of `obs_dsp` autocorrelation in the 40–250 BPM lag range | "no-pulse" region detection (BEAT_TRACKING §7, TEMPO_STABILITY §6) |

## 7. Pseudocode
```
function onsetFeatures(mono, sr):
    X   = stft(mono, hann(N_main(sr)), hop = sr/100)               # float32 data, float64 time
    Xp  = hpssPercussive(|X|, Lt = 0.2 s, Lf = 370 Hz, p = 2)
    Y   = log10(1 + logFilterbank(Xp, 24/oct, 30 Hz, min(17 kHz, 0.45 sr)))
    sf  = superflux(Y, mu = max(1, round(N/4/hop)), maxbins = 3)    # float64 accumulation over bands
    low, lowmid, mid, air = superflux over band subsets
    obs = normalise(0.4 low + 0.4 mid + 0.2 sf)
    band = (kickSalience(low) ≥ θ_kick) ? LOW : BROAD               # frozen per song
    Xh  = stft(mono, hann(512), hop = 128)
    hr  = superflux(log10(1 + logFilterbank(interpMask(Xp/X) · |Xh|, band)))
    return {sf, low, lowmid, mid, air, obs, hr, hrBand = band, fps = sr/hop}
```

## 8. Determinism and precision
- Fixed FFT plans (no runtime autotuning), fixed thread count for the per-song pass. Band sums accumulate in float64.
- Ties in max/median filters resolve to the lowest index. No RNG.
- The same `sr`-dependent integer parameters are written into the analysis record (`dspParamsVersion`) so a re-analysis on another machine can be compared at D2 tolerance (DSP_TEST_STRATEGY.md §6).

## Alternatives Considered
| Option | Verdict | Why |
|---|---|---|
| Energy envelope derivative | Reject | Weak on dense limited masters |
| HFC | Feature only | Biased to bright masters and hats |
| Plain spectral flux | Test baseline | Fires on vibrato and tremolo |
| Phase deviation | Reject | Noisy in dense mixes and after MP3 |
| CNN onset detector (madmom) | Reject | CC BY-NC-SA model files |
| Source separation (Demucs) before ODF | Post-MVP | Costly for ≤ 1–2 F1 points (BEAT_TRACKING_RESEARCH F6). A user-supplied Suno drum stem is the cheap alternative (Post-MVP, UNVERIFIED gain) |

## Risks
- Kick band locks onto plucked bass notes on bass-heavy material. Mitigation: HPSS, band choice frozen per song with a salience test, snap requires a salient peak.
- Lossy-codec "swirl" in highs creates spurious air/mid peaks on Suno MP3s (hypothesis H4, UNVERIFIED). Mitigation: hats excluded from `obs_dsp`; mid band stops at 5 kHz.
- HPSS median lengths are untuned for AI masters (DSP_RESEARCH open question 1). Calibrate on the corpus.

## Open Questions
1. Does the 100 fps SuperFlux need μ > 1 (i.e. 200 fps) for vibrato suppression on Suno vocals? Compare false-peak rates on S11 and U-tier items.
2. θ_kick and the `obs_dsp` band weights: fit on corpus categories 9–11 (BENCHMARK_CORPUS.md).

## Tests (Catch2, synthetic signals from DSP_TEST_STRATEGY.md §3)
| ID | Input | Assertion |
|---|---|---|
| ON-1 | S01(a) impulse clicks, 120 BPM, 44.1 and 48 kHz | every click yields one `sf` peak within ±1 frame (10 ms); after parabolic interpolation on `hr`, |error| ≤ 1 ms; zero extra peaks |
| ON-2 | S11 vibrato tone 440 Hz ± 50 cents, 5.5 Hz | SuperFlux false peaks ≤ 10 % of plain-SF false peaks (placeholder; set from first run) |
| ON-3 | S08 kick-only vs snare-only loops | kick hits: `odf_low` salience > 3× `odf_mid`; snare hits: the reverse |
| ON-4 | S10 sustained saw chord + S01(c) kicks at −12 dB | HPSS percussive energy at kick frames ≥ 10 dB above pad frames; no peaks during pad-only spans |
| ON-5 | S07 flams (+15 ms) | `hr` peak list keeps both hits (combine 8 ms); `sf` merges them (combine 30 ms) |
| ON-6 | Determinism | two runs, same build/thread count: ODF arrays bit-identical (D1) |
| ON-7 | `sr` independence | the same click file at 44.1 and 48 kHz gives peak times within ±0.5 ms |
| ON-8 | S14 digital silence, −90 dBFS noise | no peaks, no NaN; `obs_dsp` all zero (ε guard) |
