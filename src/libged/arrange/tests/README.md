# `arrange nest` verification and rendered examples

The automated tests divide the implementation at its two important boundaries:

- `arrange_test_masks` checks bit rows across 64-bit boundaries, all orientation
  sets, clearance dilation, repeated determinism, quality monotonicity, and 100
  randomized grids against an independent cell-by-cell feasibility oracle.
- `arrange_test_command` creates a temporary geometry database and exercises
  command dispatch, diagnostics, transactional failure, dry runs, 2D concave
  containers, 3D nesting, repeated instances, values, fractional negative
  origins, clearance, and gravity.
- `analyze_voxelize` (in `src/libanalyze/tests`) checks fractional origins and
  anisotropic cells after an `rt_i` has already been prepared.

Run the tests from a configured build directory with:

```sh
ctest --output-on-failure \
  -R '^(analyze_voxelize|arrange_test_masks|arrange_test_command)$'
```

`arrange_examples.mged` is a reproducible visual demonstration.  It builds an
L-shaped 2D permitted area, an ellipsoidal 3D permitted volume, source parts,
and the two output combinations.  From the source directory, replace `BUILD`
with the configured build directory and run:

```sh
BUILD/bin/mged -c arrange-examples.g < \
  src/libged/arrange/tests/arrange_examples.mged

BUILD/bin/rt -a 0 -e 90 -s 900 -o arrange-nest-2d.pix \
  arrange-examples.g scene-2d.c
BUILD/bin/pix-png -s 900 arrange-nest-2d.pix > arrange-nest-2d.png

BUILD/bin/rt -a 35 -e 25 -s 900 -o arrange-nest-3d.pix \
  arrange-examples.g scene-3d.c
BUILD/bin/pix-png -s 900 arrange-nest-3d.pix > arrange-nest-3d.png
```

The 2D render uses the gray L shape as the permitted area.  The 3D render uses
a translucent shell outside the permitted ellipsoid so that packed objects
remain visible without adding a competing solid to the nesting volume.
The script's expected reports are 5/5 objects at 61.93% occupied raster fill
for 2D and 7/7 objects at 9.37% for 3D.

## M35 corpus benchmark

`run_m35_benchmark.py` exercises real CSG by selecting every numbered,
non-air region in `m35.g`.  That selects 894 regions in the current model.
Seven (`r135`, `r136`, `r157`, `r173`, `r762`, `r1016`, and `r1017`) have zero
ray-traceable volume even at a 0.01-inch `gqa` grid, so the demonstrated corpus
explicitly excludes them and packs all 887 nonempty physical regions.  A
requested count above 887 cycles through that ordered corpus, modeling repeated
part instances without duplicating their immutable raster data.  Each case
packs into either a convex rectangle or a connected C-shaped concave area.

The timed phase starts a fresh MGED process and includes container creation,
ray-based voxelization through `libanalyze`, orientation preparation, and the
complete packing search.  It uses a dry run so database serialization and
filesystem speed do not contaminate the algorithm measurement.  Counts named
by `--write-counts` are then run once more outside the measurement to retain a
packed database and a renderable `arrange-bench-scene.c` combination.

For example, from the source directory:

```sh
python3 src/libged/arrange/tests/run_m35_benchmark.py \
  --model BUILD/share/db/m35.g --mged BUILD/bin/mged \
  --output-dir /tmp/arrange-m35-benchmark \
  --exclude-regions r135 r136 r157 r173 r762 r1016 r1017

BUILD/bin/rt -a 0 -e 90 -s 1200 -o /tmp/m35-convex.pix \
  /tmp/arrange-m35-benchmark/m35-convex-887.g arrange-bench-scene.c
BUILD/bin/pix-png -s 1200 /tmp/m35-convex.pix > /tmp/m35-convex.png
BUILD/bin/rt -a 0 -e 90 -s 1200 -o /tmp/m35-concave.pix \
  /tmp/arrange-m35-benchmark/m35-concave-887.g arrange-bench-scene.c
BUILD/bin/pix-png -s 1200 /tmp/m35-concave.pix > /tmp/m35-concave.png
```

The default sequence is 10, 100, 887, 1,000, 10,000, and 20,000 instances for
both spaces, with demonstrations retained at 887.  Results are written as CSV
and JSON alongside one diagnostic log per case.  Use the same build, model,
cell size, orientation set, and idle host when comparing code revisions.
The measured runs use a four-inch cell, sampled silhouettes, and zero requested
clearance to isolate scaling of the core raster and search operations.
Retained demonstrations use `--conservative`, which tests complete object
bounding boxes and therefore protects M35 plates only 0.05 inch thick even at
the same coarse cell size.  A one-cell clearance avoids numerical contact, and
the containers are enlarged by 1.5x to account for the intentionally unused
space.  These settings are explicit in the CSV and JSON metadata.
