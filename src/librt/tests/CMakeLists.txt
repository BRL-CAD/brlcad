BRLCAD_ADDEXEC(test_bundle test_bundle.c "librt" NO_INSTALL)
add_test(NAME rt_test_bundle COMMAND test_bundle)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")

BRLCAD_ADDEXEC(rt_pattern rt_pattern.c "librt" NO_INSTALL)
add_test(NAME rt_pattern_5 COMMAND rt_pattern 5)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3")


BRLCAD_ADDEXEC(test_dvec test_dvec.cpp "librt" NO_STRICT NO_INSTALL)
add_test(NAME rt_test_dvec COMMAND test_dvec)

BRLCAD_ADDEXEC(test_ssi test_ssi.cpp "librt" NO_INSTALL)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
# add_test(NAME test_ssi COMMAND test_ssi)

# NURBS testing
BRLCAD_ADDEXEC(nurbs_tester nurbs_tests.cpp "librt;libbrep;libbu" NO_INSTALL)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
#add_test(NAME NURBS-get_closest_point-distinct_points nurbs_tester ${CMAKE_CURRENT_SOURCE_DIR}/nurbs_surfaces.g 1)

# diff testing
BRLCAD_ADDEXEC(test_diff test_diff.c "librt" NO_INSTALL)

# nmg_mk testing
BRLCAD_ADDEXEC(test_nmg_mk test_nmg_mk.c "librt" NO_INSTALL)
add_test(NAME rt_test_nmg_mk COMMAND test_nmg_mk)

# nmg_copy testing
BRLCAD_ADDEXEC(test_nmg_copy test_nmg_copy.c "librt" NO_INSTALL)
add_test(NAME rt_test_nmg_copy COMMAND test_nmg_copy)

set(distcheck_files
  arb_intersect.g
  brep_boolean_tests.g
  nurbs_surfaces.g
  extreme_ssi_test.g
  )

CMAKEFILES(${distcheck_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
