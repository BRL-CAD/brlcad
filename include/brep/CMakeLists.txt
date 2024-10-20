set(
  brep_headers
  defines.h
  bbnode.h
  boolean.h
  brnode.h
  cdt.h
  csg.h
  curvetree.h
  edit.h
  intersect.h
  pullback.h
  ray.h
  surfacetree.h
  util.h
)
brlcad_manage_files(brep_headers ${INCLUDE_DIR}/brlcad/brep REQUIRED libbrep)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
