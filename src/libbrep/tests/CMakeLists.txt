include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

brlcad_addexec(test_brep_ppx ppx.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_ppx COMMAND test_brep_ppx)

# Standalone CDT hole-triangulation tests: verify that bg_nested_poly_triangulate
# produces no triangles whose centroid falls inside a hole polygon, for two
# faces from the NIST MBE PMI sample that are known to have multiple holes.
brlcad_addexec(test_brep_cdt_hole_face17 brep_cdt_hole_face17.c "libbg;libbu;m" TEST)
brlcad_add_test(NAME brep_cdt_hole_face17 COMMAND test_brep_cdt_hole_face17)

brlcad_addexec(test_brep_cdt_hole_face27 brep_cdt_hole_face27.c "libbg;libbu;m" TEST)
brlcad_add_test(NAME brep_cdt_hole_face27 COMMAND test_brep_cdt_hole_face27)

cmakefiles(
  CMakeLists.txt
  ayam_hyperbolid.3dm
  brep_cdt_hole_face17.c
  brep_cdt_hole_face27.c
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
