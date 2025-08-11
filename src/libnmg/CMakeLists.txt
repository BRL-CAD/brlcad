set(
  LIBNMG_SOURCES
  nurb_basis.c
  nurb_bezier.c
  nurb_bound.c
  nurb_c2.c
  nurb_copy.c
  nurb_diff.c
  nurb_eval.c
  nurb_flat.c
  nurb_interp.c
  nurb_knot.c
  nurb_norm.c
  nurb_oslo_calc.c
  nurb_oslo_map.c
  nurb_plot.c
  nurb_poly.c
  nurb_ray.c
  nurb_refine.c
  nurb_reverse.c
  nurb_solve.c
  nurb_split.c
  nurb_tess.c
  nurb_trim.c
  nurb_trim_util.c
  nurb_util.c
  nurb_xsplit.c
  bool.c
  ck.c
  class.c
  copy.c
  eval.c
  extrude.c
  fcut.c
  fuse.c
  index.c
  info.c
  inter.c
  io.c
  manif.c
  mesh.c
  misc.c
  mk.c
  mod.c
  plot.c
  pr.c
  pt_fu.c
  isect.c
  tri.c
  visit.c
)

cmakefiles(
  CMakeLists.txt
  junk.c
  nmg_private.h
)

# Note - libnmg_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
brlcad_addlib(libnmg "${LIBNMG_SOURCES}" "${libnmg_deps};${STDCXX_LIBRARIES}" "" "")

add_subdirectory(tests)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
