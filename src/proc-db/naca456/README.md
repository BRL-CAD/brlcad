# NACA Procedural Wing Generator

This directory contains the development version of `naca456`, a proc-db tool
for generating semi-span NACA wing geometry as either BoT or BREP objects in
`.g` databases.

The implemented section math covers classic 4-digit NACA airfoils, 5-digit
mean-line sections, 5-digit reflex mean-line sections, and the NACA 6/6A
families covered by the public-domain NACA456 reference implementation from
Public Domain Aeronautical Software:

https://www.pdas.com/naca456download.html

## Current Capabilities

- BoT output with closed root and tip topology.
- BREP output with explicit vertices, shared edges, loops, trims, and
  `IsValid()`/`IsSolid()` checks before writing.
- Local bilinear ruled NURBS side patches for robust BREP skins.
- Optional `--brep-surface smooth` mode using bicubic Hermite NURBS side
  patches with shared explicit edge curves and trims.
- NACA 4-digit sections such as `0012`, `2412`, and `4415`.
- NACA 5-digit sections such as `23012`, including reflex sections such as
  `23112`.
- NACA 6-series sections in the `63` through `67` families, such as `63-215`
  and `65-210`.
- NACA 6A-series sections in the `63A`, `64A`, and `65A` families, such as
  `64A210`.
- Optional 6-series mean-line loading parameter with `-u`; the default is
  `1.0`. The 6A modified mean line uses the NACA456 fixed `a=0.8` behavior.
- Optional sharp trailing edge coefficient for 4- and 5-digit thickness forms.
- Root-to-tip airfoil interpolation with `-A`, including interpolation across
  different supported NACA families when the requested section topology matches.
- Taper, sweep angle or sweep offset, dihedral, tip twist, spanwise station
  count, chordwise sample count, and model placement offsets.
- `--tip-style rounded` by default, with `--tip-style flat` as a planar cap
  fallback.
- `--tip-specification` JSON input for advanced rounded, raked, Hoerner,
  winglet, and blended winglet parameters, with strict schema validation.
- Append mode for placing multiple wings in one `.g` database.
- `--demo-file` sampler output with a top-level `all` comb unioning all sample
  wing regions.

The BREP model is solid and uses explicit topology. The default side skin is
represented as local bilinear ruled NURBS patches per span/chord cell, avoiding
global strip interpolation across sharp tip transitions. The optional smooth
side skin uses bicubic Hermite patches per span/chord cell with limited
tangents. In both BREP surface modes, root caps and flat tip caps are
triangulated planar faces, while rounded tips use refined closure stations and
a small terminal cap.

By default, the generator creates a rounded tip by placing the nominal tip
section at a tip shoulder, adding elliptical-style closure stations, and closing
with a small terminal cap. `--semi-span` remains the distance from root to the
outer rounded closure. With `--tip-style flat`, the final section is placed at
the semi-span station and closed with a planar cap. BoT output includes
surface normals for smoother rounded-tip display, with root and terminal caps
kept visually flat.

Advanced tip specifications are supplied as versioned JSON. Version 1 accepts
`rounded`, `flat`, `raked`, `hoerner`, `winglet`, and `blended_winglet`. The
Hoerner and winglet styles are procedural geometric approximations intended for
model generation, not aerodynamic design or certification.

Length inputs are BRL-CAD model-space values. The generator creates a semi-span
wing: `-s`/`--semi-span` specifies the generated root-to-tip length directly,
while `--full-span` specifies an aircraft full span and generates half of it.
The `-w` short option is `--sweep-angle`; use `--sweep-offset` when an exact tip
leading-edge X offset is desired instead.

## Examples

Generate a BoT wing:

```sh
naca456 -m bot -f -o naca456-wing.g -n wing_bot -a 2412 -s 1000 -r 250 -t 125 -w 4.6 -d 5 -T -2 -S 9 -c 65
```

Generate a BREP wing with root-to-tip section interpolation:

```sh
naca456 -m brep -f -o naca456-wing-brep.g -n wing_brep -a 23012 -A 65-210 -s 1000 -r 280 -t 110 --sweep-offset 120 -d 4 -T -3 -S 8 -c 29
```

Generate a 6A-series BREP:

```sh
naca456 -m brep -f -o naca456-64a.g -n wing_64a -a 64A210 -s 900 -r 240 -t 120 -d 4 -S 7 -c 25
```

Generate a BREP with smoother side patches:

```sh
naca456 -m brep --brep-surface smooth -f -o naca456-smooth.g -n wing_smooth -a 2412 -s 900 -r 240 -t 120 -w 5 -d 4 -T -2 -S 8 -c 29
```

Generate a BREP with the flat tip fallback:

```sh
naca456 -m brep --tip-style flat -f -o naca456-flat.g -n wing_flat -a 0012 -s 700 -r 220 -t 120 -S 6 -c 25
```

Generate a BREP with a JSON raked tip:

```sh
naca456 -m brep -f -o naca456-raked.g -n wing_raked -a 2412 -s 780 -r 230 -t 110 --sweep-offset 70 --tip-specification regress/naca456/tip-raked.json -S 7 -c 25
```

Create a 36-wing sampler database. The generated `.g` includes individual wing
regions and a top-level `all` comb:

```sh
naca456 --demo-file naca456-demo.g
```

Create the same sampler with smooth BREP rows:

```sh
naca456 --brep-surface smooth --demo-file naca456-demo-smooth.g
```

## Validation

`regress/naca456/naca456.sh` is the CTest-facing smoke/regression harness. It
checks:

- BoT creation and right-handed/counter-clockwise orientation reporting.
- BREP creation and MGED recognition.
- Smooth BREP side-surface generation.
- Sharp trailing edge BREP generation.
- 5-digit, reflex 5-digit, 6-series, and 6A-series generation.
- Append mode and object name collision rejection.
- Root-to-tip airfoil interpolation.
- Rounded and flat tip-style metadata and demo propagation.
- JSON rounded/raked/Hoerner/winglet/blended winglet tip specifications and
  mutual-exclusion validation.

Run the test through CTest with `ctest -R regress-naca456`.
