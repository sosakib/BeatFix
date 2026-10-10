# Self-Critique and Consistency Review (brief §46–47)

Review pass on 2026-10-10 over the whole `docs/` tree, after synthesis.

**Process caveat:** this pass was done by the orchestrating session, not an independent agent. Usage limits repeatedly stopped the parallel agents. **Recommendation:** before Phase 0 starts, have a fresh session (or a human DSP engineer) review `DECISION_SUMMARY.md`, `WARP_FUNCTION.md`, `TIME_STRETCH_ENGINE.md` and `IMPLEMENTATION_ROADMAP.md` cold.

## 1. Answers to the §47 questions

**What is technically hardest?**
1. *Beat-tracking continuity* on full mixes. One missed or extra beat, or a mid-song octave switch, produces a 2× or ½ local stretch. F1 scores hide this. Mitigated by choosing the octave before decoding, the ±40 % corridor, IOI repair, and the hard ratio bound. Still the #1 technical risk (R-01).
2. *Stretch quality on dense, loud AI masters* with a permissive engine. Signalsmith's quality relative to Rubber Band R3 is supported only by others' listening reports (R-02).
3. *Calibration without legally clean real data.* Suno ToS and dataset licences restrict exactly the material the product targets (R-35, R-37).

**What is most likely to sound bad?**
Snare and hi-hat smearing and "phasiness" on wide reverberant masters in Smooth mode. Strict mode on noisy anchors (audible jitter). Chunk seams if the overlap is too short. Peak overs (handled by the gate, but users may pick "export anyway").

**Where can beat detection fail?**
Half/double-time genres (trap, drum & bass), drumless intros and breakdowns, swing and shuffle, Suno Extend seams (splices), rubato or ambient material, heavy sidechain pumping (false periodic onsets), and lossy-origin transients softened by codecs. Beat This! is also *confidently* wrong on some material (SMC findings), which is why confidence is calibrated, not trusted.

**Where can the stretch engine produce artifacts?**
Transients (smearing, pre-echo), sustained tonal material (phasiness at larger ratios), stereo image (handled by linked phase), chunk joins, and ratio steps at beats in Strict mode. Artifacts grow with |r − 1|, which is why the warn bands exist.

**Which design decisions are still assumptions?**
| Assumption | Where | How it gets tested |
|---|---|---|
| Natural N_c = 8 is the right default | WARP_FUNCTION §4.3 | listening S-3, Phase 8 |
| Suno drift is slow (ramps, steps) and stable songs sit on integer BPM | BPM_ESTIMATION §5, TEMPO_STABILITY | annotated corpus ≥ 50 (M-8) |
| Signalsmith ≈ R3 at ≤ 3 % | ADR-006 | Phase 0 listening (M-2) |
| Signalsmith variable-chunk drive aligns < 0.5 ms | TIME_STRETCH_ENGINE §3.2 | Phase 0 impulse test (M-1) |
| minimp3_ex honours LAME gapless exactly | ADR-009 | Phase 0 click test (M-5) |
| Performance targets (0.15 / 0.25 × RT, 500 ms preview) | PERFORMANCE_BENCHMARKS | Phase 0 / REF-MID |
| ±25 ms fixed-band onset snap improves anchors | WARP_FUNCTION §5 | corpus A10 (M-9) |
| Correction-class thresholds (0.5/2/5 %) match perception | WAVEFORM_VIEW | 2AFC calibration |

**Which libraries create licensing risk?**
JUCE (commercial tier must be bought, JUCE 9 EULA unconfirmed), Beat This! (training-data caveat), LAME/libmpg123 (LGPL obligations), Rubber Band (GPL, so dev-only), the Fraunhofer transient patent (blocks Slice, reviews Transient), AAC (excluded). See RISK_REGISTER R-30…R-37.

**Which requirements are unrealistic?**
- *Effort:* the roadmap's 40–52 weeks for one developer is realistic for this quality bar but long. The biggest schedule risks are calibration and listening (R-40), not coding.
- *REQ-PERF-001 (analysis ≤ 0.15 × RT) with the full Beat This model on a mid-range x86 laptop* is unverified (the only data point is an Apple M4 running a Rust port). The fallback to the small model is built in.
- *REQ-STR-005 (Path K ≤ 0.5 ms P95)* is plausible for impulses but may need relaxing to ~1 ms on real transients. It is PROVISIONAL.
- *Bit-exact pass-through at r = 1* is not required, by design (−90 dBFS null instead).

**Which features should be removed from the MVP (first to cut if late)?**
In order: Resample mode (MVP-Should), 3/4 meter support, DAW-ready start, tempo graph, metadata carry-over, click overlay. None of these affect the core promise. **Not** to be cut: confidence display, candidate picker, Natural↔Strict, the peak gate, export validation, autosave. Those are the trust features.

**What would make a professional producer distrust the tool?**
Silent changes (gain, limiting, re-analysis), a confident wrong tempo, one audible click, no way to see *what* was changed, an MP3 export offset by 25 ms, cloud upload. Every one of these has an explicit countermeasure: no silent processing (REQ-AUDIO-008), confidence plus candidates, V1/V3 validation, the correction heatmap and connectors, gapless round-trip tests, and no network (REQ-SEC-001).

**What makes BeatFix substantially better than a NewTime-like workflow?**
A whole-song automatic tempo map (NewTime is per-sample/manual), confidence-aware automation that asks only when unsure, groove-preserving correction with an explainable strictness control, a correction heatmap plus original/corrected comparison, always rendering from source (unlimited iteration without generation loss), DAW-ready start, and offline/local processing of any source file.

## 2. Consistency review: issues found and fixed in this pass

| # | Issue | Fix |
|---|---|---|
| 1 | UI offered Auto/Transient modes and Reset → Auto in the MVP | MVP modes are Smooth + Resample everywhere (MAIN_WINDOW, UX_BEHAVIOR, UI_UX_PRINCIPLES, WAVEFORM_VIEW) |
| 2 | Candidate range 50–220 BPM in the UI vs 40–250 ×{1/3…3} in the algorithms | UI aligned |
| 3 | Export defaults (MP3 V0, dither only for 16-bit, post-export peak warning) | CBR 320, TPDF for all integer outputs, pre-encode peak gate (EXPORT_UI §3.1) |
| 4 | Three ratio thresholds (8/12/15 %) looked contradictory | defined as different quantities (DECISION_SUMMARY §6, SRS §2.4) |
| 5 | WARP_MAP defined `g` inverted (B_target/B_detected) | corrected to B_detected/B_target (= r convention) |
| 6 | RENDER_PIPELINE played original audio as B in unrendered spans, against PLAYBACK_AND_AB | B pauses with a buffering indicator. Uncorrected audio is never presented as B |
| 7 | Kept intentional-tempo sections broke the single-φ grid rule | TARGET_BEAT_GRID §2.1 (piecewise grid) |
| 8 | Output origin undefined when W(0) ≠ 0 | TARGET_BEAT_GRID §5.1, REQ-GRID-003 updated |
| 9 | Crash dumps "opt-in" vs "written by default" | written locally by default, never uploaded, can be disabled |
| 10 | Cache budget 5 vs 10 GB; hash XXH3 vs SHA-256 | 5 GB LRU. SHA-256 via Windows CNG everywhere |
| 11 | Own SRC vs r8brain; dr_libs vs JUCE readers | r8brain + contract tests. JUCE readers + minimp3_ex |
| 12 | Roadmap referenced open-question/risk IDs that did not exist | remapped to OPEN_QUESTIONS (O-/L-/M-) and RISK_REGISTER (R-) IDs |
| 13 | 29 requirement IDs were referenced before being defined | all defined. Automated check: 0 undefined REQ IDs, 0 broken relative links (624 checked) |
| 14 | Calibration models planned on Suno outputs (ToS item 10) | fit on S/M/C tiers only. Suno use pending legal review (L-5) |

## 3. Remaining known weaknesses (accepted, tracked)
- The research web-fetch quota ran out during some tracks. Several facts are marked **UNVERIFIED** (JUCE 9 pricing page, Suno file specs, some library versions, DAW gapless support). None blocks Phase 0, and each has an owner in OPEN_QUESTIONS.
- No listening tests or real benchmarks were run during planning. Every quality judgement about engines is second-hand until Phase 0. The one exception is AUDIO_QUALITY_RESEARCH's own measurements of codec offsets and peak growth (`03_research/_evidence/`).
- Some track documents in `03_research/` predate final decisions (e.g. they discuss the Rust/Tauri option or "Auto" modes). They are kept as research records. **Where they disagree with DECISION_SUMMARY, the decision summary wins.**
