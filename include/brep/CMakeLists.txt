set(brep_headers
  defines.h
  bbnode.h
  boolean.h
  brnode.h
  csg.h
  curvetree.h
  intersect.h
  pullback.h
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
