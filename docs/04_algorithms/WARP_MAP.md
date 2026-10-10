# Smart Warp Map: Data Model and Serialisation

## Objective
Define the machine-readable warp map (brief §10). It covers what is stored, what is derived, the JSON schema, and the versioning rules.

## Why It Matters
The warp map is the contract between the analysis, the UI, the renderer and the cache key. If it is ambiguous, renders stop being reproducible and the A/B comparison stops being trustworthy.

## Key decision: inputs vs derived data
To avoid two sources of truth, the warp map is **derived** and the project stores only **inputs**:

```
Inputs (persisted in project, PROJECT_SESSION_MODEL.md)       Derived (recomputed, cached, exportable)
──────────────────────────────────────────────────────        ───────────────────────────────────────
source fingerprint + analysis result (raw beats, conf,         WarpMap = buildWarp(inputs)   (WARP_FUNCTION.md §9)
  downbeats, regions, change points, analyzer version)          - refined/smoothed anchors x_i
user edits (beat level ×2/÷2, downbeat shift,                   - target times T_i, ratios r_i
  manual anchors [post-MVP], change-point overrides)            - per-segment mode, region ids
target BPM, phase policy, Natural↔Strict N_c,                   - correction class for display
  stretch mode / Auto config, engine id + version
```
`buildWarp` is deterministic: same inputs and same `algoVersion` give a bit-identical WarpMap. The serialised WarpMap carries an `inputsHash`. On load, a stored WarpMap is used only if `inputsHash` and `algoVersion` match, and otherwise it is recomputed.

## Units and precision
- Times: **seconds as IEEE-754 float64**, serialised with shortest round-trip formatting (`std::to_chars` / Ryu). Sample-frame conversion happens only in the renderer, using the source sample rate.
- Ratios: float64, dimensionless, output/input (WARP_FUNCTION.md §1).
- BPM: float64.
- No float32 anywhere in the model.

## JSON schema (v1)

```json
{
  "format": "beatfix.warpmap",
  "schemaVersion": 1,
  "algoVersion": "warp-1.0.0",
  "inputsHash": "sha256:…",
  "source": {
    "fingerprint": "sha256-pcm:…",
    "sampleRate": 48000,
    "channels": 2,
    "durationSec": 201.437
  },
  "target": {
    "bpm": 110.0,
    "beatsPerBar": 4,
    "phasePolicy": "preserveStart",
    "phaseSec": 0.512,
    "leadingSilenceSec": 0.0
  },
  "correction": { "mode": "natural", "cutoffBeats": 8, "lambda": 2.914 },
  "anchors": [
    {
      "i": 0,
      "beatIndex": 0,
      "barIndex": 0,
      "beatInBar": 0,
      "isDownbeat": true,
      "srcDetectedSec": 0.5121,
      "srcAnchorSec": 0.5120,
      "tgtSec": 0.5120,
      "confidence": 0.94,
      "inferred": false,
      "transientStrength": 0.81,
      "localBpm": 108.3
    }
  ],
  "segments": [
    {
      "seg": 0,
      "fromAnchor": 0, "toAnchor": 1,
      "srcStartSec": 0.5120, "srcEndSec": 1.0662,
      "tgtStartSec": 0.5120, "tgtEndSec": 1.0575,
      "ratio": 0.98431,
      "correctionClass": 2,
      "mode": "smooth",
      "regionId": "r0"
    }
  ],
  "regions": [
    { "id": "r0", "label": "Intro", "srcStartSec": 0.0, "srcEndSec": 14.2,
      "class": "sparse", "mode": "smooth", "modeSource": "auto", "confidence": 0.71 }
  ],
  "changePoints": [ { "atAnchor": 128, "kind": "detected", "honoured": true } ],
  "edges": { "preRatio": 0.98431, "postRatio": 1.0021 },
  "diagnostics": { "maxAbsShiftSec": 0.183, "ratioMin": 0.972, "ratioMax": 1.031,
                   "warnSegments": [], "residualScaleSec": 0.006 }
}
```

### Field notes
| Field | Source | Notes |
|---|---|---|
| `srcDetectedSec` | beat tracker (refined) | Kept to draw "detected vs used" on screen |
| `srcAnchorSec` | Natural/Strict optimiser `x_i` | What W actually uses |
| `localBpm` | `60 / (x_{i+1} − x_i)` | Display. Smoothed-tempo-curve value |
| `transientStrength` | onset salience at anchor (0–1) | Used by Auto mode and display |
| `inferred` | outlier repair | true means the beat was inserted at a gap. It gets low weight. |
| `correctionClass` | `|log2(r/g)|` thresholds, g = global ratio B_detected/B_target (WAVEFORM_VIEW.md) | 0 = unchanged … 3 = strong. A deliberate global tempo change does not colour the whole song. Display only. |
| `mode` | segment stretch mode after Auto resolution | `resample`, `smooth`, `transient`, `slice`, `texture` (names in TIME_STRETCH_ENGINE.md). MVP has `smooth` (default) and `resample` (constant-ratio sections only). `transient` is Phase 1.5. `slice`/`texture` are future. |
| `regions[].modeSource` | `auto` or `user` | For future per-region overrides |

## Versioning and backward compatibility
- `schemaVersion` is an integer. It is bumped only on **breaking** changes (removed or renamed field, changed unit). Additive fields don't bump it.
- Readers **must ignore unknown fields** and **must preserve them** when round-tripping a project file (forward-compatibility for older app builds opening newer files: open read-only and warn when `schemaVersion` > supported).
- Migrations: `migrate_v1_to_v2(json)` are pure functions. Golden fixture files for each historical version live in `tests/fixtures/warpmap/vN/`.
- `algoVersion` (semver) changes whenever `buildWarp` output could change for the same inputs. Old projects keep their stored inputs, so re-opening with a new `algoVersion` triggers a recompute and a **visible notice** ("Correction recalculated with a newer algorithm. Re-render to compare."). It never changes the output silently.

## Interchange (post-MVP)
Export adapters can be generated from the derived map: a MIDI tempo map (tempo events from `localBpm`), a CSV of anchors, and DAW warp-marker formats where documented. The JSON stays the canonical format.

## Validation (`WarpMap::validate`)
Monotonic anchors, ratio bounds and segment contiguity (`seg[k].srcEnd == seg[k+1].srcStart`, exact float64 equality because both come from the same anchor) are checked. Every `isDownbeat` must sit at `beatInBar == 0`, and the totals must match: `Σ(tgtEnd − tgtStart) == tgt span`, within 1e-9 s.

## Implementation Implications
- One struct, one serialiser and one validator. No inheritance.
- Fixture tests cover round-trip byte stability (serialise → parse → serialise is identical), unknown-field preservation, and v1 golden files.
