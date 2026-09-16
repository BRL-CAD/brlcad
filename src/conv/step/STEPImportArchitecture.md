STEP Import Architecture
========================

Purpose
-------

This note describes the internal organization of the public `step-g`
importer.  These interfaces are implementation details, not plugin ABI.  The
design goals are deterministic cross-platform behavior, one implementation of
schema-neutral logic, bounded compiler resource use, and short rebuild cycles
for focused geometry work.

Runtime boundary
----------------

`step-g` is a small host.  It examines the Part 21 header and loads one schema
plugin.  A plugin contains its generated STEPcode bindings, schema-specific
adapters, and the schema-neutral importer common objects.

The common importer remains a CMake object library.  Direct object consumption
is intentional:

* file-scope `Factory` registrations cannot be discarded by an archive linker;
* Windows does not need another exported C++ DLL interface;
* there is no additional runtime dispatch or allocation boundary; and
* every schema plugin uses exactly the same compiled common implementation.

Schema-dependent code belongs under `ap203`, `ap203e2`, `ap214`, or `ap242`.
A source listed by `StepImportCommon.cmake` is checked at configure time for
generated schema includes and AP-dependent preprocessor conditionals.

Common build units
------------------

`STEPWrapper.cpp`
    Owns STEPcode sessions, lazy indexing and materialization, import options,
    entity inventory, diagnostics, progress, work scheduling, and telemetry.
    It deliberately does not include the document conversion implementation
    or the solid-BRep algorithms.

`STEPImportPipeline.cpp`
    Owns document traversal, product and representation dispatch, detached
    geometry jobs, deterministic result ordering, and database publication.
    The document implementation fragment is included only here; it calls the
    private BRep operations below rather than text-including them.

`STEPBrepTopology.cpp`, `STEPBrepPeriodic.cpp`, `STEPBrepPullback.cpp`,
`STEPBrepSeamRepair.cpp`, `STEPBrepLoopRepair.cpp`, `STEPBrepFinalize.cpp`
    Own the ordered solid-BRep phases as independently compiled units.  The
    split follows algorithmic ownership, so a seam or pullback fix does not
    rebuild unrelated topology, document, or GEOMETRIC_SET code.

`STEPBrepRepairInternal.h`
    Declares the private data and operations shared by those BRep units and
    the document pipeline.  Its namespace has hidden visibility and is not a
    schema-plugin ABI.

`STEPGeometricSet.cpp`
    Owns GEOMETRIC_SET curve, point, datum-plane, and bounded-surface output.
    It is independent of the solid-BRep pipeline so work on wire and surface
    models has a focused compile/test cycle.

`STEPImportInternal.h`
    Holds private policy ceilings and bounded-work helpers needed by both the
    wrapper core and conversion pipeline.  It must not acquire schema types or
    become a public plugin interface.

`STEPConversionStatus.h`
    Defines the small internal conversion-result vocabulary shared by
    document and GEOMETRIC_SET dispatch.

`step-g/OpenNurbsInterfaces.*`
    Provides schema-neutral geometry proofs and topology operations used by
    entity adapters and higher-level import paths.  New callers should use a
    narrow declared function rather than text-including another algorithm.

`step-g/*.cpp`
    Implements schema-neutral entity adapters.  These objects may register
    with `Factory`, but must not include generated schema bindings.

Conversion flow
---------------

1. The host selects and loads a schema plugin.
2. `STEPWrapper` indexes the file, prints the entity census, and builds the
   schema-independent document inventory.
3. The import pipeline resolves products, occurrences, representations, units,
   style, and selected geometry roots.
4. Exact geometry dependencies are detached from STEPcode-owned instances.
5. Geometry workers construct and validate candidates without writing the
   database.
6. Completed results are published in deterministic STEP-ID order.  Safe
   repair, permissive inference, invalid preservation, strict transaction, and
   loss accounting are finalized at this boundary.

Design invariants
-----------------

* Generated schema classes never enter common build units.
* STEPcode instances are not retained after their lazy materialization batch.
* A worker does not write the BRL-CAD database directly.
* Source-faithful construction precedes safe repair; tagged inference is a
  whole-item retry and must still produce a valid completed object.
* Every authored geometry item has exactly one final accounting result.
* Repeated traversal paths do not duplicate skipped-item or warning records.
* Model-space acceptance uses the declared or explicitly recorded measured
  tolerance, never a solver convergence threshold.
* Schema plugins share code at build time without requiring a common C++
  runtime library ABI.

Adding functionality
--------------------

Put a new feature in the narrowest owner:

* schema layout or legal alternatives: the corresponding AP directory;
* entity-to-private-geometry adaptation: `step-g`;
* product, occurrence, representation, or result policy: import pipeline;
* GEOMETRIC_SET family conversion: `STEPGeometricSet.cpp`;
* reusable geometric proof or topology operation: OpenNURBS interfaces; and
* reporting, metadata, presentation, material, CSG, sweep, or tessellation:
  the existing named common subsystem.

Do not add a schema conditional to a common source merely because two editions
currently happen to need different callers.  Put the edition-specific choice
at the plugin boundary and expose the smallest schema-neutral operation.

Focused verification
--------------------

A GEOMETRIC_SET change should rebuild only its common object and the requested
schema module links.  The owned fast gate is:

```
cmake --build .build --target step-schema-ap214 -j8
ctest --test-dir .build -R '^ap214_geometric_surface_set$' --output-on-failure
```

Solid-BRep work should select the affected entity with `step-g --entity` and
run the corresponding periodic, seam, inference, or transaction test range
before a corpus retry.  A topical edit should compile one BRep object plus the
requested module links.  Build all schema modules and run plugin-load tests
after changing `STEPBrepRepairInternal.h` or another common interface.

Measured iteration
------------------

On the qualification host, an AP203/AP214 seam-repair edit compiled one
common object and linked two modules in 9.1 seconds with 463 MiB peak RSS.
Wrapper-core and GEOMETRIC_SET edits measured 7.2 and 4.5 seconds
respectively.  Preserve this granularity: do not reintroduce a unity include,
duplicate algorithms between units, or add a cross-platform C++ shared-library
ABI merely to share this private implementation.

Post-split dry runs also retained the focused runtime profile and exact
results: c-wireframe completed in 9.9 seconds with 618/618 bounded surfaces,
and d-wireframe completed in 26.7 seconds with the same 1195/1201 result and
same six source discrepancies as the pre-split run (approximately 9 and 26
seconds).  Machine load prevents treating sub-second differences as a
benchmark result, but there is no indicated material conversion overhead.
