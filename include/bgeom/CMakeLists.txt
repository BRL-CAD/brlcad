set(bgeom_headers
  defines.h
  tri_ray.h
  tri_tri.h
  )

install(FILES ${bgeom_headers} DESTINATION ${INCLUDE_DIR}/brlcad/bgeom)

CMAKEFILES(${bgeom_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
