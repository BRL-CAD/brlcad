set(gm_headers
  defines.h
  chull.h
  obr.h
  polygon.h
  tri_ray.h
  tri_tri.h
  )

install(FILES ${gm_headers} DESTINATION ${INCLUDE_DIR}/brlcad/gm)

CMAKEFILES(${gm_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
