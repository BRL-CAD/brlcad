
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(distcheck_files
  single_bad_edge.cdtmesh
  cyl_hole.g
  cyls_holes.g
  nurbs_csg.g
  )

BRLCAD_ADDEXEC(brep_cdt_mesh brep_cdt_mesh.cpp "libbrep;libbg;libbn;libbu" TEST)

CMAKEFILES(${distcheck_files})
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
