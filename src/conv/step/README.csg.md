# Native CSG in AP203e2, AP214, and AP242

`g-step` normally uses its existing BRep-oriented export path.  Passing
`--native-csg` asks the AP203e2, AP214, or AP242 plugin to preserve a BRL-CAD Boolean
expression using STEP `CSG_SOLID`, `BOOLEAN_RESULT`, and CSG primitive
entities where possible:

```sh
g-step --schema ap203e2 --native-csg -o model.stp model.g region
g-step --schema ap214 --native-csg -o model.stp model.g region
g-step --schema ap242 --native-csg -o model.stp model.g region
```

The option is intentionally off by default: many STEP consumers support BRep
substantially better than CSG entities.  Schema selection is always performed
by the common `g-step` host; fixed-schema compatibility executables are no
longer built.

## Representation policy

Union, intersection, and difference map directly to STEP.  XOR is lowered to
the exact set expression `(A union B) difference (A intersection B)`.  STEP
has no finite universal-set operand with which to preserve BRL-CAD NOT, and
guard/no-op nodes do not have a lossless CSG encoding; those operations make a
native-CSG export fail rather than silently change the model.

Nested combinations are expanded into the STEP Boolean expression and leaf
matrices are applied to their geometry.  Thus the set expression is retained,
but nested combination names, object identity, and instancing are not.  A
cycle in the combination graph is rejected.

AP203e2, AP214, and AP242 permit `SOLID_MODEL` (including a
`MANIFOLD_SOLID_BREP`) as a `BOOLEAN_RESULT` operand.  When a finite BRL-CAD
leaf has no matching STEP CSG primitive but supplies a BRep conversion, the
exporter uses that BRep as a leaf without flattening the surrounding Boolean
tree.  A BRep cannot be the root expression of `CSG_SOLID`; a one-leaf wrapper
therefore becomes one ordinary BRep representation.  A leaf with neither a
legal CSG primitive nor a BRep conversion makes the requested native-CSG
export fail explicitly.

## Primitive coverage

| BRL-CAD geometry | AP203e2 | AP214 | AP242 | Imported back as |
| --- | --- | --- | --- | --- |
| spherical ELL | `SPHERE` | `SPHERE` | `SPHERE` | ELL sphere |
| orthogonal, non-spherical ELL | BRep operand | BRep operand | `ELLIPSOID` | BRep / ELL |
| circular RCC | `RIGHT_CIRCULAR_CYLINDER` | same | same | TGC |
| circular cone or frustum | `RIGHT_CIRCULAR_CONE` | same | same | TGC |
| elliptical TGC with similar, aligned end sections and optional top offset | BRep operand | BRep operand | `ECCENTRIC_CONE` | BRep / TGC |
| orthogonal rectangular ARB8 | `BLOCK` | `BLOCK` | `BLOCK` | ARB8 |
| matching right-wedge ARB8 | `RIGHT_ANGULAR_WEDGE` | same | same | ARB8 |
| rectangular ARB5 with centred normal apex | BRep operand | BRep operand | `RECTANGULAR_PYRAMID` | BRep / ARB5 |
| valid tetrahedral ARB4 | BRep operand | BRep operand | `TETRAHEDRON` | BRep / ARB4 |
| valid planar convex ARB8 not matched above | BRep operand | BRep operand | `CONVEX_HEXAHEDRON` | BRep / ARB8 |
| legal ring TOR (`minor < major`) | `TORUS` | `TORUS` | `TORUS` | TOR |
| planar HALF used as a Boolean operand | `HALF_SPACE_SOLID` | same | same | HALF |
| other finite geometry with `ft_brep` support | `MANIFOLD_SOLID_BREP` operand | same | same | BRep |

The AP242 schema also defines `CYCLIDE_SEGMENT_SOLID`.  It is not emitted yet:
BRL-CAD has no direct Dupin-cyclide primitive, and a safe implementation needs
to identify and parameterize exact subsets of curved PIPE geometry.  Such
geometry can still participate as a BRep operand when its BRep conversion is
available.

The importer accepts the shared AP203e2/AP214/AP242 core primitives and Boolean
operators, AP242-only implicit primitives, planar half spaces, nested CSG
solids, and manifold-BRep/solid-replica operands.  It performs a complete
structural and parameter validation pass before materializing a tree.
Unsupported or malformed CSG is reported as skipped and causes strict imports
to fail; an invalid root is not attached to the product.

## Exercised examples

The regression test uses installed `share/db` models as its source of truth:

- `boolean-ops.g:a-b+c` becomes difference followed by intersection in all
  three schemas.  Its
  block and cylinder stay implicit, while the non-block ARB8 is a BRep leaf;
  AP203e2, AP214, and AP242 imports reconstruct that operator hierarchy.
- A generated AP203e2 tree covers every shared implicit primitive and a
  half-space operand.  A separate hand-authored AP203e2 Part 21 fixture covers
  the same importer path independently of our exporter.
- `operators.g:subtraction` exercises nested combination expansion and a leaf
  matrix.  The sphere stays implicit, and the translated EHY BRep returns at
  the transformed bounds.
- `primitives.g:ell.r` is one AP214 BRep representation but an AP242
  `ELLIPSOID`, which imports as an ELL with the original axes.
- `primitives.g:tec.r` is an AP242 `ECCENTRIC_CONE` and imports as a TGC with
  the original dimensions.  Axis-vector signs may change because STEP's
  placement is right-handed; that does not change the represented solid.
- `primitives.g:arb5.r` is an AP242 `RECTANGULAR_PYRAMID` and imports with its
  original ARB5 vertices.  An off-centre-apex fixture verifies that a general
  ARB5 is not incorrectly classified as this narrower primitive.
- `primitives.g:arb4.r` is an AP242 `TETRAHEDRON`.  A generated planar,
  tapered ARB8 covers `CONVEX_HEXAHEDRON`; the non-planar `primitives.g:arb8`
  is correctly ineligible and remains a BRep.

This mode preserves the best available implicit leaves and Boolean tree.  It
does not replace the future fully evaluated-BRep export path; that remains the
preferred broadly interoperable result once Boolean evaluation is reliable
enough.
