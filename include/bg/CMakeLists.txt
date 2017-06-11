set(bg_headers
  chull.h
  defines.h
  obr.h
  polygon.h
  tri_ray.h
  tri_tri.h
  trimesh.h
  )

install(FILES ${bg_headers} DESTINATION ${INCLUDE_DIR}/brlcad/bg)

CMAKEFILES(${bg_headers})

CMAKEFILES(pointgen.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
