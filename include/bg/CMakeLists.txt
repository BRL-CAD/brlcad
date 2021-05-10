set(bg_headers
  aabb_ray.h
  chull.h
  clip.h
  defines.h
  lseg.h
  obr.h
  plane.h
  polygon.h
  polygon_types.h
  spsr.h
  tri_pt.h
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
