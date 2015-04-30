set(rt_headers
  anim.h
  application.h
  arb_edit.h
  db_diff.h
  db_fullpath.h
  db_instance.h
  db_internal.h
  debug.h
  defines.h
  directory.h
  functab.h
  hit.h
  mater.h
  mem.h
  nongeom.h
  op.h
  pattern.h
  piece.h
  ray_partition.h
  region.h
  resource.h
  rt_instance.h
  search.h
  seg.h
  soltab.h
  space_partition.h
  tol.h
  tree.h
  view.h
  wdb.h
  xray.h
  )

install(FILES ${rt_headers} DESTINATION ${INCLUDE_DIR}/brlcad/rt)

CMAKEFILES(${rt_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
