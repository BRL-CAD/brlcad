add_subdirectory(primitives)

set(rt_headers
  anim.h
  application.h
  arb_edit.h
  binunif.h
  boolweave.h
  calc.h
  cmd.h
  comb.h
  db4.h
  db5.h
  db_attr.h
  db_diff.h
  db_fullpath.h
  db_instance.h
  db_internal.h
  db_io.h
  debug.h
  defines.h
  directory.h
  dspline.h
  func.h
  functab.h
  geom.h
  global.h
  hit.h
  htbl.h
  mater.h
  misc.h
  mem.h
  nmg_conv.h
  nongeom.h
  op.h
  overlap.h
  pattern.h
  piece.h
  prep.h
  private.h
  ray_partition.h
  region.h
  resource.h
  rt_instance.h
  search.h
  seg.h
  shoot.h
  soltab.h
  space_partition.h
  tie.h
  timer.h
  tol.h
  tree.h
  uv.h
  version.h
  view.h
  vlist.h
  wdb.h
  xray.h
  )
BRLCAD_MANAGE_FILES(rt_headers ${INCLUDE_DIR}/brlcad/rt)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
