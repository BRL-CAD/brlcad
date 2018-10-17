set(bg_headers
  chull.h
  clipper.hpp
  defines.h
  obr.h
  polygon.h
  spsr.h
  tri_ray.h
  tri_tri.h
  trimesh.h
  )
BRLCAD_MANAGE_FILES(bg_headers ${INCLUDE_DIR}/brlcad/bg)

CMAKEFILES(pointgen.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
