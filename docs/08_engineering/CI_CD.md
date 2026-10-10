# CI / CD

Status: 2026-10-08, v1.0. GitHub Actions on `windows-latest`. Test tiers and corpus rules come from [QA_STRATEGY.md](../07_quality/QA_STRATEGY.md) §5 and [BENCHMARK_CORPUS.md](../07_quality/BENCHMARK_CORPUS.md); build details from [BUILD_AND_DEPENDENCIES.md](BUILD_AND_DEPENDENCIES.md); release steps from [RELEASE_STRATEGY.md](RELEASE_STRATEGY.md).

## 1. Principles
- CI uses only **corpus Tiers S, M and C** (synthetic, CC BY 4.0 MIDI renders, CC BY/CC0 tracks). Tier P (research datasets) and Tier U (user Suno tracks) never enter CI: `bench.py` refuses items whose `allowed_use` lacks `"ci"` when `CI=true` ([BENCHMARK_CORPUS.md](../07_quality/BENCHMARK_CORPUS.md) §4).
- Shared runners are noisy: **performance numbers in CI are trend-only, never gating** ([PERFORMANCE_BENCHMARKS.md](../07_quality/PERFORMANCE_BENCHMARKS.md) §1). Gating perf runs happen on the reference laptop before release.
- Everything that can fail fast runs on every PR (≤ 10 min target). Expensive suites run nightly and before merging DSP-touching PRs.
- Secrets (signing) exist only in the `release` environment, which requires manual approval.
- `windows-latest` currently maps to Windows Server 2025 + VS 2022 (**verify on runner**). The compiler-version guard ([BUILD_AND_DEPENDENCIES.md](BUILD_AND_DEPENDENCIES.md) §1) catches silent image changes.

## 2. Jobs

```
            PR / push to main                         nightly (02:00 UTC) + PRs labelled dsp|deps
 ┌──────────────┬──────────────┬─────────────┐     ┌─────────────┬──────────────┬─────────────┬──────────────┐
 │ build+unit   │ dsp-short    │ licence     │     │ asan(+fuzz) │ dsp-full     │ golden      │ perf-smoke   │
 │ format/tidy  │ (≤ 30 s sig.)│ (policy)    │     │             │ (S, 44.1/48/ │ (S/M/C)     │ (trend only) │
 │ D1 smoke     │              │             │     │             │  96 kHz)     │ + bench S/M/C│              │
 └──────────────┴──────────────┴─────────────┘     └─────────────┴──────────────┴─────────────┴──────────────┘
                                   tag v*  →  release.yml (§5)
```

| Job | Trigger | Preset | Does | Gate? | Time budget |
|---|---|---|---|---|---|
| `build-unit` | PR, push | `ci` (RelWithDebInfo, `/WX`) | configure, build all targets, `ctest -L unit`, clang-format check on changed files, clang-tidy on changed files, **D1 smoke** (render S16 twice with `bftool`, SHA-256 equal) | yes | ≤ 8 min warm cache |
| `dsp-short` | PR, push | `ci` | `ctest -L dsp` subset: DSP-DEC-1/2, DSP-RES-1, DSP-STR-1 (ratios 0.95/1.05), DSP-MAP-1, DSP-EXP-1 on signals ≤ 30 s | yes | ≤ 5 min |
| `licence` | PR, push | none (Python) | `tools/licence_check.py`: every `third_party/manifest.json` entry has an SPDX id on the ALLOW list; no DENY/unknown; `dumpbin /dependents` of built binaries vs DLL allow-list; asserts Rubber Band is not linked in the `ci`/`release` builds | yes | ≤ 1 min |
| `asan` | nightly, `dsp`/`deps` label | `asan` | unit + DSP tests under ASan; libFuzzer targets (MP3 decode, WAV/AIFF/FLAC headers, project JSON) 30 min total, corpus cached | yes (nightly red = fix next day) | ≤ 60 min |
| `dsp-full` | nightly, `dsp` label | `ci` | all DSP-* tests at 44.1/48/96 kHz, full ratio set, DSP-REN-1 null test (1/2/4/8 threads), DSP-REN-2 cancel storm, **D2** checks (thread counts 1/4/8) | yes | ≤ 40 min |
| `golden` | nightly, `dsp` label; **required before merging any PR touching `bf_dsp`, `bf_analysis`, `bf_io`** | `ci` | GOLD-01…12 via `bftool` + `bench/eval`; metric diff vs `tests/golden`; then `bench.py` on Tiers S/M/C with bootstrap CIs; uploads diff report, spectrogram PNGs, null-residual audio as artifacts | yes | ≤ 45 min |
| `perf-smoke` | nightly | `release` | `bftool --profile` analysis + render of a fixed 4-min S16 item and a 20-min S01 item; writes `perf.json`; compares with the rolling median of the last 7 nights | **no**; alert at > 25 % regression | ≤ 15 min |

Labels: `ctest` labels `unit`, `dsp`, `golden`, `slow` map to the Catch2 tags in [REPOSITORY_STRUCTURE.md](REPOSITORY_STRUCTURE.md) §4.

### 2.1 Caching
- `actions/cache` keyed on `hashFiles('cmake/Deps.cmake','vcpkg.json')` for the FetchContent download dir and the vcpkg binary cache. A changed pin invalidates the cache by construction.
- The model file is cached under the key `models/manifest.json` hash.
- Tier M renders and Tier C audio are cached under their manifest hashes. Tier C audio is fetched from the private release-asset bucket by SHA-256 ([REPOSITORY_STRUCTURE.md](REPOSITORY_STRUCTURE.md) §6).

### 2.2 Skeleton

```yaml
# .github/workflows/ci.yml (abridged)
on: [pull_request, push]
jobs:
  build-unit:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ilammy/msvc-dev-cmd@v1          # vcvars64 for Ninja; action name/version: verify at setup
      - uses: actions/cache@v4
        with: { path: "build/_deps\n${{ runner.temp }}/vcpkg-bin",
                key: deps-${{ hashFiles('cmake/Deps.cmake','vcpkg.json','models/manifest.json') }} }
      - run: cmake --preset ci
        env: { VCPKG_BINARY_SOURCES: "clear;files,${{ runner.temp }}/vcpkg-bin,readwrite" }
      - run: cmake --build --preset ci
      - run: ctest --preset ci -L unit
      - run: python tools/null_test.py --d1-smoke   # render twice, compare hashes
  licence:
    runs-on: windows-latest
    steps: [ { uses: actions/checkout@v4 }, { run: python tools/licence_check.py --manifest-only } ]
```
The licence job runs `--manifest-only` on PRs (fast, no build) and the full binary check inside `build-unit` after the build.

## 3. Branch protection
- `main` requires: `build-unit`, `dsp-short`, `licence` green; one review (owner self-review allowed while solo, but the PR template checklist is mandatory, [CODING_STANDARDS.md](CODING_STANDARDS.md) §11).
- PRs touching `src/bf_dsp`, `src/bf_analysis`, `src/bf_io` additionally require `golden` (CODEOWNERS path rule + required check on label `dsp`, auto-applied by a path filter).
- Golden re-baselining is a separate commit produced by `bftool … --update-goldens` on a clean tree, with a changelog entry: cause, metric deltas, listening confirmation if the change exceeds D2 tolerance ([DSP_TEST_STRATEGY.md](../07_quality/DSP_TEST_STRATEGY.md) §5).

## 4. Artifacts
| Artifact | Produced by | Retention | Signed? |
|---|---|---|---|
| Unsigned dev build zip (`beatfix-<sha>.zip`) | `build-unit` on `main` | 14 days | no (internal only, never distributed) |
| Golden/bench reports (JSON, PNG, residual FLAC) | `golden` | 30 days | — |
| `perf.json` history | `perf-smoke` | 90 days | — |
| Fuzz crash inputs | `asan` | 90 days | — |
| Release binaries, installer, zip, SBOM, notices, `SHA256SUMS` | `release.yml` | permanent (GitHub release) | yes |
| PDBs | `release.yml` | permanent, **private** artifact store (never public) | — |
| LGPL source bundle (LAME source + vcpkg port patches) | `release.yml` | permanent, published next to the binaries | — |

## 5. Release pipeline (`release.yml`)

Trigger: push of tag `vX.Y.Z` or `vX.Y.Z-beta.N` on `main`. Runs in the `release` GitHub environment (manual approval; signing secrets scoped to it).

```
 1. checkout tag; verify tag == version in CMakeLists/vcpkg.json
 2. full gate: build-unit + dsp-full + golden + licence (re-run, not reused)
 3. build `release` preset twice in the fixed path → diff unsigned hashes (warn on mismatch, BUILD §8)
 4. tools/make_notices.py → THIRD_PARTY_NOTICES.txt, sbom.spdx.json (incl. JUCE.spdx.json, model hash)
 5. sign: BeatFix.exe, <lame>.dll (ours to sign), NOT onnxruntime.dll if already Microsoft-signed
         → signtool /fd sha256 /tr <RFC 3161 TSA> /td sha256
 6. Inno Setup → BeatFix-X.Y.Z-setup.exe ; sign installer (same flags)
 7. portable zip (signed binaries + models + notices) → BeatFix-X.Y.Z-portable.zip
 8. SHA256SUMS over installer + zip; upload PDBs to private store; LGPL source bundle
 9. create DRAFT GitHub release with release notes from CHANGELOG.md
10. human: smoke-test the signed installer on a clean Windows 11 VM, then publish
```

Signing mechanics (details and certificate realities in [RELEASE_STRATEGY.md](RELEASE_STRATEGY.md) §4):
- **Preferred:** Microsoft Artifact Signing (ex-Trusted Signing) via its GitHub Action, if the owner's entity is eligible (organisations in listed countries; individuals only in US/Canada [V]). No private key ever on the runner.
- **Otherwise:** OV certificate on a cloud HSM, accessed via the vendor's KSP/CLI with credentials in the `release` environment.
- Every signature is RFC 3161 timestamped so binaries stay valid after the (≤ 460-day) certificate expires.
- Signing verification step: `signtool verify /pa /v` on every signed file; failure aborts.

## 6. What CI does not do (MVP)
- No auto-publish to users and no update feed (the app has no updater, [RELEASE_STRATEGY.md](RELEASE_STRATEGY.md) §5).
- No macOS/Linux jobs (out of scope, DECISION_SUMMARY §4). The `bf_dsp` unit tests are portable C++, so a Linux job (e.g. for Clang RealtimeSanitizer) can be added later at low cost.
- No listening tests (L7) or reference-laptop perf gating (L5): these are release-candidate manual steps ([QA_STRATEGY.md](../07_quality/QA_STRATEGY.md) §5).
- No Tier P/U data, ever.

## 7. Open questions
1. Runner minutes: `golden` + `dsp-full` nightly on Windows runners may exceed the free tier for a private repo; measure in Phase 1 and trim the nightly matrix (e.g. 96 kHz weekly) if needed.
2. Tier C audio storage location (shared with REPOSITORY_STRUCTURE §6).
3. Whether a self-hosted runner on the reference laptop is worth it for gating perf (trade-off: security of a self-hosted runner on a personal machine).
