# STEP and IGES Semantic Storage Study for BRL-CAD `.g`

## Conclusion

BRL-CAD's database can already retain a large fraction of the *shape* found
in mechanical STEP and IGES files. Its B-rep, NMG, BOT, sketch, analytical
primitive, and combination records understand curves, surfaces, topology,
meshes, CSG, and fixed transforms. That should not be conflated with
understanding the source model.

The main gap is not another long list of geometry primitives. It is a small
set of missing semantic foundations:

1. a typed, queryable relationship graph with stable object identities;
2. stable references to B-rep subshapes such as a face, edge, or vertex;
3. product, occurrence, representation, and configuration semantics beyond a
   static combination tree;
4. typed values, dimensional units, material assignments, and properties; and
5. semantic PMI: dimensions, GD&T, datum systems, and their associations.

Without those foundations, a converter can preserve a shape, a display
approximation, or a foreign payload, but it cannot truthfully say that the
corresponding STEP or IGES construct is stored with native BRL-CAD
understanding. Reifying every foreign entity as a DB5 primitive would be the
wrong response: STEP alone has application-protocol- and edition-specific
schemas with thousands of entities. A schema-neutral native model is needed
for the useful common concepts; specialized domains should stay out of core
unless a demonstrated BRL-CAD workflow needs them.

## Scope

### STEP boundary

STEP is a family of standards, not one entity catalogue. It is not meaningful
to claim coverage of all STEP entities without naming application protocols
and editions. This study covers every `ENTITY` declaration in the mechanical
schemas that the checkout's `step-g` implementation recognizes:

| Family | Checked-in source |
| --- | --- |
| AP203 edition 1 | `src/conv/step/step-g/ap203.exp` |
| AP203 edition 2 | `src/conv/step/ap203e2/ap203e2.exp` |
| AP214 edition 3 | `src/conv/step/ap214/ap214e3.exp` |
| AP242 editions 1-4 | `src/conv/step/ap242/ap242e1.exp` through `ap242e4.exp` |

The source inventory contains **2,469 unique `ENTITY` declarations** and
**552 unique `TYPE` declarations**. `TYPE` declarations are included because
their SELECTs, enumerations, measures, units, and aggregates carry essential
information even when they do not create an independent Part 21 instance.
AP209, AP210, AP224, AP232, AP238, AP239, IFC, and other protocols are
explicitly outside this count. They require separate studies rather than
being silently treated as AP203/AP214/AP242 data.

### IGES boundary

The IGES audit uses the IGES 5.3 entity catalogue: **90 type codes** including
the null marker. Form-specific entities are split whenever a form changes
meaning, resulting in **145 IGES matrix rows**. The historical BRL-CAD
`iges-g` manual documents IGES 5.1 input, so the 5.3 comparison deliberately
separates database capability from present translator coverage.

The audit also records BRL-CAD's nonstandard legacy type `700`
(4-by-4 transformation matrix). It is not treated as an IGES 5.3 construct.

### What counts as "with understanding"

A construct is native only when the `.g` model has an explicit record and
operations that can validate, query, edit, traverse, and export its fields and
relationships. It is not enough to:

- attach an unregistered attribute convention to a geometry object;
- insert opaque source text or a serialized graph into a `binunif` object;
- store only the visible geometry while discarding the source association;
- materialize a fixed result of a parametric, configuration, or constraint
  construct and call the original construct preserved; or
- retain an identifier with no native object relationship that can resolve it.

This is intentionally stricter than lossless *payload preservation*. The
current STEP documentation describes `step:` attributes and a `_STEP_METADATA`
binary object for retained metadata. Those mechanisms can be useful for
provenance and re-export, but they are excluded from native-support credit in
this study.

### Status vocabulary

| Status | Meaning |
| --- | --- |
| **Native geometry** | `.g` directly understands the mathematical geometry or topology. |
| **Geometry only** | The result can be represented, but source semantics, identity, or relationships are absent. |
| **Partial native** | A useful subset exists, such as rigid placement or annotation graphics. |
| **Missing native** | No DB5 object/relationship has the required semantics. |
| **Defer domain** | The construct is real but should normally remain in a domain module or sidecar. |
| **Do not add** | Executable/procedural or unsafe foreign semantics should not become native CAD state. |

## Method and audit artifacts

`doc/step_iges_g_entity_inventory.py` parses the checked-in EXPRESS sources,
collects direct supertypes, inventories each supported `ENTITY` and `TYPE`,
and emits one CSV row per construct. The script also contains the IGES 5.3
type/form table used for this study and compares it with
`src/conv/iges/iges_types.c`.

Run the following from the repository root:

```powershell
python doc/step_iges_g_entity_inventory.py --check
python doc/step_iges_g_entity_inventory.py --format csv > doc/step_iges_g_entity_matrix.csv
```

The check verifies the 2,469/552 STEP counts, 90 IGES type-code count, and the
observed IGES type-table delta. The generated matrix has columns for schema
family, entity/type/form, immediate EXPRESS supertypes or type kind, semantic
family, native status, native target, recommended disposition, and current
IGES catalogue presence. It is the exhaustive per-construct appendix; the
remaining sections explain the findings by semantic family rather than
repeating thousands of nearly identical rows.

The classifier is intentionally conservative. A construct with several
facets is placed in the family that requires the strongest missing native
semantics. The CSV retains its exact name and source schemas so a reviewer can
override a family choice without losing inventory coverage.

## Native `.g` baseline

The following table distinguishes capabilities already present in DB5 from
nearby concepts that are often mistaken for full support.

| Native facility | What `.g` understands today | Important limit |
| --- | --- | --- |
| `brep` and `nmg` | Trimmed surfaces, curves, vertices, edges, loops, faces, shells, and solid topology | No stable database-level address for a subshape and no source feature/shape-aspect identity |
| `bspline` and `sketch` | NURBS surfaces plus planar line, circular-arc, NURB, and Bezier sketch segments | No generic 3-D curve/document graph or source associative history |
| Analytical primitives and combinations | CSG leaves, union/intersection/subtraction trees, rigid leaf matrices, regions | A combination is a Boolean expression, not a generic product or relationship node |
| `bot` and `pnts` | Triangle mesh and point positions; supported point colour/scale/normal variants | No scan registration, intensity, acquisition metadata, or submesh/property association model |
| `annot` | Model-space graphics, text, segment roles, line styles, fills, fonts, and placement | No dimension value, tolerance, datum-system, or target association semantics |
| `datum` | Point, line, plane, frame, and target/reference geometric forms | No feature/subshape association or semantic datum-system graph |
| `material` and combination appearance | Material record, RGB, shader, region material fields | Material values are not a typed, unit-aware property/assignment graph |
| `joint` and `constraint` | A display-oriented joint with two path strings; a generic expression record | Not a typed kinematic assembly model or a validated geometric-constraint system |
| Global database units | One database-wide linear conversion factor | No dimensions, angle/time/temperature units, derived units, or per-property measure context |
| Attributes and `binunif` | Generic key/value data and binary arrays | Explicitly excluded here: they have no native STEP/IGES semantic contract |

The relevant DB5 type declarations are in `include/rt/db5.h`; B-rep, sketch,
annotation, datum, point, and joint structures are in `include/rt/geom.h`;
combination, constraint, and material records are in `include/rt/nongeom.h`.
The narrowness of the existing annotation and datum types is deliberate
evidence for calling their current support partial rather than a substitute
for a complete PMI model.

## Existing converter behavior is not database coverage

`step-g` currently supports AP203, AP203e2, AP214, and AP242 schema plugins,
and its documentation accurately describes useful geometry, assembly,
tessellation, selected material/presentation, and selected PMI conversion.
That conversion work remains valuable. It does not change the answer to the
storage question when a source relationship is represented only through a
`step:` attribute or `_STEP_METADATA` binary record.

Likewise, an IGES entity missing from `iges-g` conversion dispatch may still
fit a native B-rep or annotation object, while an imported entity can be
visibly rendered without its original semantics. The findings below always
state which of those cases applies.

## IGES 5.3 findings

### Geometry and topology

The following IGES families need little or no *new* general geometry storage.
They may need converter work, B-rep robustness work, or a loss contract for
source-derived geometry, but `.g` already has mathematical geometry capable of
representing the result.

| IGES entities/forms | Native status | Correct `.g` target | Semantic caveat |
| --- | --- | --- | --- |
| 100, 102, 104, 106 forms 1-3/11-13/31-38, 108-130 | Geometry only | B-rep, sketch, NURBS, points, or analytical primitive | Offset/derived curves and source construction relationships are not preserved as associations |
| 140-144, 186, 190-198, 502-514 | Geometry only | B-rep/NMG topology | Standalone boundary, edge, loop, and face identities cannot be cited by a second native object |
| 150-168, 180 | Native geometry/geometry only | Analytical primitives and combination Boolean tree | Exact result is available; any source feature history remains absent |
| 124, 184, 308, 408, 412, 414, 430 | Partial native | Combination leaves and rigid matrices | Static instancing works, but source definition, array parameters, and non-geometric membership are not modeled |
| 116, point-bearing 106 forms | Partial native | `pnts` or B-rep/sketch vertices | Point-cloud interpretation and source references are not present |

A new standalone line, circle, or NURBS primitive is therefore not the highest
priority. B-rep and sketch should be used deliberately where the construct is
an actual model curve or surface. The importer must report when it turns a
derived, offset, or parameter-space relationship into merely evaluated
geometry.

### Drafting, presentation, and PMI

| IGES entities/forms | What exists | What is missing |
| --- | --- | --- |
| 106 forms 20-21 and 40; 125; 208-214; 228; 230 | `annot` can retain visible text, leaders, centerlines, symbols, and fills | Associativity, drawing ownership, and exact foreign font/template semantics |
| 202, 204, 206, 216, 218, 220, 222 | Annotation geometry can be displayed | Dimension value, nominal/bounds/tolerance, unit, datum, target, and geometry association |
| 304, 310, 314, 402 visibility forms, 404, 410, 406 presentation forms | RGB/shader and several annotation style fields | Named line-font definitions, layers, visibility scopes, view/camera, paper-space/sheet semantics |
| 406 forms 13, 28-31 | None beyond visual annotation and global linear units | Nominal size, dimensional unit, tolerance, display semantics, and basic-dimension meaning |

The important distinction is between an arrow or text string and a dimension.
A dimension is a semantic statement about one or more targets, a typed value,
a unit, and potentially a tolerance/datum framework. `annot` is a valuable
rendering object but is not that statement.

### Graph, property, and domain entities

| IGES entities/forms | Status | Recommended posture |
| --- | --- | --- |
| 132, 134, 182, 302, 320, 322, 402 grouping/parent/flow forms, 420, 422 | Missing native | Cover only through a future typed relationship graph |
| 406 forms 6-15, 23, 27, 32-33 | Missing/partial | Typed property/assignment model after a product graph exists |
| 406 forms 24-26 | Defer domain | LEP/printed-board artwork and drilling should not become generic solid-model core state |
| 306 and 600 | Do not add | Do not execute or pretend to natively understand IGES macro language |
| 136, 138, 146, 148, 418 | Defer domain | Finite-element topology, loads, displacement, and results belong in an analysis model, not geometry DB5 records |
| 416 and 402 form 12 | Missing native | A safe external-link identity may be worthwhile; automatic dereference or embedded executable content is not |

### Current IGES type-table delta

`NTYPES` is `78`, while the initializer in `src/conv/iges/iges_types.c` has
79 rows: code `0` for the `Unknown entity type` fallback, 77 nonzero standard
codes, and the local type `700`. The comparison excludes the nonstandard
`700` row. Against the IGES 5.3 catalogue, the table does not list these
twelve standard codes:

| Code | Entity | `.g` storage conclusion |
| --- | --- | --- |
| 141 | Boundary | B-rep topology can store geometry; import support is a converter gap |
| 143 | Bounded Surface | B-rep can store bounded/trimmed geometry; import support is a converter gap |
| 146 | Nodal Results | Requires analysis/results semantics; defer domain |
| 148 | Element Results | Requires analysis/results semantics; defer domain |
| 182 | Selected Component | Requires a generic selection/relationship graph |
| 192 | Right Circular Cylindrical Surface | B-rep can store it; import support is a converter gap |
| 194 | Right Circular Conical Surface | B-rep can store it; import support is a converter gap |
| 196 | Spherical Surface | B-rep can store it; import support is a converter gap |
| 198 | Toroidal Surface | B-rep can store it; import support is a converter gap |
| 204 | Curve Dimension | Requires semantic PMI |
| 213 | New General Note | `annot` can represent a visual subset; rich text semantics remain partial |
| 316 | Units Data | Geometry can normalize length; unit-system semantics are missing |

This is an important implementation distinction: six codes are primarily
translator inventory gaps because `.g` has a good target; the others expose
actual missing semantic domains. The full 106/402/406 form split is recorded
in the generated matrix rather than hidden behind a single type-number row.

## STEP findings

The full matrix handles every source declaration. The table below gives the
substantive conclusion for each family, with representative entities rather
than a misleading claim that a family name itself is an entity.

| STEP family and examples | Matrix determination | Native gap or action |
| --- | --- | --- |
| Geometry and topology: `advanced_face`, `b_spline_*`, `manifold_solid_brep`, `shell_based_surface_model`, `surface_curve`, `swept_*`, `csg_*` | Geometry only | B-rep/NMG/combination can represent most resulting geometry. Preserve exact source semantics only where a native equivalent exists; otherwise report evaluation/loss. |
| Tessellation: `triangulated_face`, `tessellated_solid`, `tessellated_shell`, `coordinates_list` | Partial native | BOT retains triangles and can retain a surface/volume result. It lacks source face, LOD, representation, and product associations. |
| Point clouds and scans: `point_cloud_dataset*`, `scan_3d_model`, `camera_*` | Partial native | `pnts` covers positions and some colours/normals. Sensor, intensity, registration, camera, and scan provenance require a distinct optional model. |
| Product and assembly: `product*`, `assembly_component_usage`, `next_assembly_usage_occurrence`, `mapped_item`, `representation_map` | Partial native | A combination plus matrix carries a static tree. It does not carry product identity, usage role, occurrence identity, revision, substitute, effectivity, or configuration. |
| Representation graph: `shape_representation`, `representation_context`, `representation_relationship`, `property_definition_representation` | Missing native | Add representations, context/unit ownership, and typed directed edges rather than flattening all geometry into one combination. |
| Presentation: `styled_item`, `colour_*`, `presentation_layer_assignment`, `camera_*`, `surface_style_*` | Partial native | RGB/shader and annotation styles help visual fidelity. Layers, visibility scopes, per-item/per-face style, camera/view, and drawing-sheet data need a presentation graph if they are in scope. |
| Annotation graphics: `annotation_*`, `draughting_*`, `text_*`, `leader_*`, `symbol_*` | Partial native | `annot` is the right visual carrier for a bounded subset. It is not a semantic PMI or drawing-model graph. |
| PMI/MBD: `geometric_tolerance*`, `dimensional_*`, `datum*`, `limits_and_fits`, `tolerance_zone*`, `shape_dimension_representation` | Missing native | Add typed dimensions/GD&T, datum systems, values/units, and links to stable subshapes. Do not infer semantic text from glyph outlines. |
| Shape aspects and features: `shape_aspect*`, `face_surface_shape_aspect`, `round_hole`, `pocket`, `slot`, `chamfer`, `edge_blended_solid` | Geometry only | B-rep stores the result but not design intent or the stable feature-to-face/edge relation. Requires subshape links and, only if justified, a feature graph. |
| Constraints and functional definitions: `*_geometric_constraint`, `assembly_*_constraint`, `functionally_defined_transformation`, `generic_expression`, `variational_*` | Missing / do not add | Current `constraint` and `joint` records do not model these. Typed constraints are a possible future layer; executable expression/procedure semantics should be deferred. |
| Materials and properties: `material_designation*`, `material_property*`, `general_property*`, `property_definition*` | Partial native | Material names and appearance exist, but not typed values, measure units, assignment/inheritance, or property-to-product/subshape relation. |
| Measures and units: `*_measure_with_unit`, `*_unit`, `derived_unit`, `conversion_based_unit`, `uncertainty_*`, `precision_*` | Partial native | Global linear scale is insufficient. A typed quantity and unit graph is required for engineering properties and PMI. |
| PDM/administration: `approval*`, `action*`, `document*`, `effectivity*`, `person*`, `organization*`, `security_*`, `configuration_*` | Missing native | This is a full product-data-management domain. A minimal product graph may represent selected useful records; full PLM should not be smuggled into `.g`. |
| Kinematics: `kinematic_*`, `mechanism*`, `*_pair*`, `assembly_joint*`, `motion_*` | Partial native | Existing `joint` is not enough. A true implementation needs occurrence references, joint type/DOF/ranges, drivers, and constraint solving. |
| Quality and equivalence: `a3m_*`, `data_quality_*`, `shape_data_quality_*`, inspection results | Defer domain | Preserve as a validated report sidecar if required. Geometry tools can consume a pass/fail result without pretending to understand all criteria. |
| Manufacturing/additive/composite/harness: `additive_manufacturing_*`, `laminate*`, `ply*`, `harness_*`, process/tool records | Defer domain | Retain through dedicated manufacturing/analysis modules only after a concrete BRL-CAD workflow warrants it. |

### What is already sufficient for STEP geometry

No new `.g` type is justified solely to represent the following classes of
information when the objective is a usable geometric model:

- analytic geometry, B-spline/NURBS geometry, trimmed faces, shells, and
  topology already suitable for B-rep/NMG;
- faceted/tessellated shape suitable for BOT;
- ordinary CSG result trees expressible with BRL-CAD union, intersection,
  subtraction, and rigid transforms;
- a static assembly hierarchy with repeated rigid occurrences; and
- graphical annotations or datum-like planes/axes where semantic PMI is not
  claimed.

Those are converter, kernel, and robustness priorities. They must not be
listed as semantic database gaps simply because a particular importer does
not currently map every entity.

## Native-model requirements

### 1. A real semantic relationship foundation

The common prerequisite is not an arbitrary metadata bucket. A future native
semantic layer needs, at minimum:

- stable IDs independent of object name and directory position;
- closed, versioned core kinds such as product, occurrence, representation,
  property, unit, material assignment, shape target, and PMI item;
- typed scalar, aggregate, enumeration, reference, and measure values;
- directed, ordered relationships with a role and cardinality rules;
- validation, traversal, query, copy, rename, delete, and merge behavior; and
- a documented extension registry for domains that are intentionally not core.

A raw graph containing arbitrary foreign entity names would merely repackage a
binary payload. The native layer must expose operations that understand the
core vocabulary and must reject invalid references or units.

### 2. Stable subshape addressing

PMI, properties, shape aspects, feature associations, and inspection results
frequently target a specific face, edge, vertex, loop, or shell. Object-level
names and bare OpenNURBS array indices are not enough: indices can change when
a B-rep is edited or regenerated.

A subshape target needs a database-level reference resolver with at least the
owning stable object ID, topology kind, persistent identity strategy, and an
explicit unresolved/invalid state. The design must define copy, Boolean,
tessellation, and B-rep-repair behavior. This is a high-value prerequisite for
PMI and is more important than trying to add one foreign feature type at a
time.

### 3. Product, occurrence, and representation graph

Use combinations for geometry and rigid placement, but add semantic nodes for:

- product and product definition/revision;
- occurrence/usage with a transform and role;
- representation and representation context;
- occurrence-to-representation and product-to-property relationships; and
- selected configuration/effectivity records if a use case requires them.

This design preserves assembly intent without changing the fact that a
combination is a Boolean geometry expression. It also gives STEP and IGES
subfigures a native target without forcing each static definition to become a
Boolean union.

### 4. Typed quantities, units, properties, and material assignment

Introduce a first-class measure/value model before adding dozens of individual
material fields. It should support numerical scalar/vector/range values,
seven-dimensional exponents, SI/conversion/derived units, uncertainty, and
provenance. Then add typed assignment relationships from properties and
materials to products, occurrences, regions, or subshape targets.

This is required to preserve STEP physical properties and measure graphs and
IGES units/dimension-property forms. The present `material` record is a useful
starting point but does not make an arbitrary attribute set a semantic
engineering property system.

### 5. Semantic PMI over existing graphics

Keep `annot` as the display object and `datum` as the geometric datum carrier.
Add semantic nodes for dimensions, tolerance values/zones, tolerance type and
modifiers, datum features/targets/systems, display association, and target
links. A semantic PMI record should be valid without a graphical presentation;
conversely, graphical strokes must not be promoted to a dimension merely
because they look like one.

This is the clearest high-value STEP/AP242 and IGES gap after product/subshape
identity. It also permits import/export policy to distinguish semantic loss
from a missing display glyph.

### 6. Optional presentation and scan models

Layers, named styles, views, drawing sheets, custom fonts, point-cloud
registration, and scan acquisition data should be separate optional models.
They are valuable for CAD review and inspection workflows but are not required
to make the ray-traceable solid correct. Do not block PMI or product identity
on a complete paper-space drawing system.

## What should probably not enter core `.g`

| Construct family | Why it is not a good default core addition |
| --- | --- |
| IGES MACRO and STEP generic expressions/procedures | Security, reproducibility, versioning, and foreign-language execution risks; an opaque script is not native semantics |
| Full PLM/PDM administration | Approvals, organizations, contracts, security, workflow, and effectivity have large lifecycle semantics beyond a geometry database |
| FEA models, loads, result fields, and quality criteria | Large domain-specific structures with their own solvers/validation; use an analysis/QA sidecar with explicit references if needed |
| Manufacturing, additive setup, composites, harnesses, and PCB artwork | Valuable in dedicated applications but not broadly useful to solid geometry/ray tracing; add only through domain modules with a real workflow |
| Parametric feature history and a general constraint solver | High complexity and difficult edit semantics; defer until BRL-CAD explicitly chooses to be a parametric feature modeler |
| Exact import of every drawing/font/view convention | Rendering and interchange value is lower than PMI/product identity; selectively support styles after a concrete UI/export need appears |

The decision is not to discard these constructs. It is to avoid claiming they
are native `.g` data before there is a validated, queryable model for them.
A versioned extension can retain a well-defined domain model without forcing
all of it into the core ray-tracing database.

## Recommended implementation sequence

1. **Lock down the semantic storage contract.** Keep the generated inventory
   in review, add representative corpus fixtures, and require import reports
to distinguish native, geometry-only, payload-only, and skipped results.
2. **Add IDs, relationship integrity, and subshape targets.** This is the
   foundation for all product, property, PMI, and inspection associations.
3. **Add values/units/properties/material assignments.** Implement this before
   individual AP242 material/property export claims.
4. **Add product/occurrence/representation nodes.** Preserve static
   combinations as geometry while exposing identity, configuration, and usage
   separately.
5. **Add semantic PMI.** Reuse `annot` and `datum` for display/geometry but
   make semantic dimensions/GD&T independently valid and targetable.
6. **Choose optional extensions by demonstrated corpus.** Presentation sheets,
   scan data, kinematics, manufacturing, FEA, and PDM should each pass a
   separate value/complexity review.

## Completion criteria for future support claims

A future importer/exporter should claim a STEP or IGES construct is supported
with understanding only when all of the following are true:

- its native `.g` object/relationship survives write/read and database copy;
- every source reference resolves to a stable native target or is explicitly
  marked unresolved;
- units and typed values retain their physical meaning;
- import, edit, and export have defined behavior and loss reporting;
- relevant query/validation APIs understand the construct; and
- tests cover both direct storage and a representative interchange round trip.

Until then, retain a payload only as optional provenance and label it exactly
that way. It is not a substitute for semantic support.

## Evidence sources

- STEP schema inventory: the seven EXPRESS sources listed in this report and
  `doc/step_iges_g_entity_inventory.py`.
- STEP converter scope and retained metadata behavior:
  `doc/asciidoc/system/man5/STEP.adoc` and
  `doc/asciidoc/system/man1/step-g.adoc`.
- IGES converter scope: `doc/asciidoc/system/man1/iges-g.adoc` and
  `doc/asciidoc/system/man1/g-iges.adoc`.
- IGES type catalogue present in the source tree:
  `src/conv/iges/iges_types.c`; the audit's IGES 5.3 names/forms are taken
  from *Initial Graphics Exchange Specification, IGES 5.3* (US PRO, 1996).
- Native object evidence: `include/rt/db5.h`, `include/rt/geom.h`,
  `include/rt/nongeom.h`, `include/rt/primitives/brep.h`, and
  `src/librt/primitives/table.cpp`.
