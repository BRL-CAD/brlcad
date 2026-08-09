include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

find_package(Threads REQUIRED)

brlcad_addexec(test_brep_ppx ppx.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_ppx COMMAND test_brep_ppx)

# Replay serialized face-mesh failures without rerunning a complete BRep
# tessellation.  mesh.cpp emits this format at the exact repair failure site,
# making it useful both for diagnosis and for compact regression fixtures.
brlcad_addexec(test_brep_cdt_mesh brep_cdt_mesh.cpp "libbrep" TEST)
brlcad_add_test(
  NAME brep_cdt_mesh_single_bad_edge
  COMMAND test_brep_cdt_mesh "${CMAKE_CURRENT_SOURCE_DIR}/single_bad_edge.cdtmesh"
)

# A cyclic rotation of the same co-circular boundary must not change the CDT
# diagonal merely because its cpolyedge allocations have different addresses.
brlcad_addexec(test_brep_cdt_boundary_start brep_cdt_boundary_start.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_boundary_start COMMAND test_brep_cdt_boundary_start)

# An interior sample exactly on a constrained face boundary must not be passed
# to the triangulator a second time as a Steiner point.
brlcad_addexec(test_brep_cdt_boundary_steiner brep_cdt_boundary_steiner.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_boundary_steiner COMMAND test_brep_cdt_boundary_steiner)

# A closed source solid may not retain redundant open local-remesh islands
# after either retained-solid or same-source-face coverage validation.
brlcad_addexec(test_brep_cdt_spurious_components brep_cdt_spurious_components.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_spurious_components COMMAND test_brep_cdt_spurious_components)

brlcad_addexec(test_brep_cdt_local_defects brep_cdt_local_defects.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_local_defects COMMAND test_brep_cdt_local_defects)

# A conical pole is one chart vertex even though its native rectangular UV
# domain has two periodic seam copies.  Exercise scale, rotation, translation,
# explicit pole identity, and chart/native evaluation round trips.
brlcad_addexec(test_brep_cdt_cone_chart brep_cdt_cone_chart.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_cone_chart COMMAND test_brep_cdt_cone_chart)

# A closed two-pole surface is a diamond after cutting its periodic seam.
# Verify explicit pole identities, chart/native round trips, constrained
# meridians, scale independence, and certified solid assembly.
brlcad_addexec(test_brep_cdt_polar_chart brep_cdt_polar_chart.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_polar_chart COMMAND test_brep_cdt_polar_chart)

# A trimmed cylinder is unwrapped in physical angle-height coordinates.  Keep
# full periodic seams on the native path until both seam copies are explicit.
brlcad_addexec(test_brep_cdt_cylinder_chart brep_cdt_cylinder_chart.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_cylinder_chart COMMAND test_brep_cdt_cylinder_chart)

# Conversion-quality tessellation is a fail-closed transaction: status and
# diagnostics must describe every attempt, tolerance changes invalidate all
# derived state, and rebuilding the same input must produce identical arrays.
brlcad_addexec(test_brep_cdt_contract brep_cdt_contract.cpp "libbrep;librt" TEST)
brlcad_add_test(
  NAME brep_cdt_contract
  COMMAND test_brep_cdt_contract "${CMAKE_CURRENT_SOURCE_DIR}/cyl_hole.g" CUBE.s
)

# Fast display tessellation must be deterministic across worker counts, leave
# its const BRep input untouched, and stop cleanly at configured limits.
brlcad_addexec(test_brep_fast brep_fast.cpp "libbrep;librt" TEST)
brlcad_add_test(
  NAME brep_fast
  COMMAND test_brep_fast "${CMAKE_CURRENT_SOURCE_DIR}/cyl_hole.g" CUBE.s
)
brlcad_add_test(
  NAME brep_fast_closed
  COMMAND test_brep_fast "${CMAKE_CURRENT_SOURCE_DIR}/cyl_hole.g" PEG_ROT_LONG.s
)

# Fast display recovery must retain thin nonlinear loops, reconstruct periodic
# caps and bands, repair malformed pcurves from their 3-D edges, and report
# exact zero-area imports separately from triangulation failures.
brlcad_addexec(test_brep_fast_repair brep_fast_repair.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_fast_repair COMMAND test_brep_fast_repair)

# Standalone CDT hole-triangulation tests: verify that bg_nested_poly_triangulate
# produces no triangles whose centroid falls inside a hole polygon, for two
# faces from the NIST MBE PMI sample that are known to have multiple holes.
brlcad_addexec(test_brep_cdt_hole_face17 brep_cdt_hole_face17.c "libbg;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME brep_cdt_hole_face17 COMMAND test_brep_cdt_hole_face17)

brlcad_addexec(test_brep_cdt_hole_face27 brep_cdt_hole_face27.c "libbg;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME brep_cdt_hole_face27 COMMAND test_brep_cdt_hole_face27)

brlcad_addexec(test_brep_cdt_invalid brep_cdt_invalid.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_cdt_invalid COMMAND test_brep_cdt_invalid)
set_tests_properties(brep_cdt_invalid PROPERTIES TIMEOUT 10)

brlcad_addexec(test_pullback_context pullback_context.cpp "libbrep;Threads::Threads" TEST)
brlcad_add_test(NAME brep_pullback_context COMMAND test_pullback_context)

cmakefiles(
  CMakeLists.txt
  ayam_hyperbolid.3dm
  brep_cdt_invalid.cpp
  brep_cdt_hole_face17.c
  brep_cdt_hole_face27.c
  brep_cdt_boundary_start.cpp
  brep_cdt_boundary_steiner.cpp
  brep_cdt_contract.cpp
  brep_cdt_cone_chart.cpp
  brep_cdt_cylinder_chart.cpp
  brep_cdt_polar_chart.cpp
  brep_cdt_local_defects.cpp
  brep_cdt_spurious_components.cpp
  brep_fast.cpp
  brep_fast_repair.cpp
  brep_cdt_mesh.cpp
  cyl_hole.g
  cyls_holes.g
  freecad_surface.stp
  nurbs_csg.g
  plate_mode_samples.g
  plate_mode_samples_creo.stp
  plate_mode_samples_rhino.stp
  pullback_context.cpp
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
