set(
  bg_headers
  aabb_ray.h
  chull.h
  clip.h
  defines.h
  lseg.h
  obr.h
  plane.h
  polygon.h
  polygon_types.h
  sat.h
  spsr.h
  tri_pt.h
  tri_ray.h
  tri_tri.h
  trimesh.h
  vert_tree.h
)
brlcad_manage_files(bg_headers ${INCLUDE_DIR}/brlcad/bg REQUIRED libbg)

cmakefiles(pointgen.h)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
