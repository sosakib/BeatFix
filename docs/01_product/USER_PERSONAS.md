# User Personas & Journeys

Date: 2026-10-08 · Author: Agent D (Product/Competitor Research) · Grounding: `docs/03_research/COMPETITOR_RESEARCH.md` (finding IDs F1–F10, opportunity IDs D1–D12).
Status: these personas are **hypotheses** built from desk research (official docs + *anecdotal* forum/Reddit/blog evidence). Validate with 5–8 user interviews before final scope lock.

## Objective
Define who BeatFix serves first, what each persona is trying to get done, and the end-to-end journeys the MVP must support.

## Why It Matters
The competitor scan shows that the capability already exists (DAWs, Suno Studio, web fixers). The opening is in **who is underserved**: Windows/FL users without Logic, Suno users without Premier or DAW skills, and quality-sensitive users who don't trust cloud black boxes. Personas keep scope pointed at those gaps.

## Key Findings
- The primary market is **Suno users**. Udio exports have been disabled since 2025-10 (F9).
- The pain is concrete and documented: drift breaks DAW sync by bars 16–32 (*anecdotal* blog), stems drift after export (F2), non-integer BPM (F10), and manual warp markers every few bars is the standard workaround.
- Users fear **groove flattening** and **artifacts** as much as drift (F7).

---

## Personas

### P1 — "Maya", the Suno Creator (primary)
- **Who:** content creator / hobby songwriter, 18–40. Makes 10–50 Suno songs a month for YouTube/TikTok, small releases, and sync pitches. Suno Pro (not Premier). Windows laptop. No DAW, or knows CapCut/Audacity only.
- **Goal:** "Make the song sit on a steady beat so my video edit / DJ friend / producer can use it."
- **Context:** downloads MP3/WAV from Suno. Has hit "the beat drifts against my edit grid" or "the producer said it's not on grid."
- **Pains:** doesn't know what a warp marker is. Suno's fix needs Premier + Studio (F1). Web fixers are cloud and opaque. Can't judge whether the result is good.
- **Needs:** one-click fix (D1), plain-language confidence ("Great / Check these 2 spots / Not suitable") (D2), a before/after toggle, a sane default BPM (D1), MP3/WAV export.
- **Success metric:** fixed file exported in < 2 min without reading docs. Zero settings touched in ≥70% of songs.
- **Won't tolerate:** jargon, accounts/cloud upload, audible warble on vocals.

### P2 — "Dev", the Hybrid Producer (primary)
- **Who:** bedroom/semi-pro producer, FL Studio on Windows (owns NewTime via Producer edition) or Ableton. Uses Suno songs and stems as raw material for remixes, flips, and demos.
- **Goal:** "Get a Suno track and its stems onto a fixed project BPM fast, without killing the groove, so I can build on it in FL."
- **Pains:** NewTime is single-BPM per sample with manual markers (F6). Ableton auto-warp drifts or places markers beside transients. Spends 20–60 min per song placing markers every few bars. Stems warped separately don't line up (F2). Octave/half-time misdetections.
- **Needs:** ranked tempo hypotheses (D3), Natural vs Strict (D4), correction heatmap to spot-check hotspots (D5), editable beat map separate from the correction (D6), linked stems (D8, post-MVP), lossless export with first downbeat at 0 (D10).
- **Success metric:** under 5 min from import to stems-on-grid in FL, with no audible flam between stems.
- **Won't tolerate:** lossy re-encodes, losing the original, being forced into full-auto with no override.

### P3 — "Rico", the DJ / Mashup Editor (secondary)
- **Who:** club/mobile DJ, rekordbox or Serato. Plays AI edits, old disco/funk, and live-drummer records. Prepares tracks in batches.
- **Goal:** "Tracks that beatgrid as one Fixed BPM so sync and loops never slip."
- **Pains:** Dynamic/Flexible grids describe drift but don't remove it, and editing sometimes flattens a dynamic grid back to one tempo (Serato 5.0 beta review). Manual grid locking per section.
- **Needs:** batch fix to a target BPM (or nearest integer), strong transient preservation on drums, BPM written to metadata/filename (D10), a Natural setting that keeps swing. Speed matters more than per-bar editing.
- **Success metric:** a 20-track folder fixed unattended. Grids show as Fixed in the DJ app with no drift at the outro.

### P4 — "Lena", the Quality-First Engineer (secondary / validator)
- **Who:** mix/mastering or post engineer. Receives AI-assisted or live-recorded material from clients and must deliver to a click (sync, ads, stems for remix contests). Owns Pro Tools/Cubase/Melodyne.
- **Goal:** "A defensible, artifact-minimal conform with a paper trail of what changed."
- **Pains:** DAW real-time stretch quality < offline (Cubase "flatten", Pro Tools X-Form is offline). Per-track stretch breaks inter-track phase. Clients' Suno files at odd BPMs.
- **Needs:** offline high-quality render, max-stretch warnings (D12), per-region exclusions (D9), project file = source + map (D7), a session report (detected BPM, target, max/mean stretch, low-confidence spans).
- **Success metric:** passes a critical listen on monitors. The report answers "what did you do to my audio?"

**Priority:** P1 and P2 drive MVP scope. P3 drives batch/export post-MVP. P4 drives quality gates and reporting.

---

## User Journeys

Notation: **[Auto]** = system does it, **[User]** = user action, **[Gate]** = confidence/quality decision point. Times are targets, not measurements.

### J1 — One-click Suno fix (P1) · MVP
1. [User] Drags `song.mp3` (Suno, 3:20) onto the window.
2. [Auto] Decode → full-song analysis (beats, downbeats, local tempo, confidence). Progress shown. Target ≤ 15 s for a 4-min song on a mid laptop (PROVISIONAL, REQ-PERF-001).
3. [Auto] Summary card: "Detected ~107.6 BPM, drifts 105.9–109.2 · Confidence: High · Suggested target: **108**."
4. [Gate] High confidence → the **Fix** button is primary. (Medium/Low → J3/J5.)
5. [User] Clicks **Fix**. [Auto] Renders from source (Natural default).
6. [Auto] Shows original vs corrected waveforms + heatmap. Playback with an A/B toggle and metronome click.
7. [User] Export → WAV (or MP3 if the source was MP3, with a lossy→lossy notice). Filename `song_108bpm.wav`.
- **Exit criteria:** ≤ 3 clicks after drop. No dialogs on the happy path.
- **Failure branch:** step 3 shows Low → the Fix button becomes "Review first" (J5).

### J2 — Wrong tempo octave / meter (P2) · MVP
1. [User] Imports a half-time trap Suno song. [Auto] Detects 70 BPM. Hypotheses ranked: 70 (0.52), **140** (0.41), 93.3 (0.07).
2. [User] Hears the click is half-speed and clicks the "140" chip. [Auto] The grid redraws instantly (no re-analysis; the beat map is derived).
3. [User] Downbeat looks off by one beat → "Shift bar start" (‹ ›) or clicks the real "1" to set it (Set-first-downbeat).
4. [Auto] Re-derives correction. [User] Fix → export.
- **Grounding:** x2/÷2/x4/3/x3/4/offbeat buttons exist in Ableton/Cubase/Logic but users must know to use them (F4). BeatFix surfaces them as ranked options (D3).

### J3 — Keep the groove: Natural vs Strict (P2) · MVP
1. [User] Imports a Suno neo-soul song. Fixes with the default Natural.
2. [User] Opens the heatmap: mostly neutral, two orange hotspots at the chorus pickup.
3. [User] Switches to **Strict** and A/Bs. Drums tighter, vocals feel robotic. Moves the slider back to 30%.
4. [Auto] Re-renders from source (never from the previous render). The heatmap updates and the max local stretch reads 3.1%.
5. [User] Exports FLAC 24-bit for FL.
- **Grounding:** groove-flattening warnings in Suno timing guides. DJ apps' Fixed/AI/Flex split (F7, D4, D5).

### J4 — Drift-free stems in FL Studio (P2) · post-MVP (D8)
1. [User] Imports the Suno mix + 4 stems as a **linked set**.
2. [Auto] Analyses the mix (or user-chosen stem, e.g. drums) and builds one correction map.
3. [Auto] Applies the identical map to all files and checks sample-length equality and inter-stem alignment.
4. [User] Exports a folder: `mix_128.wav`, `drums_128.wav`, … first downbeat at t=0.
5. [User] Drops them into FL at 128 BPM. No drift at bar 64.
- **Grounding:** stems drifting after a Suno tempo-lock export (F2, *anecdotal*). The baseline research flags per-track stretch phase issues.

### J5 — Low confidence / intentional tempo change (P1, P4) · MVP (basic) + post-MVP (advanced)
1. [Auto] The analysis finds a 16-bar ambient intro with no percussion (low confidence) and a deliberate slowdown in the outro.
2. [Gate] Summary: "Confidence: Medium — 2 spans need a look." The spans are highlighted on the timeline.
3. [User] Picks per span: **Interpolate** (default: stretch evenly between confident anchors), **Leave as is**, or (post-MVP) **Own tempo**.
4. [Auto] The heatmap shows the excluded spans hatched. The render respects the choices.
5. [Gate] If the required local stretch exceeds the safe band (≥ 8 % warn, ≥ 12 % extreme; DECISION_SUMMARY §6): warning "Heavy correction at 2:41 (+12%). Consider regenerating this part in Suno."
- **Grounding:** web fixers admit failure on rubato, intentional changes, and beatless sections (F8, D2, D9, D12).

### J6 — DJ batch prep (P3) · post-MVP
1. [User] Selects a folder of 20 tracks, chooses Target: "nearest integer" + Natural + "write BPM tag".
2. [Auto] Runs the queue. High-confidence tracks auto-render. Medium/Low tracks are parked in a "Needs review" list.
3. [User] Reviews the parked tracks via J2/J5.
4. [Auto] Exports with BPM in metadata/filename and a summary CSV (detected, target, max stretch, confidence).
- **Grounding:** rekordbox/Serato/DJ.Studio describe drift with dynamic grids but don't render corrected audio (D10, D11).

---

## Recommendation
- Design the MVP around **J1, J2, J3, J5 (basic)**. Treat J4 and J6 as the first post-MVP increments.
- Default behaviour: Natural strength, nearest-integer target, render from source, export lossless unless the user picks otherwise.
- Write UI copy for P1 (no "warp", "flex", "transient"). Reveal P2/P4 depth (beat-map editing, report) progressively.

## Risks
- Personas rest mostly on Low-confidence (*anecdotal*) evidence for AI-music pain. Interviews are needed.
- P1 may be satisfied by Suno's in-app Manual BPM if they upgrade to Premier. BeatFix must win on cost (one-time vs subscription; pricing TBD), offline use, and transparency.
- Trying to serve P4 fully in v1 risks bloating scope.

## Open Questions
1. What share of Suno users own a DAW (P2) vs none (P1)? It sets how much manual editing UI the MVP needs.
2. Do P1 users export MP3 or WAV from Suno? It affects the lossy-input UX and quality expectations.
3. What is the acceptable "Fix" time budget? Is it ≤ 30 s per song?
4. Pricing model expectation (one-time vs subscription) for P1 vs P2.

## Implementation Implications
- Required state: source handle (immutable), beat map + per-beat confidence, ranked tempo hypotheses, correction params (target, strength, exclusions), derived per-segment stretch array (heatmap + warnings), render cache keyed by params hash.
- The UI needs: summary card with a confidence badge, hypothesis chips, strength control, heatmap lane, A/B toggle, span-level actions, and an export dialog with format/metadata options.
- The test corpus must cover each journey: clean drift (J1), half-time (J2), groove-heavy (J3), mix+stems (J4), beatless intro + ritardando (J5), mixed-genre batch (J6).
