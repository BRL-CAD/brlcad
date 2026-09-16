# OpenSCAD (.scad) importer regression tests.
#
# Each sample .scad program is converted to a .g via the gcv tool using the
# shared gcv_regress_util() helper (defined in ../CMakeLists.txt), which
# verifies the conversion ran and produced a reasonably-sized .g file.
#
# The input format is forced to scad so gcv does not have to guess it.
#
# gcv_regress_util() derives a single per-format target (regress-scad-g), so
# all samples must be registered in ONE call as a list:
#   primitives -> cube/sphere/cylinder/cone, 2D polygon + linear_extrude,
#                 translate/rotate/scale, union/difference/intersection, color
#   control    -> module defaults, if/else, for [a:step:b], hull, mirror,
#                 modifiers, faceted $fn=6 prism
#   functions  -> recursion, ternary, let, list comprehension, swizzles,
#                 sin/cos/len/concat/norm/sqrt builtins
#   children   -> operator modules, children()/$children, nested for
#
# Converted sizes range from ~2.3 KB to ~6.3 KB, so the size window is widened
# accordingly.
gcv_regress_util(
  "scad"
  "primitives.scad;control.scad;functions.scad;children.scad"
  GCV_OPTIONS "--input-format=scad"
  EXPECTED_FILE_SIZE "4200"
  EXPECTED_FILE_SIZE_DELTA "2400"
)

cmakefiles(
  CMakeLists.txt
  primitives.scad
  control.scad
  functions.scad
  children.scad
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
