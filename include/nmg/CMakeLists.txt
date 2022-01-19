set(nmg_headers
  debug.h
  defines.h
  edge.h
  face.h
  globals.h
  index.h
  isect.h
  loop.h
  model.h
  nurb.h
  plot.h
  print.h
  radial.h
  ray.h
  region.h
  shell.h
  vertex.h
  visit.h
  )
BRLCAD_MANAGE_FILES(nmg_headers ${INCLUDE_DIR}/brlcad/nmg)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
