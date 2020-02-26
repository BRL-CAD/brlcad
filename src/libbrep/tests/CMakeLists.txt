
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(distcheck_files
  ayam_hyperbolid.3dm
  single_bad_edge.cdtmesh
  plate_mode_samples.g
  plate_mode_samples_rhino.stp
  plate_mode_samples_creo.stp
  cyl_hole.g
  cyls_holes.g
  freecad_surface.stp
  nurbs_csg.g
  vsp.stp
  )

BRLCAD_ADDEXEC(brep_cdt_mesh brep_cdt_mesh.cpp "libbrep;libbg;libbn;libbu" TEST)

BRLCAD_ADDEXEC(test_point_intersect test_point_intersect.cpp "libbrep" NO_STRICT NO_INSTALL)

BRLCAD_ADDEXEC(test_curve_intersect test_curve_intersect.cpp "libbrep" NO_STRICT NO_INSTALL)

CMAKEFILES(${distcheck_files})
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
