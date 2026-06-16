# VTK legacy-ASCII importer regression tests.
#
# Each valid ASCII sample is converted to a .g via the gcv tool using the
# shared gcv_regress_util() helper (defined in ../CMakeLists.txt).  The helper
# verifies the conversion ran and produced a reasonably-sized .g file.
#
# Input format is forced to vtk so gcv does not have to guess from the .vtk
# extension (which is shared with several converters).

# gcv_regress_util() derives a single per-format target name (regress-vtk-g),
# so all samples for this format must be registered in ONE call as a list.
# Only samples that the current importer converts to a .g are listed here:
#   points       -> pnts        (POINTS + SCALARS + VECTORS)
#   verts_lines  -> pnts        (POINTS emitted; VERTICES/LINES dropped, Phase 6)
#   mesh         -> bot         (POLYGONS tris + quad)
#   ugrid_cells  -> arb4/arb8   (TETRA + HEXAHEDRON)
#   image        -> vol+binunif (STRUCTURED_POINTS)
# rgrid.vtk (RECTILINEAR_GRID) is not yet implemented and binary.vtk is a
# graceful-reject negative test (no .g produced); both are tracked below but
# not registered as positive conversions.  Size window is widened to cover the
# range from a small pnts/bot .g up to the vol+binunif .g.
gcv_regress_util(
  "vtk"
  "points.vtk;verts_lines.vtk;mesh.vtk;ugrid_cells.vtk;image.vtk"
  GCV_OPTIONS "--input-format=vtk"
  EXPECTED_FILE_SIZE "1500"
  EXPECTED_FILE_SIZE_DELTA "3000"
)

# Expected-failure sample: binary.vtk declares BINARY on its 3rd line and the
# ASCII importer must gracefully REJECT it (no .g produced).
#
# TODO(human): the shared gcv_regress_util() helper has NO expected-failure
# mode -- it always asserts the conversion succeeded and produced a .g of a
# reasonable size, so it CANNOT be used to register binary.vtk as a negative
# test.  binary.vtk is still cmakefiles()'d below so it is tracked and copied,
# but its rejection is not yet wired into ctest.  Two viable approaches; pick
# whichever the maintainers prefer:
#
#   1. Add an EXPECTED_FAILURE option to gcv_regress_util() in
#      ../CMakeLists.txt that inverts the success check (expect non-zero gcv
#      result AND no output .g), then call:
#        gcv_regress_util("vtk" "binary.vtk" GCV_OPTIONS "--input-format=vtk" EXPECTED_FAILURE)
#
#   2. Register a direct add_test() with set_tests_properties(... WILL_FAIL TRUE)
#      that runs $<TARGET_FILE:gcv> --input-format=vtk on binary.vtk.  Note the
#      project convention routes regression tests through
#      BRLCAD_REGRESSION_TEST(); a bespoke add_test() would not join the
#      "regress"/"check" agglomeration targets without extra labeling.
#
# Left unwired pending maintainer confirmation rather than guessing the idiom.

cmakefiles(
  CMakeLists.txt
  binary.vtk
  image.vtk
  image2d.vtk
  mesh.vtk
  points.vtk
  rgrid.vtk
  ugrid_cells.vtk
  verts_lines.vtk
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
