# IGES converter design

IGES is a libgcv plugin with both reader and writer filters. Its C++17
implementation lives in this private `core/` directory. One non-installed
static target, `gcv_iges_core`, supplies the plugin and both command-line
programs, `iges-g` and `g-iges`. The executables do not depend on runtime
plugin discovery. There are no new public BRL-CAD library APIs or exported
IGES interfaces. NMG remains the representation for faceted export,
polygonal import, and drawing-wire output.

The adapters own option parsing, database lifetime, and publication policy.
The plugin borrows the caller's in-memory database; it never closes it.
Reader and writer callbacks contain C++ exceptions at libgcv's C boundary.
The command-line drivers retain stdout, JSON reports, and multi-file output.
Both accept positional source/destination pairs. The export driver recognizes
IGES extensions (or an existing directory in multi-file mode); a matching
database object requires an explicit `-o` destination. File export without
object arguments selects the database's top-level objects.

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

Recovered bounded and trimmed faces are reassembled before partitioning their
connected components. Closed shells with overlapping bounds stay together to
preserve possible cavities. A disconnected open remainder becomes a non-region,
uncolored combination of connected sheet children. The combination retains the
aggregate's source references; children retain face provenance, repair metadata,
and uniform sheet colors. Color boundaries never drive topology partitioning.
Extraction is validated before writing any children, with the original validated
assembly as the fallback. Explicit manifold solids bypass this sheet partition.

`iges_import` handles annotations, datums, and semantic drawing groups. Nested
subfigure definitions and instances are ordered by their dependencies.
`iges_wire` adaptively samples supported model-space curves into bounded NMG
wire geometry. A default file containing only drawings or datums is imported
without silently projecting its coordinates to XY. Unbounded construction
planes do not force these files through the finite-surface importer.

`iges_parameters` shares parameter access and units between the importers.
`iges_convert` shares representation detection, diagnostic summaries, and
output-reference verification between adapters.
`iges_report` owns report formatting and JSON escaping. Reports distinguish
native solids, recovered faces, retained invalid solids, wire output, and
unresolved references. An explicitly authored manifold solid is not silently
reclassified as unrelated sheets when its topology cannot be reconstructed.

## Export

`iges_export` owns database traversal, selection state, instance placement, and
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

CLI progress is reported roughly every five seconds, including during individual
geometry operations. Source-item counts do not pretend to estimate remaining
CPU time. Short conversions do not wait for a reporting interval.
The shared core only invokes optional progress callbacks. The plugin enables
periodic reporting when libgcv verbosity is nonzero.

## Qualification

The IGES CTest group covers physical records and numeric boundaries, native CSG,
exact BRep and NMG round trips, instance transforms, retained-solid policy,
recovery and reassembly, metadata, naming, malformed input, output aliases,
write failure, multi-file collisions and placements, and serialized faceted
export. Callback-reachability regressions include binary metadata, inward-only
shells, and submodels.
Plugin integration tests use libgcv's loader and `gcv_execute`, checking both
registered filters, native CSG, mixed hierarchies, NMG export modes, flattened
BReps, drawing projection, strict failures, and repeatable option defaults.

Useful targeted commands with a configured Ninja build are:

```sh
ninja -C .build iges-g g-iges gcv-iges test_gcv_iges test_iges_document test_iges_import test_iges_writer
ctest --test-dir .build -R iges --output-on-failure
```

Broader qualification should retain a fixed IGES input corpus and a complete
`share/db` export/default-import/strict-import pass. A readable file, a complete
conversion, and geometric equivalence are different checks. Sampled ray and
surface comparisons supplement status and topology checks; neither establishes
universal geometry-kernel correctness. Shared BRep tessellation and ray-tracing
limitations remain library work, not changes to the IGES interchange contract.
## Orientation checking and correction

`iges_orientation` integrates signed volume and oriented area over individual
trimmed B-Rep shells. The divergence theorem supplies the volume integrand;
Green's theorem reduces the trimmed parameter region to nested one-dimensional
integrals around its loops. The adaptive Gauss/Kronrod rule works on curve and
surface spans, with bounded subdivision and evaluation budgets. Coordinates
are centered and scaled per shell. No mesh, scene ray tree, or cached
`SolidOrientation()` answer is used.

Face adjacency supplies a provisional consistent orientation without changing
the input. Only isolated closed orientable shells with a resolved volume sign
and cancelling oriented area receive correction plans. Safe import applies
these plans and records reversed faces; no-repair modes retain the input.
Nested and overlapping shells remain unchanged, because box containment alone
does not establish a valid cavity. Numerical estimates are diagnostics, not
proofs of geometric validity or absence of self-intersections. The read-only
database scan visits stored B-Rep primitives, not Boolean results or transformed
instances, and shares the same checker with import.

In repair-enabled imports, trimmed-face loop winding is normalized before
assembly. A bounded, centered parameter-area calculation establishes direction;
outer/inner conventions are imposed without changing the underlying surface
normal. OpenNURBS validity alone does not enforce this winding convention, and
adjacency propagation on unnormalized trim uses can reverse the wrong faces.
The closed-shell checker rejects uncertain or incorrect parameter-loop winding.

The exporter applies the same conservative shell corrections to temporary
B-Reps obtained from native primitive callbacks. It does not change authored
B-Rep objects or the source database. Native extrusion callbacks in particular
can return inward shells even when their topology passes `IsSolid()`.
