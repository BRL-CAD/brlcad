
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

BRLCAD_ADDEXEC(test_brep_ppx ppx.cpp "libbrep"  NO_INSTALL)
BRLCAD_ADD_TEST(NAME brep_ppx COMMAND test_brep_ppx)

CMAKEFILES(
  CMakeLists.txt
  ayam_hyperbolid.3dm
  brep_cdt_mesh.cpp
  cyl_hole.g
  cyls_holes.g
  freecad_surface.stp
  nurbs_csg.g
  plate_mode_samples.g
  plate_mode_samples_creo.stp
  plate_mode_samples_rhino.stp
  single_bad_edge.cdtmesh
  test_curve_intersect.cpp
  test_point_intersect.cpp
  vsp.stp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
