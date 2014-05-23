BRLCAD_ADDEXEC(test_bundle test_bundle.c "librt" NO_INSTALL)
add_test(test_bundle test_bundle)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES bundle.plot3)

BRLCAD_ADDEXEC(test_dvec test_dvec.cpp "librt" NO_STRICT NO_INSTALL)
add_test(test_dvec test_dvec)

BRLCAD_ADDEXEC(test_ssi test_ssi.cpp "librt" NO_INSTALL)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
# add_test(test_ssi test_ssi)

# NURBS testing
BRLCAD_ADDEXEC(nurbs_tester nurbs_tests.cpp "librt;libbrep;libbu" NO_INSTALL)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
#add_test(NURBS-get_closest_point-distinct_points nurbs_tester ${CMAKE_CURRENT_SOURCE_DIR}/nurbs_surfaces.g 1)

# diff testing
#BRLCAD_ADDEXEC(test_diff test_diff.c "librt" NO_INSTALL)

# nmg_mk testing
BRLCAD_ADDEXEC(test_nmg_mk test_nmg_mk.c "librt" NO_INSTALL)
add_test(test_nmg_mk test_nmg_mk)

# nmg_copy testing
BRLCAD_ADDEXEC(test_nmg_copy test_nmg_copy.c "librt" NO_INSTALL)
add_test(test_nmg_copy test_nmg_copy)

# dirbuild test
BRLCAD_ADDEXEC(test_dirbuild test_dirbuild.c "librt" NO_INSTALL)
add_test(test_dirbuild test_dirbuild ${CMAKE_CURRENT_SOURCE_DIR}/dirbuild-test-1.g ${CMAKE_CURRENT_SOURCE_DIR}/dirbuild-test-0.g ${CMAKE_CURRENT_SOURCE_DIR}/dirbuild-test-2.g)

set(distcheck_files
  arb_intersect.g
  brep_boolean_tests.g
  nurbs_surfaces.g
  extreme_ssi_test.g
  dirbuild-test-0.g
  dirbuild-test-1.g
  dirbuild-test-2.g
  test_diff.c
  )

CMAKEFILES(${distcheck_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
