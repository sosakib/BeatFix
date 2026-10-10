# Release Strategy

Status: 2026-10-08, v1.0. Sources: [ARCHITECTURE_RESEARCH.md](../03_research/ARCHITECTURE_RESEARCH.md) F11–F13, §14; [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) §Attribution; [DECISION_SUMMARY.md](../10_implementation/DECISION_SUMMARY.md) §1 (Packaging row). Decision record: [ADR-013](../09_decisions/ADR-013-build-packaging-distribution.md). Pipeline mechanics: [CI_CD.md](CI_CD.md) §5. Not legal advice; items marked **for legal review** need counsel.

## 1. Versioning

| Version | Format | Bumped when | Stored in |
|---|---|---|---|
| App | SemVer `MAJOR.MINOR.PATCH[-beta.N]`, `0.y.z` until the first public stable | MAJOR: project files from this version can't be opened by the previous MAJOR, or a breaking UX change. MINOR: features. PATCH: fixes only | git tag `vX.Y.Z`, CMake, Windows VERSIONINFO, About box |
| Project `schemaVersion` | integer | breaking project-format change (migration chain added) | `.beatfix` ([ADR-010](../09_decisions/ADR-010-project-storage.md)) |
| Warp `algoVersion` | semver | `buildWarp` output may change for the same inputs | warp map ([WARP_MAP.md](../04_algorithms/WARP_MAP.md)) |
| Analysis model id | `beatthis-final0-onnx-op<N>@<sha256-8>` | model or export changes | analysis record |
| Stretch engine id | `signalsmith-1.4.0`, `resample-r8brain-6.5` | engine version or preset change | render cache key, project |
| `renderFormatVersion` | integer | cache file layout change | cache headers |

Rule from [DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) §6 (D3): when a project made with older engine/model/algo versions is reopened, the app recomputes derived data and tells the user that output may differ. It never changes output silently. Release notes list every change to these component versions.

## 2. Channels

| Channel | Audience | Build | Signed | Distribution |
|---|---|---|---|---|
| dev | owner, CI | every `main` push | no | CI artifact, 14 days, never shared |
| beta | invited testers | tag `vX.Y.Z-beta.N` | **yes** | GitHub release marked pre-release (or private link) |
| stable | public | tag `vX.Y.Z` after RC sign-off | **yes** | website download + GitHub release |
| store (Phase 2) | public | same stable build, MSIX-packaged | by Microsoft | Microsoft Store |

Stable release gate (all must hold): CI full gate green; golden + corpus benchmarks within tolerance; reference-laptop perf within targets ([PERFORMANCE_BENCHMARKS.md](../07_quality/PERFORMANCE_BENCHMARKS.md) §6); listening sign-off for any engine/mode change since the last release ([SUBJECTIVE_LISTENING_TESTS.md](../07_quality/SUBJECTIVE_LISTENING_TESTS.md)); acceptance matrix passed; licence/notice review done; clean-VM install/uninstall smoke test.

## 3. Packages (MVP)

### 3.1 Inno Setup per-user installer
- Installs to `%LOCALAPPDATA%\Programs\BeatFix` with `PrivilegesRequired=lowest`: **no admin prompt**, no UAC, no machine-wide writes. Reason: consumer app; admin prompts reduce trust and fail on locked-down PCs.
- Contents: `BeatFix.exe`, `onnxruntime.dll`, the LAME DLL, app-local VC++ runtime DLLs, `models\`, `THIRD_PARTY_NOTICES.txt`, `LICENSE-LGPL.txt`, `sbom.spdx.json`, EULA.
- Registers per-user file association `.beatfix` (HKCU) and Start-menu shortcut. Uninstaller removes program files; it **asks** before removing user data in `%LOCALAPPDATA%\BeatFix\` (cache, autosave, logs). Default: keep autosaves, delete cache.
- Shows the EULA (owner-supplied, **for legal review**: LGPL carve-outs and user-content responsibility, [LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) §5, §11).
- Inno Setup: free; since 6.5 commercial users are *asked* (not required) to buy a licence [V]. Recommendation: buy it once the product earns money.
- **Why not WiX/MSI:** WiX v6+ requires the Open Source Maintenance Fee for organisations above US$10k revenue [V], MSI authoring overhead, and nothing an MSI adds is needed for a per-user consumer app.

### 3.2 Portable zip
Same signed files, no installer, no registry writes. Detects portability by a `portable.flag` file next to the exe: then data goes to `.\BeatFixData\` instead of `%LOCALAPPDATA%`. For users who can't or won't run installers (studios, locked machines).

### 3.3 MSIX via Microsoft Store: **Phase 2**
- Store registration is free for individuals (since 2025-09) and companies (since 2026-05); Store MSIX packages are **signed and hosted by Microsoft for free**, with Store-managed updates [V]. This removes signing cost and SmartScreen friction for that channel.
- Caveats: `%LOCALAPPDATA%` writes are virtualised into the package store (cache path differs on disk → document for support); uninstall removes caches; `.beatfix` association via the manifest.
- Not in MVP because the first releases need fast iteration outside Store certification, and the portable/installer path must exist anyway.

## 4. Code signing realities

| Fact | Consequence |
|---|---|
| Since 2024, **EV certificates no longer give instant SmartScreen reputation**; EV and OV both build reputation by download volume [V Med-High] | Expect "Windows protected your PC" warnings for the first releases regardless of certificate type. Don't pay the EV premium for SmartScreen |
| Since **2026-03-01** (CA/B Forum CSC-31), new code-signing certificates are valid **≤ 460 days** [V High] | Budget a renewal roughly every 15 months; calendar reminder 60 days before expiry; reputation is tied to the publisher identity, so renewals under the same name keep it better than a new name |
| **Microsoft Artifact Signing** (ex-Trusted Signing): cloud signing, "from $9.99/mo" (pricing page did not render, **Med**); orgs in US/CA/EU/UK/AU/NZ/JP/KR/SG/CH/NO/IL; **individuals only in US/Canada**; no EV [V] | Preferred if the owner's entity is eligible: no key handling, CI-native |
| OV certificate keys must be on hardware (HSM/token) | Use a cloud-HSM-backed OV certificate so CI can sign without a USB token; price **UNVERIFIED** |
| RFC 3161 timestamps keep signatures valid after cert expiry | Timestamp every signature; never ship an untimestamped binary |

What gets signed: `BeatFix.exe`, the LAME DLL, the installer, and the exe inside the zip. `onnxruntime.dll` is shipped as Microsoft signed it (verify on pin; if it is unsigned, we sign it). Mitigations for SmartScreen: consistent publisher name; publish `SHA256SUMS`; Store channel in Phase 2; ask beta testers to report the warning text so the website FAQ can show it.

The signing entity and country are an **owner decision** (DECISION_SUMMARY §8) and determine Artifact Signing eligibility.

## 5. Updates: none in the MVP app

Decision: **no auto-updater and no update check in the app.** The About dialog shows the version and a "Visit website" link; clicking it opens the default browser (a user action; the app itself makes no network request).

Why:
1. DECISION_SUMMARY §1: "The app makes **no network calls**." Offline-first is a product promise (brief §39) and a privacy promise (no telemetry, DECISION_SUMMARY §4). Even an update ping reveals IP, version and usage time.
2. An updater is a security surface: signature verification, rollback, partial downloads, elevation. Getting it wrong is worse than not having it.
3. The Phase 2 Store channel gives free, signed, managed updates to users who want them.
4. Projects store engine/model/algo versions, so a user on an old version loses nothing by not updating.

Cost: users on the direct-download channel update manually and may run stale versions. Accepted for MVP. Post-MVP option: an **opt-in** update check (off by default, explicit consent, no identifiers beyond the version) using Velopack (MIT) or a signed JSON feed; requires its own ADR.

## 6. Attribution, NOTICE and licence obligations

Generated by `tools/make_notices.py` from `third_party/manifest.json`, `third_party/licenses/`, `JUCE.spdx.json` and `models/manifest.json`. Shipped in the installer, the zip, and shown in **Help → About → Third-party notices**.

| Component | Obligation in the release |
|---|---|
| MIT / BSD / ISC / BSL / Zlib (Signalsmith, r8brain, libebur128, nlohmann/json, ONNX Runtime, libFLAC/libvorbis via JUCE) | copyright line + full licence text in `THIRD_PARTY_NOTICES.txt` |
| CC0 (minimp3) | none required; credited as courtesy |
| ONNX Runtime | MIT text + the third-party notices file shipped in its release package (file name **UNVERIFIED**, check the zip) |
| Beat This! weights | MIT text; paper citation (courtesy); model hash + source in SBOM; training-data caveat **for legal review** |
| JUCE (EULA) | per EULA (no splash screen since JUCE 8 [S]); notices for JUCE-bundled third-party code from `JUCE.spdx.json` |
| **LAME (LGPL)** | `LICENSE-LGPL.txt`; DLL unrenamed and user-replaceable (delay-loaded, never hash-pinned); exact corresponding source + vcpkg patches + build notes published with each release (written offer not needed if source is offered alongside the binary); EULA must allow modification for own use and reverse engineering to debug such modifications (LGPL-2.1 §6) — **for legal review** |
| Bungee (MPL-2.0), only if ever enabled | MPL notice + where to get the source; our adapters stay in our own files |
| Rubber Band | never in a release under GPL. If a commercial licence is bought: £590 tier requires a prominent credit in the app |

SBOM: `sbom.spdx.json` (SPDX 2.3 JSON) listing every shipped component with version, SPDX id, hash; transitive JUCE components included from `JUCE.spdx.json`.

Trademark hygiene for release notes, website and UI: no "FL Studio", "NewTime", "élastique" or Suno logos; feature names descriptive and lower-case ("warp map", "tempo correction"); "BeatFix" itself is **not cleared** (trademark search before public launch, **for legal review**) ([LICENSING_RESEARCH.md](../03_research/LICENSING_RESEARCH.md) §8).

## 7. Release checklist (stable)
1. Version bumped; CHANGELOG lists user-visible changes and every component-version change (§1).
2. Release CI green; draft release created.
3. Reference-laptop perf, listening sign-off, acceptance matrix: recorded in the RC notes.
4. Notices/SBOM diffed against the previous release; any new licence reviewed.
5. Signed installer + zip tested on a clean Windows 10 22H2 and Windows 11 VM: install without admin, open sample, analyse, render, export WAV/FLAC/MP3, uninstall.
6. `signtool verify /pa` on all signed files; `SHA256SUMS` matches.
7. Publish; LGPL source bundle published at the same location.

## 8. Open questions
1. Signing entity and country (owner) → Artifact Signing vs OV cloud HSM.
2. Minimum Windows version: Windows 10 22H2 + Windows 11 proposed; Windows 10 reached end of standard support in Oct 2025, so supporting it is a goodwill choice (owner).
3. EULA text (counsel).
4. Beta distribution: public pre-release vs private link.
