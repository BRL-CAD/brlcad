set(rt_headers
  arb_edit.h
  db_diff.h
  db_fullpath.h
  db_instance.h
  db_internal.h
  debug.h
  defines.h
  directory.h
  hit.h
  mater.h
  mem.h
  op.h
  pattern.h
  ray_partition.h
  region.h
  search.h
  seg.h
  soltab.h
  space_partition.h
  tol.h
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
