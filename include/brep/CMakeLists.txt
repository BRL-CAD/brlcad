set(brep_headers
  defines.h
  bbnode.h
  brnode.h
  curvetree.h
  ray.h
  surfacetree.h
  util.h
  )

install(FILES ${brep_headers} DESTINATION ${INCLUDE_DIR}/brlcad/brep)

CMAKEFILES(${brep_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
