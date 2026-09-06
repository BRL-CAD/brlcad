# IGES converter design

Both command-line programs are C++17 applications. Their interfaces to BRL-CAD
remain the existing library APIs. NMG remains the representation for faceted
export, polygonal import, and drawing-wire output.

## Import

`iges_document` owns the immutable physical-file representation, validated
directory entries, typed parameter values, and parser diagnostics. There is
one parser, including for native CSG and compatibility modes. Entity numbers
and positional parameter indices in the translators follow the IGES 5.3
entity definitions.

`iges_brep_import` reconstructs geometry and completes the source hierarchy
after all geometry, recovery, and native CSG work is finished. `iges_native`
translates CSG and sweep profiles. Its BRep callback uses the same output policy
as other imported BReps. Shared instances are resolved after their definitions;
source placements are applied once. Root reconciliation happens against the
completed database, avoiding duplicate unplaced definitions.

`iges_import` handles annotations, datums, and semantic drawing groups. Nested
subfigure definitions and instances are ordered by their dependencies.
`iges_wire` adaptively samples supported model-space curves into bounded NMG
wire geometry. A default file containing only drawings or datums is imported
without silently projecting its coordinates to XY. Unbounded construction
planes do not force these files through the finite-surface importer.

`iges_parameters` shares parameter access and units between the importers.
`iges_report` owns report formatting and JSON escaping. Reports distinguish
native solids, recovered faces, retained invalid solids, wire output, and
unresolved references. An explicitly authored manifold solid is not silently
reclassified as unrelated sheets when its topology cannot be reconstructed.

## Export

`g-iges` owns database traversal, selection state, instance placement, and
Boolean expressions. It never repurposes database directory bookkeeping fields
as writer state. Its primitive support list is explicit: the presence of a
library callback alone does not establish that the callback is suitable for
this converter.

When a selected root is also a dependency of another selection, an independent
instance preserves its unplaced occurrence as well as the nested placement.

`iges_primitive_export` chooses native CSG, exact BRep, or the NMG fallback.
Already-polyhedral geometry stays on the NMG path. Unsupported voxel, submodel,
annotation, and metadata selections are diagnosed rather than passed to an
unqualified callback. The BRep and NMG exporters own their topology mappings;
they do not use process-global directory or parameter sequence counters.

`iges_writer` owns one IGES file's scratch sections, sequence numbers, root
status, properties, and counters. Its typed parameter builder rejects
non-finite numbers and physical record delimiters in strings. Numeric fields
remain within physical records, while Hollerith strings may continue without
splitting their count-plus-H prefix. Output records are checked for field and
sequence overflow and for write, flush, and close failures.

Flattened BReps retain a group containing all their trimmed faces, so a placed
instance does not refer only to the last exported face. A face with an
unrepresentable trimming boundary is diagnosed; missing inner loops are not
silently filled. Singular boundaries may use parameter-space curves alone.

Multi-file export owns a fresh writer per region occurrence. Accumulated
placements are retained, including repeated instances. Sanitized filename
collisions get numbered suffixes, and existing files are preserved.

## Ownership and failure boundaries

`iges_runtime` provides scoped database-internal objects, file handles, BRep
callback destinations, staged destinations, and progress reporting. A failed
import or I/O failure does not replace an existing destination. A diagnosed
partial export can publish a structurally complete file and return failure.
Input/output aliases are checked before writing, including hard links.

The remaining non-local NMG error handling is isolated in small functions
without automatic C++ owners across the jump boundary. Conversion is serialized;
`-P` remains accepted for command-line compatibility but does not enable parallel
NMG recovery. Library debug masks are the existing library globals, not new
converter-owned global state.

Progress is reported roughly every five seconds, including during individual
geometry operations. Source-item counts do not pretend to estimate remaining
CPU time. Short conversions do not wait for a reporting interval.

## Qualification

The IGES CTest group covers physical records and numeric boundaries, native CSG,
exact BRep and NMG round trips, instance transforms, retained-solid policy,
recovery and reassembly, metadata, naming, malformed input, output aliases,
write failure, multi-file collisions and placements, and serialized faceted
export. Callback-reachability regressions include binary metadata, inward-only
shells, and submodels.

Useful targeted commands with a configured Ninja build are:

```sh
ninja -C .build iges-g g-iges test_iges_document test_iges_import test_iges_writer
ctest --test-dir .build -R iges --output-on-failure
```

Broader qualification should retain a fixed IGES input corpus and a complete
`share/db` export/default-import/strict-import pass. A readable file, a complete
conversion, and geometric equivalence are different checks. Sampled ray and
surface comparisons supplement status and topology checks; neither establishes
universal geometry-kernel correctness. Shared BRep tessellation and ray-tracing
limitations remain library work, not changes to the IGES interchange contract.
