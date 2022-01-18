set(nmg_headers
  debug.h
  defines.h
  edge.h
  nurb.h
  ray.h
  vertex.h
  )
BRLCAD_MANAGE_FILES(nmg_headers ${INCLUDE_DIR}/brlcad/nmg)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
