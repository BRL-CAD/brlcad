set(brep_headers
  defines.h
  bbnode.h
  boolean.h
  brnode.h
  cdt.h
  csg.h
  curvetree.h
  intersect.h
  pullback.h
  ray.h
  surfacetree.h
  util.h
  )
BRLCAD_MANAGE_FILES(brep_headers ${INCLUDE_DIR}/brlcad/brep)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
