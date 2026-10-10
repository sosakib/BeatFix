# Licensing, IP, Trademark and Patent Research

Agent C. Research date 2026-10-08. Companion to `LIBRARY_EVALUATION.md`. **Not legal advice.** Every item marked **LEGAL REVIEW** must be confirmed by qualified counsel before commercial release.

Tags: **[V]** verified against primary source this session · **[S]** secondary source · **UNVERIFIED**.

---

## Objective

Determine which candidate dependencies can ship inside a closed-source, commercially distributed Windows desktop app; list obligations (notices, source offers, relinking); flag GPL/AGPL/NC traps; cover codec patents, trademarks (FL Studio / NewTime / "Warp" / Suno), the NewTime functional-reference boundary, and ML-model/data provenance. Output an engineering-safe licence policy.

## Why It Matters

A single copyleft or non-commercial component in the shipped binary either forces source release of BeatFix or makes distribution infringing. Codec patents (AAC) can create per-unit royalties. Trademark misuse in the name/UI/marketing invites takedowns.

---

## Key Findings

### 1. Licence classification of all candidates

| Class | Components | Closed-source shipping |
|---|---|---|
| **Permissive (notice only)** | Signalsmith Stretch (MIT) [V], r8brain-free-src (MIT) [V], libebur128 (MIT) [V], ONNX Runtime (MIT) [V], Beat This! code+weights (MIT) [V], beat_this_cpp (MIT) [V], Dear ImGui (MIT) [V], NAudio (MIT) [V], libsamplerate (BSD-2) [V], libFLAC/libopus/libvorbis (BSD-3) [V], PocketFFT (BSD-3, per beat_this_cpp) [S], madmom **code** (BSD) [V], librosa (ISC) [V], rubato/rten/tract/ort (MIT OR Apache-2.0) [V], Tauri (MIT/Apache-2.0), cpal (Apache-2.0) [V] | Yes. Reproduce copyright + licence text in distribution/About. Apache-2.0: also carry any NOTICE file. |
| **Public domain / MIT-0 / CC0** | dr_libs (PD or MIT-0) [V], miniaudio (Unlicense or MIT-0) [V], minimp3 (CC0) [V] | Yes, no obligation (credit as courtesy). |
| **Weak copyleft, file-level** | Bungee (MPL-2.0) [V], Symphonia (MPL-2.0) [V] | Yes. Modified MPL-covered **files** must be made available under MPL; larger work can be proprietary. Notice required. |
| **Weak copyleft, library-level (LGPL)** | libsndfile (LGPL-2.1) [V], LAME (LGPL, version **UNVERIFIED** this session), libmpg123 (LGPL-2.1), libsoxr (LGPL-2.1+) [V], SoundTouch (LGPL-2.1), FFmpeg default build (LGPL-2.1+) [V], Qt (LGPLv3) [V], wxWidgets (wxWindows licence = LGPL + static-link exception) | Yes **if** dynamically linked and the LGPL checklist below is met. LGPLv3 (Qt) adds installation-information / anti-tivoization duties [V qt.io]. |
| **Strong copyleft → TRAP** | Rubber Band (GPL-2.0+ unless commercial) [V], **Audacity StaffPad TimeAndPitch (GPLv2+ default)** [V], aubio (GPL-3.0) [V], FFmpeg built with `--enable-gpl` (e.g. libx264, libpostproc) [V], `rubberband-sys` crate (GPL-2.0+) [V], `flac` CLI/tools parts (GPL) | **No** (unless commercial licence purchased where available). |
| **Network copyleft → TRAP** | JUCE under AGPLv3 [V], Essentia (AGPL-3.0) [V] | **No**. JUCE OK only under JUCE 9 EULA tier; Essentia only under paid MTG/UPF licence. |
| **Non-commercial → TRAP** | **madmom model/data files (CC BY-NC-SA 4.0)** [V] — including anything that loads madmom's bundled `.pkl/.npz/.h5` models | **No** for commercial use. Even a free app is risky if it supports a commercial product. Contact JKU for commercial licence if ever needed. |
| **Non-free / non-redistributable → TRAP** | fdk-aac (Fraunhofer FDK licence; patent rights not granted) [V NOTICE]; FFmpeg `--enable-nonfree` builds [V] | **No.** |
| **Attribution-only content licence** | BeatNet weights/code (CC BY 4.0) [V] | Licence-wise OK with attribution, **but** BeatNet requires madmom ≥0.16.1 [V] → drags in NC models risk + Python. Do not ship. |
| **Commercial, paid** | Rubber Band commercial (£590/£1,490/£9,320) [V], JUCE Indie/Pro [S], zplane élastique (royalty per unit, negotiated) [S], Bungee Pro (on request) [S], Qt commercial (e.g. Small Business ADE ≈ €546/yr) [S], Essentia commercial | Yes, per contract. |

### 2. JUCE 9 licensing (verified repo; tier prices secondary)

- Modules are dual-licensed **AGPLv3** or the **JUCE 9 EULA**; using JUCE not under AGPL means agreeing to the EULA + privacy policy + website ToS [V JUCE LICENSE.md].
- Tiers (unchanged from JUCE 8 per JUCE team announcement): **Starter** free, ≤ US$20k annual revenue/funding, **no splash screen**; **Indie** ≤ $300k, $40/mo or $800 perpetual per user; **Pro** unlimited, $175/mo (12-month minimum) or $3,500 perpetual per user [S forum.juce.com; official juce.com/get-juce page could not be fetched — **re-verify before purchase**].
- JUCE team "cannot provide compliance confirmations" — EULA questions → own counsel [V LICENSE.md].
- JUCE third-party components inventoried in `JUCE.spdx.json` [V]; include in our SBOM.
- JUCE examples are ISC [V].
- **Implication:** BeatFix closed-source + JUCE ⇒ must be under the EULA tier matching revenue/funding; AGPL path only if BeatFix is open source. **LEGAL REVIEW**: definition of "revenue/funding" for a pre-revenue project and seat counting for AI-assisted development.

### 3. Rubber Band (verified)

- GPL: source + right to modify/redistribute under GPL [V breakfastquay.com/rubberband].
- Commercial: **Standard £590** (must credit Breakfast Quay "prominently" in the app), **Non-attribution small publisher £1,490** (individuals/companies < 10 employees), **Non-attribution £9,320**; GBP ex-VAT; perpetual; no royalties; covers use/modify/redistribute in proprietary products; **no formal support agreement** [V breakfastquay.com/technology/license.html].
- **Implication:** keep `RubberBandEngine` behind a compile flag `BEATFIX_WITH_RUBBERBAND=OFF` by default; enabling it in a distributed build requires the purchased licence on file.

### 4. FFmpeg LGPL compliance checklist (if FFmpeg is ever shipped) [V ffmpeg.org/legal]

1. Build **without** `--enable-gpl` and **without** `--enable-nonfree`.
2. **Dynamic** linking (`avcodec-*.dll`, etc.); do not rename libraries (prefix/suffix OK).
3. Distribute/host the **exact corresponding FFmpeg source** (incl. our patches) alongside the binary on the same server; document the build configuration.
4. State FFmpeg/LGPL use in the EULA; **remove any reverse-engineering prohibition** that would apply to the LGPL parts.
5. Prominent attribution ("This software uses code of FFmpeg licensed under the LGPLv2.1…") in About and on the download page.
6. Patents are separate from copyright: decoders for patented formats (AAC, etc.) may still require patent licences — **LEGAL REVIEW**.

Which FFmpeg pieces are LGPL-safe for BeatFix: native decoders (pcm, flac, mp3/mp3float, vorbis, opus, alac, aac), native encoders (pcm, flac, alac), `libmp3lame` wrapper (LAME itself LGPL), `libopus`/`libvorbis` (BSD), swresample (LGPL). GPL-only items to avoid: `libx264/x265`, `libpostproc`, some filters (e.g. those marked GPL in `configure`), `--enable-libfdk-aac` (nonfree). **UNVERIFIED** per-component list against FFmpeg 8.1 `configure` — check `configure --list-*` + `LICENSE.md` at build time.

### 5. Generic LGPL (libsndfile, LAME, mpg123) obligations

- Ship as separate DLLs; app must still work if user replaces the DLL with a compatible modified build (do not statically link, do not hash-pin the DLL in a way that blocks replacement).
- Provide LGPL text + written offer / link to exact source used.
- EULA must permit modification of the work for the customer's own use and reverse engineering for debugging such modifications (LGPL-2.1 §6).
- Qt LGPLv3 additionally: installation information so relinked library can be run [V qt.io].

### 6. MPL-2.0 (Bungee, Symphonia)

- File-level copyleft: if we modify Bungee source files, those files (not our app) must be published under MPL-2.0. Unmodified vendoring → notice + inform recipients where to get the source.
- Larger Work may be proprietary. Keep a `third_party/bungee` pristine copy; put adapters in our own files.

### 7. Codec patents

| Codec | Status | Implication |
|---|---|---|
| **MP3 (MPEG-1/2 Layer III)** | Last US patent (6,009,399) expired 2017-04-16; Technicolor/Fraunhofer programme terminated 2017-04-23; EU patents expired earlier (2012 per press) [S multiple] | Encode/decode royalty-free. Ship LAME. Confidence High. |
| **AAC (LC/HE/HEv2/xHE)** | **Active** Via LA pool; licence needed by makers of end-user encoder/decoder products; $0.98/unit (1–500k), sliding to $0.10; $15k initial fee ($1k small entity < 15 staff and < $1M revenue); no fees for distributing AAC bitstreams; Via LA says no OS-vendor pass-through for Android OEMs [V via-la.com]. Baseline AAC patents expire ~2028, extensions ~2031 [S]. | **Do not ship our own AAC encoder/decoder in v1.** Using Windows Media Foundation's built-in AAC decoder/encoder may be covered by Microsoft's licence — **LEGAL REVIEW** (not confirmed by any source found). |
| **FLAC** | Royalty-free, BSD reference implementation [V] | Safe. |
| **Opus** | BSD; royalty-free patent licences from contributors (Xiph/IETF) [S, well known] | Safe (optional export). |
| **Vorbis** | BSD, royalty-free | Safe. |
| **WAV/RF64/AIFF PCM** | No codec patents | Safe. |
| **ALAC** | Apache-2.0 reference (Apple) **UNVERIFIED** this session | Not needed v1. |

### 8. Trademarks

| Mark | Owner | Risk | Rule for BeatFix |
|---|---|---|---|
| **FL Studio**, **NewTime** (styled "Newtime" in docs) | Image-Line (NewTime ships with FL Studio Producer+, since FL 20.6) [S image-line.com manual/news] | Using in product name, UI labels, icons or ads implying affiliation | Never use in product name, feature names or UI. Comparative/nominative text only ("for producers who use FL Studio"), no logos, with "FL Studio is a trademark of Image-Line" disclaimer. **LEGAL REVIEW** for any marketing comparison. Registration status of "NewTime" **UNVERIFIED** — treat as protected. |
| **"Warp" / "Warp Markers"** | Generic audio term; Ableton uses "Warp" heavily for Live's feature; registration of "Warp" in class 9 for audio **UNVERIFIED** (also Warp Records label) | Low–medium as descriptive term, higher as a product/feature brand | Use descriptive lower-case terms: "warp map", "beat markers", "tempo correction". Avoid branding a feature "Warp" or "Auto-Warp". Avoid Ableton's "Complex/Complex Pro" mode names. |
| **élastique** | zplane | Implying use of élastique | Never mention unless licensed. |
| **Suno** | Suno, Inc. | Implying partnership/endorsement | Nominative use only ("fixes tempo drift in AI-generated songs, e.g. from Suno"), no logo, no "Suno-certified". **LEGAL REVIEW** before using in marketing. |
| **Rubber Band** | Breakfast Quay | Standard commercial licence *requires* prominent credit | If £590 tier used, credit text per licence. |
| **"BeatFix"** (our name) | — | A single quick web search found no audio-software product/mark named "BeatFix" [S, low confidence] | **Not cleared.** Run USPTO/EUIPO/WIPO (class 9 + 42) and app-store searches before public launch. |

### 9. NewTime / FL Studio IP boundaries (functional reference only)

Allowed (per brief §28 and general practice):
- Studying **public** documentation, manuals, videos and the visible UX of NewTime/Ableton/Logic to understand *what* problems users solve and *which* workflows exist.
- Implementing **generic, published** techniques (phase vocoder, phase locking, transient phase reset, onset detection, DBN/Viterbi tempo tracking from papers, DTW) from academic sources.
- Matching **functional** behaviour expectations (e.g. "user can drag a beat marker and audio follows").

Not allowed:
- Decompiling, disassembling, debugging or memory-inspecting FL Studio/NewTime (or élastique, Ableton) binaries; network/file-format reverse engineering of proprietary project formats for compatibility without legal sign-off.
- Copying UI artwork, icons, layout pixel-for-pixel, colour schemes that create confusing similarity, or manual text.
- Copying preset names, mode names, or internal parameter names that are distinctive.
- Using leaked SDKs/source.
- Importing/exporting `.nwt`/FLP project data — **LEGAL REVIEW** (interoperability exceptions vary by jurisdiction).

### 10. ML model and training-data provenance

- Beat This! weights are MIT, but the README states some training files are "fully copyrighted or under limited Creative Commons licenses" and leaves the assessment to the user [V]. Risk: low-to-medium for inference-only use of released weights; **LEGAL REVIEW** recommended, and record model hash + source URL + licence in SBOM.
- Do **not** fine-tune on GTZAN/Ballroom/other research datasets with restrictive licences for a shipped model without clearance; use them only for evaluation (internal, not redistributed).
- madmom pretrained models are CC BY-NC-SA → never bundle, never derive weights from them.
- If a Beat This `--dbn` / madmom-style HMM post-processor is reimplemented: algorithm (Krebs/Böck bar-pointer model) is published in papers; the "madmom DBN patented" claim remains single-source/low-confidence (baseline) → **LEGAL REVIEW** before shipping; an own Viterbi tempo tracker from textbook HMM theory is lower risk.

### 11. User content

- BeatFix processes user-supplied audio locally; output ownership/licensing of AI-generated songs is the user's matter (Suno etc. ToS). App should not claim it grants rights. Add EULA clause: user responsible for rights in input audio. **LEGAL REVIEW** for EULA wording.

---

## Technical Details

### Licence policy for the repo (enforceable in CI)

```
ALLOW (vendored/static OK): MIT, MIT-0, BSD-2-Clause, BSD-3-Clause, ISC, Zlib, Apache-2.0, Unlicense, CC0-1.0, BSL-1.0
ALLOW (unmodified or publish modified files): MPL-2.0
ALLOW (dynamic-link DLL only, with source offer): LGPL-2.1-or-later, LGPL-3.0 (Qt only if chosen)
REQUIRE PURCHASED LICENCE ON FILE: JUCE (EULA tier), Rubber Band (commercial), zplane, Bungee Pro, Essentia commercial
DENY: GPL-*, AGPL-*, CC-BY-NC-*, CC-BY-SA for code, "nonfree" FFmpeg, fdk-aac, SSPL, unknown/no licence (e.g. StaffPad files)
```

Mechanics: SPDX SBOM generated at build (JUCE provides `JUCE.spdx.json`; add our own via a script listing `third_party/*/LICENSE*`); CI fails on DENY or unknown; DLL manifest checked against the allow-list; cargo-deny if any Rust is used (`[licenses] allow = [...]`).

### Attribution / notice obligations (what goes in the installer + About → Third-party notices)

| Component | Obligation |
|---|---|
| MIT/BSD/ISC/Apache libs | Copyright line + full licence text; Apache NOTICE contents if present. |
| ONNX Runtime | MIT text + its `ThirdPartyNotices.txt` (ships with release) — **UNVERIFIED** filename; check release package. |
| Beat This! | MIT text for code and weights; cite paper (courtesy). |
| Bungee / Symphonia | MPL-2.0 notice + where source is available. |
| LAME / libsndfile / mpg123 / FFmpeg | LGPL text, source offer/link to exact version, build instructions, EULA carve-outs, unrenamed DLLs. |
| Rubber Band (if commercial Standard) | Prominent credit per licence. |
| JUCE (EULA) | Per EULA (no splash required since JUCE 8 per forum [S]); third-party notices for JUCE-bundled libs (FLAC, Ogg, etc.) from `JUCE.spdx.json`. |
| BeatNet (not shipped) | n/a |

---

## Alternatives Considered

- **Open-sourcing BeatFix (GPL/AGPL)** would legally permit Rubber Band GPL, JUCE AGPL, aubio, Essentia AGPL — but madmom models stay NC regardless, and it removes the closed-source commercial option. Kept as a strategic option, not default.
- **Buying everything** (JUCE Indie + Rubber Band £1,490 + élastique) maximises quality but adds cost/royalty exposure before product-market fit. Deferred to paid-upgrade path.

## Evidence / Sources

- JUCE LICENSE.md (AGPLv3 / JUCE 9 EULA): https://github.com/juce-framework/JUCE/blob/master/LICENSE.md [V]
- JUCE 9 pricing unchanged: https://forum.juce.com/t/juce-9-no-pricing-or-eula-changes/69000 [S]; JUCE 8 tier table: https://forum.juce.com/t/important-changes-to-the-juce-end-user-licence-agreement-for-juce-8/61265 [S]
- Rubber Band: https://breakfastquay.com/rubberband/license.html , https://breakfastquay.com/technology/license.html [V]
- Audacity LICENSE.txt (default GPLv2+): https://github.com/audacity/audacity/blob/master/LICENSE.txt [V]
- madmom LICENSE (CC BY-NC-SA models): https://github.com/CPJKU/madmom/blob/main/LICENSE [V]
- BeatNet (CC BY 4.0, madmom dependency): https://github.com/mjhydri/BeatNet [V]
- Beat This! licence + data caveat: https://github.com/CPJKU/beat_this#license [V]
- FFmpeg legal: https://ffmpeg.org/legal.html [V]
- Qt LGPL obligations: https://www.qt.io/licensing/open-source-lgpl-obligations [V]; Qt Small Business: https://www.qt.io/pricing/qt-for-small-business [S]
- Via LA AAC: https://www.via-la.com/licensing-2/aac/ [V]; AAC expiry estimates: https://www.worlddab.org/resources/dab-plus-patent-information [S]
- MP3 patent expiry: https://i-programmer.info/news/181-algorithms/10750-mp3-free-at-last.html , https://www.digitalmusicnews.com/2017/05/12/the-death-of-mp3/ [S]
- fdk-aac NOTICE: https://github.com/mstorsjo/fdk-aac/blob/master/NOTICE [V]
- MF AAC encoder: https://learn.microsoft.com/en-us/windows/win32/medfound/aac-encoder [V]
- Image-Line NewTime manual/news: https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Newtime.htm , https://www.image-line.com/fl-studio-news/newtime-getting-started [S]
- zplane licensing: https://licensing.zplane.de/licensing [S]
- GitHub licence API + crates.io API snapshots (2026-10-08) for SPDX identifiers [V]

## Recommendation

1. Adopt the **licence policy** above as a CI gate from the first commit (High confidence).
2. Default build = **engineering-safe shortlist** (MIT/BSD/MPL/LGPL-DLL only); JUCE allowed only under a recorded EULA tier (Starter while ≤$20k). (High)
3. **No AAC encode/decode of our own in v1**; MP3 via LAME DLL; FLAC via libFLAC; WAV native. MF AAC paths only after legal sign-off. (High)
4. **Exclude**: Rubber Band GPL builds, StaffPad, aubio, Essentia, madmom models, BeatNet, fdk-aac, GPL/nonfree FFmpeg. (High)
5. Rubber Band commercial (£590 credited / £1,490 non-attribution if <10 staff) is the cheapest quality upgrade; buy only after A/B listening tests show it beats Signalsmith/Bungee on the QA corpus. (Medium)
6. Name/marketing: no "FL Studio", "NewTime", "Warp"-branded features, "élastique", or Suno logos; run formal clearance on "BeatFix". (High for avoidance; clearance outcome unknown)

## Risks

- JUCE price/tier data from forum, not the (unfetchable) official page → re-verify at purchase. (Med)
- LGPL compliance errors (static linking, missing source offer, EULA reverse-engineering clause) → licence termination. (Med likelihood, High impact) → checklist + release review.
- Transitive deps: beat_this_cpp, ONNX Runtime, JUCE bundle other third-party code → SBOM must be transitive.
- AAC via OS codec assumed safe without confirmation → keep out of v1.
- Trademark conflict on "BeatFix" undiscovered by a single search.

## Assumptions

- Distribution is a closed-source Windows installer, possibly sold; no source release planned.
- No proprietary project-file import/export (FLP/NewTime) in scope.
- Prices/tiers are as published on 2026-10-08; JUCE figures are forum-sourced.

## Open Questions (LEGAL REVIEW queue)

1. JUCE EULA: revenue/funding definition pre-launch; seat count; can engine code be JUCE-free to allow later shell swap without EULA implications?
2. Windows Media Foundation AAC decode/encode: does Microsoft's licence cover third-party apps calling the OS MFT for end users?
3. Beat This! training-data caveat — acceptable for commercial inference?
4. Reimplementing a madmom-style DBN beat tracker — any patent (low-confidence claim)?
5. "BeatFix" trademark clearance (class 9/42, US/EU/UK + app stores).
6. Permissible comparative marketing vs FL Studio NewTime / Ableton Warp.
7. EULA language for LGPL carve-outs and user-content responsibility.

## Implementation Implications

- Repo layout: `third_party/<lib>/{LICENSE,VERSION,SOURCE_URL}`; `licenses/policy.toml`; `tools/sbom` generates `THIRD_PARTY_NOTICES.txt` + SPDX JSON for installer.
- Build flags: `BEATFIX_WITH_RUBBERBAND` (OFF), `BEATFIX_WITH_FFMPEG` (OFF), `BEATFIX_WITH_MF_AAC` (OFF); each ON requires a licence/legal record referenced in CI.
- LGPL DLLs (LAME; optional libsndfile/FFmpeg) loaded via normal import or `LoadLibrary`, unrenamed, source tarballs published with each release.
- UI copy review checklist: no third-party marks except nominative; feature names generic ("Tempo Correction", "Beat Markers", "Warp Map" lower-case descriptive only).
