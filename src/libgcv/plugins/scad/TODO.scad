OpenSCAD (.scad) import plugin - fidelity notes and TODO
========================================================

This importer parses and evaluates the OpenSCAD language and writes the
resulting Constructive Solid Geometry into a BRL-CAD .g database.  Because
OpenSCAD and BRL-CAD are both CSG modelers, a large, useful subset maps with
full round-trip fidelity.  This file catalogues where information is lost or
approximated, and what BRL-CAD enhancements would close each gap.

Legend:
  [EXACT]  round-trippable, no loss
  [APPROX] geometry produced but not bit-identical to OpenSCAD's mesh
  [LOSS]   information dropped; geometry may be missing or simplified
  [ENH]    suggested BRL-CAD enhancement to remove the limitation


1. Primitives
-------------
cube          -> arb8 (mk_rpp)                                   [EXACT]
sphere        -> ell  (mk_sph)                                   [EXACT for round]
    A sphere is written as a true ellipsoid regardless of $fn.  A deliberately
    low-poly sphere (e.g. sphere($fn=6)) therefore becomes a smooth sphere.
    Use --facetize to force a faceted BoT instead.                [LOSS: faceting]
cylinder      -> rcc / trc (smooth)                              [EXACT for round]
              -> BoT n-gon prism/frustum when 3 <= $fn <= facet-max (default 12)
    The n-gon-prism idiom cylinder(h,r,$fn=6) is preserved as a faceted BoT.
    Round cylinders ($fn=0 or $fn>facet-max) are exact TGCs.     [APPROX: faceted]
polyhedron    -> BoT solid (fan-triangulated faces)             [EXACT topology]
    Faces with >3 vertices are fan-triangulated; non-planar faces are assumed
    planar.  Winding is not analyzed (BoT is written RT_BOT_UNORIENTED, so
    solidity is correct but per-face shading normals may point inward). [APPROX]


2. 2D subsystem
---------------
circle/square/polygon are only realized when consumed by an extrude.  A 2D
object left at the top level cannot be represented in a 3D .g and is skipped
with a warning.                                                  [LOSS]
    [ENH] Map standalone 2D profiles to librt "sketch" objects so they survive
          as first-class 2D geometry.
polygon() with holes (multiple paths) keeps only the outer path. [LOSS: holes]
    [ENH] Emit sketch + subtracted sketches, or a plate-mode BoT.


3. Extrusions
-------------
linear_extrude (no twist, unit scale) -> sketch + extrusion             [EXACT]
rotate_extrude (profile on one side of axis) -> sketch + revolve        [EXACT]
    The 2D profile is written as a native librt "sketch" (one closed line-
    segment curve per loop) and swept by an "extrude" or "revolve" solid, so
    these round-trip and are edited parametrically in BRL-CAD.  Nested loops
    (polygon holes, or one profile inside another) become holes via the
    even/odd fill rule the sketch primitives use.
linear_extrude with twist or non-unit scale -> BoT prism (tessellated)  [APPROX]
rotate_extrude whose profile crosses the axis (x<0) -> BoT              [APPROX]
    These cannot be represented by a single extrude/revolve, so they fall back
    to a tessellated BoT.  The BoT cap triangulation is a centroid fan, which
    is correct for convex/star-shaped profiles but can self-intersect for an
    arbitrary concave profile.
    [ENH] Replace the centroid-fan cap with a robust ear-clipping / constrained
          triangulation so concave twisted profiles tessellate correctly.
    [ENH] Curve segments (carc) could represent circle/arc profiles exactly
          instead of the $fn-tessellated line-segment approximation.
    NOTE: two OVERLAPPING (not nested) 2D children in one extrude are treated
          by the even/odd rule as cancelling in the overlap, whereas OpenSCAD
          unions them.  Disjoint children and true holes are handled correctly.


4. Operations
-------------
union/difference/intersection -> boolean combinations           [EXACT]
hull()        -> BoT of the 3D convex hull of child vertices     [APPROX]
    Child solids are sampled (cube corners, cylinder rings, a lat/long sphere
    sample, mesh/polyhedron vertices) and run through bg_3d_chull().  Curved
    inputs are hull-approximated by their sample points.
    [ENH] Sample density is fixed; expose it, or tessellate children exactly
          before hulling.
minkowski()   -> base (first) child only; rounding kernel dropped [LOSS]
    [ENH] Implement a true Minkowski sum (offset) once BRL-CAD grows the
          primitive, or approximate by sweeping the kernel.
projection()  -> skipped                                         [LOSS]
offset()      -> skipped (children pass through unchanged)       [LOSS]
resize()      -> identity (needs child bounding box)             [LOSS]
    [ENH] Evaluate the subtree bounding box (rt_bound / nmg) to honor resize().
render()      -> transparent passthrough group                  [EXACT]


5. Import of external assets
----------------------------
import()  (STL/OFF/AMF/3MF/DXF/SVG) -> skipped                  [LOSS]
    [ENH] Recurse through libgcv to import the referenced file and graft it in.
surface() (heightmap DAT/PNG)        -> skipped                  [LOSS]
    [ENH] Map to a librt "dsp" (displacement map) object.
text()                               -> skipped                  [LOSS]
    [ENH] Requires font rasterization/outline extraction (libfontconfig +
          freetype) to build 2D outlines, then extrude as above.


6. Color and materials
----------------------
color("name"|[r,g,b,a]|"#rrggbb") -> combination color attribute [APPROX]
    RGB is stored on the enclosing combination and, for a top-level colored
    object, on its region so it renders.  The alpha channel is dropped, and a
    per-leaf color inside a single region is only stored as a comb attribute
    (raytracing uses the region color).                          [LOSS: alpha]
    [ENH] Map alpha to a transparency shader; split multi-colored subtrees into
          separate regions automatically.


7. Language semantics
---------------------
- Scoping is dynamic (a new scope's parent is the enclosing runtime scope)
  rather than OpenSCAD's lexical scoping of ordinary variables.  This makes the
  special variables $fn/$fa/$fs propagate correctly; it can differ from
  OpenSCAD only when a module relies on a global that a caller has locally
  shadowed (rare).                                               [APPROX]
- Variable assignments are evaluated in source order, not with OpenSCAD's
  "last assignment in a scope wins for the whole scope" hoisting.  Module and
  function *definitions* ARE hoisted (forward references work).  [APPROX]
- C-style list-comprehension generators [for(a=0;cond;a=a+1) ...] are not
  parsed.                                                        [LOSS]
- The animation variable $t is fixed at 0 and $preview is false. [LOSS]
- rands() uses a deterministic PRNG (for reproducible conversions) rather than
  OpenSCAD's nondeterministic default.                           [APPROX]
- Unknown functions evaluate to undef and unknown modules are skipped, each
  with a warning, so an unsupported construct degrades gracefully rather than
  aborting the whole conversion.


8. include / use
----------------
include <f> and use <f> are resolved relative to the importing file's
directory only.  The OPENSCADPATH library search path and installed library
locations are not consulted.                                     [LOSS]
    [ENH] Honor OPENSCADPATH and a configurable library search path.


9. Modifier characters
----------------------
*  disable subtree      -> subtree skipped                      [EXACT]
!  root/show-only       -> only that subtree is emitted         [EXACT]
#  highlight/debug      -> emitted normally (highlight not shown) [APPROX]
%  background/transparent-> excluded from the CSG result         [EXACT per spec]
