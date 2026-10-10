# Definition of Done

Three levels. A task is never "done" because the code compiles. Phases and gates are in [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md).

## 1. Task (one PR)
- [ ] Traces to a REQ ID or a roadmap task. The PR description names it.
- [ ] Builds warning-free (MSVC `/W4 /WX`), clang-format clean, clang-tidy clean ([CODING_STANDARDS.md](../08_engineering/CODING_STANDARDS.md)).
- [ ] Non-trivial logic leaves at least one runnable test (unit or DSP) that fails if the logic breaks. Existing tests pass.
- [ ] No float32 time values. No new interface beyond `ITimeStretcher` / `IBeatAnalyzer` without an ADR.
- [ ] RT-thread code: no allocation, lock, log or I/O (the debug detector stays silent).
- [ ] New dependency: licence on the allow-list, pinned with SHA256, added to TECH_STACK and NOTICE.
- [ ] User-visible text follows the UX copy rules (no restoration claims, REQ-AUDIO-013).
- [ ] Docs touched by the change are updated in the same PR (link, don't duplicate).

## 2. Phase (gate)
- [ ] Every phase exit criterion in the roadmap is met, with evidence (test report, benchmark JSON, screenshots) attached to the phase tag.
- [ ] Relevant AT-* acceptance tests ([ACCEPTANCE_CRITERIA.md](../07_quality/ACCEPTANCE_CRITERIA.md)) pass for the scope delivered.
- [ ] Performance smoke within budget for the features delivered (PERF-* subset).
- [ ] Determinism level D1 holds for new DSP code. D2 is checked on a second machine at phases 4, 5 and 8.
- [ ] Any PROVISIONAL threshold touched in the phase is calibrated or explicitly carried forward in OPEN_QUESTIONS.
- [ ] Risk register and open questions are updated (closed items marked with evidence).
- [ ] No open P1 bugs in the phase scope.

## 3. Release (MVP 1.0)
- [ ] Every MVP-Must requirement is verified by its stated method. MVP-Should items are either verified or have a recorded slip decision.
- [ ] All AT-01…AT-25 pass on REF-MID ([ACCEPTANCE_CRITERIA.md](../07_quality/ACCEPTANCE_CRITERIA.md)).
- [ ] Listening gate: Smooth non-inferior (≤ 5 MUSHRA points) to the Rubber Band R3 reference on drums, vocals and dense mix. Natural default confirmed by S-3.
- [ ] Corpus benchmark meets the frozen thresholds (anchor error, spurious/missing anchors, octave errors, fine BPM error) on S/M/C tiers, plus a local U-tier report.
- [ ] Full PERF suite on REF-MID meets REQ-PERF-001…014 (or documented waiver).
- [ ] Security: decoder fuzz job green, network-silence test green, log-redaction test green.
- [ ] Licence: SBOM reviewed. No GPL/AGPL/NC component shipped. LGPL DLLs replaceable. NOTICE complete. Legal sign-off on JUCE tier, Beat This! training-data caveat, product name.
- [ ] Installer and binaries signed. Clean-machine install/uninstall tested on Windows 11 and Windows 10 22H2.
- [ ] Docs: README, user help, known limitations (incl. "no AAC/Opus", "MP3 gapless in DAWs unverified"), changelog.
