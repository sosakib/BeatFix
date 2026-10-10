# Competitive Analysis

Status: 2026-10-08, v1.0. Product-facing synthesis only. Per-product workflow profiles, sources and confidence labels are in [COMPETITOR_RESEARCH.md](../03_research/COMPETITOR_RESEARCH.md) (finding IDs F1–F10). This file does not repeat them. Scope status of each differentiator: [PRODUCT_SCOPE.md](PRODUCT_SCOPE.md) §8.

Confidence: High = official docs/manual. Med = reputable secondary. Low = single blog/forum (*anecdotal*). Functional and UX study only. Nothing here describes a competitor's DSP internals.

## 1. Market reality in one paragraph
Whole-song tempo-drift correction is **not a new capability**. Every serious DAW can do it, and since 2026-02 Suno Studio can too (Manual BPM + warp markers + quantize, Premier only) (F1, High). BeatFix can't win on "can it fix tempo". It can win on **who can use it** (no DAW skill, no Premier plan, Windows), **how much it shows** (confidence, correction heatmap), **how safely it renders** (render-from-source, gapless MP3, validated export), and **where it runs** (local, offline).

## 2. Gap map

Condensed from the COMPETITOR_RESEARCH matrix.

| Capability | Who has it | Gap BeatFix targets |
|---|---|---|
| Whole-song variable-tempo detection | All DAWs, DJ apps, web fixers | None. Table stakes |
| Automatic correction render to fixed BPM | DAWs (skill-gated), Suno (Premier, cloud), web fixers (cloud, opaque) | Zero-skill + local + inspectable |
| Octave / meter fixes | Manual buttons everywhere (Cubase richest: ×2, ÷2, ×4/3, ×3/4, offbeat) (F4) | Ranked hypotheses with one-click switch (D3) |
| Groove-preserving strength | Fender Quantize %, Pro Tools conform strength, DJ.Studio Fixed/AI/AI Flex (F7) | Natural↔Strict with a measurable meaning (cutoff in beats) and live error readout (D4) |
| Detection confidence shown | Nobody (Logic tempo colours are partial) (F3) | Per-beat + global confidence with reasons (D2) |
| Correction intensity shown | **Nobody** (F3) | Heatmap on residual ratio, numbers on hover (D5) |
| Analysis vs edit separation | Melodyne Assign/Edit, Pro Tools Analysis/Warp, Logic hints (F5) | Same concept, simple UI, aimed at non-experts (D6) |
| Non-destructive render-from-source | DAWs inside their sessions. NewTime saves new files (F6) | Project = source hash + inputs. Every render from source (D7) |
| Linked stems | DAWs (multitrack warp). Suno stems *reported* to drift (F2, Low) | Post-MVP (D8) |
| Standalone, Windows, offline | NewTime (inside FL), Melodyne. Logic is Mac-only. Suno/web are cloud | Standalone Windows, no network calls (D11) |

## 3. Differentiators D1–D12

Ranked by gap size × feasibility × fit to Suno users (COMPETITOR_RESEARCH "Recommendation").

| ID | Differentiator | Why it's defensible | Weakness / risk | Scope | Feature |
|---|---|---|---|---|---|
| D1 | One-click fix with a smart target (nearest integer, exact value shown) | Suno needs Premier + Studio. DAWs need marker skills | Suno could make Manual BPM free | MVP | F-009, F-013 |
| D2 | Confidence-aware automation | No competitor exposes confidence | Thresholds unproven until corpus calibration | MVP | F-006, F-019 |
| D3 | Ranked tempo hypotheses | Turns hidden buttons into visible choices | Halftime genres stay ambiguous for humans too | MVP | F-005, F-008 |
| D4 | Natural vs Strict | Groove flattening is a stated user fear | Default N_c = 8 is an assumption | MVP | F-011 |
| D5 | Correction heatmap | Nobody shows stretch intensity | Bin edges come from click-sequence JNDs, not music | MVP | F-016 |
| D6 | Analysis ↔ correction separation | Proven pro concept, simplified | Hard to explain to P1. UI copy must avoid jargon | MVP | F-008 |
| D7 | Render from source, never re-encode | DAW "flatten" / NewTime save-to-file workflows | — | MVP | F-002, F-013 |
| D8 | Linked stems | Suno stem drift complaints | Evidence Low (*anecdotal*) | Phase 2 | F-104 |
| D9 | Section-aware exceptions | Web fixers admit failure on intentional changes | Detector may flag live-feel pushes as splices | MVP basic / Phase 2 | F-012, F-103 |
| D10 | DAW/DJ handoff (BPM tag, DAW-ready start, later MIDI/tempo map) | Users re-type BPM in their DAW (Suno help) | Downstream gapless/tag handling varies by app (UNVERIFIED) | MVP part / Phase 2 | F-020, F-106 |
| D11 | Offline, private, Windows-first | Logic Mac-only. Suno/web cloud | Desktop install friction vs a web page | MVP | F-023 |
| D12 | Quality transparency (warn outside the safe ratio band, suggest regenerating) | Artifact complaints across Ableton/Suno | Per-region mode suggestion needs Auto (not MVP) | MVP warnings | F-016, F-019 |

## 4. Pricing context

List prices as found 2026-10-08. Med confidence unless noted. Details and sources: COMPETITOR_RESEARCH §1–11.

| Product | Price model | Tempo-correction access |
|---|---|---|
| Suno Premier | ~$30/mo, $24/mo annual | Manual BPM, warp markers, quantize (Studio 1.2) |
| FL Studio | Producer ~$199, Signature $299, All Plugins $499 (one-time) | NewTime from Producer edition |
| Ableton Live | Intro $99 / Standard $439 / Suite $749 | Warp in all editions |
| Logic Pro | $199.99 (Mac only) | Smart Tempo + Flex Time |
| Cubase 14 | Pro $579.99 / Artist $329.99 / Elements $99.99 | Tempo Detection panel is Pro-only |
| Fender Studio Pro 8 | $199.99 perpetual or $19.99/mo | Audio Bend + tempo detection |
| Melodyne 5 | Essential $99 / Editor ~$399 / Studio $699 | Tempo assign/edit from Editor up |
| Pro Tools Ultimate | $599/yr or $99/mo | Elastic Audio, Beat Detective |
| REAPER | $60 discounted / $225 commercial (High) | Stretch markers + SWS |
| Web fixers (Neural Analog) | Credits / freemium, amounts **UNVERIFIED** | One-click cloud |
| Mixed In Key 11 | $58 | No audio correction |

Implication: the cheapest non-cloud route to a corrected Suno song today is a DAW at $60–$200 plus skill, or Suno Premier at ~$288/yr. BeatFix's price point is **undecided** (owner decision, DECISION_SUMMARY §8). It also sets the JUCE tier and whether a Rubber Band commercial licence (£590+) pays off ([LIBRARY_EVALUATION.md](../03_research/LIBRARY_EVALUATION.md)).

## 5. Threats
1. **Suno closes the gap** (free tempo lock, better generation timing). Mitigation: be source-agnostic (live recordings, old records, samples, DJ prep). Compete on fidelity + transparency.
2. **Detection incumbents improve** (Serato Lucid Beatgrids beta, Fender AI tempo, Logic). They still don't render corrected audio for DJs.
3. **Expectation risk**: one-click marketing vs rubato, ritardando, beatless intros. Mitigation: confidence gating (D2) and flags (D9). Never claim "works on every song".
4. **Evidence quality**: AI-music pain is mostly *anecdotal* (blogs, Reddit). Validate with 5–8 interviews before heavy investment ([USER_PERSONAS.md](USER_PERSONAS.md)).

## 6. Naming and IP rules for product and marketing
From [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) §8–9:
- No competitor marks in product names, feature names or UI: "NewTime", "FL Studio", "Warp"/"Auto-Warp" as a brand, "Flex", "Smart Tempo", "Audio Bend", "Lucid", "Complex/Complex Pro", "élastique". Use generic terms: "beats", "grid", "fix strength", "warp map" (lower-case, descriptive).
- Nominative mentions only ("for songs from Suno"), with no logos and no implied endorsement. **For legal review** before any comparative marketing.
- "BeatFix" itself is **not cleared** (trademark search pending).

## 7. Open competitive questions
1. Does Suno's Manual BPM change downloaded audio/stems or only Studio playback? Test with a Premier account.
2. Quality of Neural Analog / Wondera output on our corpus, and their pricing.
3. Is NewTime still single-BPM in current FL builds? Confirm hands-on.
4. Distribution of real Suno drift magnitudes. This sets how often one-click is enough (feeds the corpus, [BENCHMARK_CORPUS.md](../07_quality/BENCHMARK_CORPUS.md)).
