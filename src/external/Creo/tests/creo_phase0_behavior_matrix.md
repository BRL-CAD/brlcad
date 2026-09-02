# Creo Phase 0 Behavior Matrix

This matrix records the current v2 snapshot boundary and the evidence required
before each behavior moves out of the diagnostic legacy converter. V2 is the
forward Phase 0 contract. The core's v1 single-part reader remains a
transitional baseline only: Phase 0 adds no v1 fixture coverage and no new v1
behavior.

## Current Boundary Coverage

| Behavior | Current source and v2 field | Current conversion effect | Required evidence |
| --- | --- | --- | --- |
| Snapshot identity and layout | `conversion_snapshot.h`; header magic, version, sizes, payload type, and bounded ranges | Accepts only a well-formed v2 scene | `creo_snapshot_core_v2` valid fixture plus malformed and unsupported requests |
| Output path | `snapshot_writer.cpp`; `settings.output_path` | Creates or updates the requested `.g` file | `creo_snapshot_core_v2` manifest |
| Root part hierarchy | `snapshot_writer.cpp`; scene root part | Writes `all` containing the part region | `creo_snapshot_core_v2` manifest |
| Mesh topology | `snapshot_writer.cpp`; part vertices and triangles | Writes a BoT solid | `creo_snapshot_core_v2` manifest |
| Model name and version | `snapshot_writer.cpp`; part name and version | Names output and writes `ptc_name` and `ptc_version_stamp` | `creo_snapshot_core_v2` manifest |
| Material name | `snapshot_writer.cpp`; part material name | Writes `ptc_material_name` on the region | `creo_snapshot_core_v2` manifest |
| Color and region ID | `snapshot_writer.cpp`; part color and region ID | Writes region color and ID | `creo_snapshot_core_v2` manifest |
| Invalid request path | `conversion.cpp`; `creo_brl_core_convert` | Rejects null or empty request paths | `creo_snapshot_core_v2` |

## Planned Coverage

| Behavior | Current source and v2 field | Current conversion effect | Planned evidence |
| --- | --- | --- | --- |
| Coordinate transform | `snapshot_writer.cpp`; `settings.xform_mode` | Transforms the top-level `all` membership | v2 fixture for none, X-to-Z, and Y-to-Z transforms |
| Assembly graph and transforms | `snapshot_writer.cpp`; scene assemblies and members | Writes combinations in dependency order and preserves member matrices | Phase 6 v2 assembly fixture manifest |
| Multiple parts and name collisions | `conversion.cpp`; name normalization and unique-name allocation | Adds deterministic suffixes for collisions | v2 multi-part collision fixture manifest |
| Tessellation normals | `snapshot_writer.cpp`; write-normals setting and part normal ranges | Writes a BoT with normals | v2 mesh-with-normals fixture manifest |
| Tessellation capture | `snapshot_writer.cpp`; max chord and min angle | Controls Creo-side tessellation before the snapshot is written | Optional Creo smoke test plus captured v2 fixture replay |
| Bounding-box fallback | `snapshot_writer.cpp`; part bounding box and fallback flags | Writes an RPP and `tess_fail` attribute when mesh data is absent | v2 fallback fixture manifest |
| Part mass properties | `snapshot_writer.cpp`; density, mass, and volume | Captured but not written by the current core | Phase 4 parity fixture before implementing or relocating output policy |
| Captured part attributes | `snapshot_writer.cpp`; part attribute ranges | Validated but not written by the current core | Phase 4 parity fixture before implementing or relocating output policy |
| Captured part parameters | `snapshot_writer.cpp`; part parameter ranges | Validated but not written by the current core | Phase 4 parity fixture before implementing or relocating output policy |
| Profile material file | `snapshot_writer.cpp`; `settings.material_path` | Captured but not consumed by snapshot conversion | Phase 2 profile mapping and replay test |
| Log path and log type | `snapshot_writer.cpp`; `settings.log_path` and `settings.log_type` | Captured but not consumed by snapshot conversion | Phase 2 diagnostic/result test |
| Minimum luminance | `profile_controls.h`; `minimum_luminance` to `settings.min_luminance` | Captured but not consumed by snapshot conversion | Phase 2 profile mapping and replay test |
| Parameter rename and preserve controls | `snapshot_writer.cpp`; settings string ranges | Captured but not consumed by snapshot conversion | Phase 2 mapping test and Phase 4 output fixture |
| Chord mode, max angle, max facets, max steps, and min edge | `snapshot_writer.cpp`; snapshot settings | Captured or defaulted; only max chord and min angle currently affect Creo-side tessellation | Phase 2 option mapping and optional Creo smoke evidence |
| Small-feature controls | `snapshot_writer.cpp`; eliminate-small-features flag and minimum hole, chamfer, and round values | Captured but not consumed by snapshot conversion | Phase 2 option mapping and Phase 5 parity fixture |
| Facets-only, STL export, solidity, and box controls | `snapshot_writer.cpp`; settings flags | Captured but not consumed by snapshot conversion | Phase 2 option mapping and corresponding behavior fixture |
| CSG primitives and boolean structure | `csg.cpp`; absent from the snapshot | Not representable by the current request | Phase 5 request extension and legacy parity fixture |
| Legacy launch and failure behavior | `main.cpp`, `part.cpp`, and `assembly.cpp` | Diagnostic reference only; not a production dependency | Optional Creo smoke test; never a normal repository-test requirement |

## Phase 0 Exit Evidence

- `creo_snapshot_core_v2` uses a synthetic v2 scene fixture, passes it through
  `creo_brl_core_convert`, and compares a normalized semantic `.g` manifest.
- The same test rejects malformed, unsupported-version, and invalid-request
  inputs without launching Creo.
- The rows above identify every current setting and conversion data category,
  including data that the v2 core does not yet consume.
