BRLCAD_ADDEXEC(bundle bundle.c "librt" TEST)
add_test(NAME rt_bundle COMMAND bundle)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")

BRLCAD_ADDEXEC(rt_pattern rt_pattern.c "librt" TEST)
add_test(NAME rt_pattern_5 COMMAND rt_pattern 5)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3")


BRLCAD_ADDEXEC(rt_dvec dvec.cpp "librt" NO_STRICT TEST)
add_test(NAME rt_dvec COMMAND rt_dvec)

BRLCAD_ADDEXEC(rt_ssi ssi.cpp "librt" TEST)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
# add_test(NAME rt_ssi COMMAND rt_ssi)

# NURBS testing
BRLCAD_ADDEXEC(rt_nurbs_tester nurbs_tests.cpp "librt;libbrep;libbu" TEST)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
#add_test(NAME NURBS-get_closest_point-distinct_points nurbs_tester ${CMAKE_CURRENT_SOURCE_DIR}/nurbs_surfaces.g 1)

# diff testing
BRLCAD_ADDEXEC(rt_diff diff.c "librt" TEST)

if(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  BRLCAD_ADDEXEC(rt_binary_attribute binary_attribute.c "librt" TEST)
  add_test(NAME rt_binary_attribute COMMAND rt_binary_attribute)
endif(BRLCAD_ENABLE_BINARY_ATTRIBUTES)

set(distcheck_files
  arb_intersect.g
  brep_boolean_tests.g
  nurbs_surfaces.g
  extreme_ssi_test.g
  binary_attribute.c
  )

CMAKEFILES(${distcheck_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
