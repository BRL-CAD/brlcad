BRLCAD_ADDEXEC(bundle bundle.c "librt" TEST)
BRLCAD_ADD_TEST(NAME rt_bundle COMMAND bundle)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")

BRLCAD_ADDEXEC(rt_pattern rt_pattern.c "librt" TEST)
BRLCAD_ADD_TEST(NAME rt_pattern_5 COMMAND rt_pattern 5)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3")


BRLCAD_ADDEXEC(rt_dvec dvec.cpp "librt" NO_STRICT TEST)
BRLCAD_ADD_TEST(NAME rt_dvec COMMAND rt_dvec)

BRLCAD_ADDEXEC(rt_ssi ssi.cpp "librt" TEST)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
# BRLCAD_ADD_TEST(NAME rt_ssi COMMAND rt_ssi)

# NURBS testing
BRLCAD_ADDEXEC(rt_nurbs_tester nurbs_tests.cpp "librt;libbrep;libbu" TEST)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
#BRLCAD_ADD_TEST(NAME NURBS-get_closest_point-distinct_points nurbs_tester ${CMAKE_CURRENT_SOURCE_DIR}/nurbs_surfaces.g 1)

# diff testing
BRLCAD_ADDEXEC(rt_diff diff.c "librt" TEST)

if(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  BRLCAD_ADDEXEC(rt_binary_attribute binary_attribute.c "librt" TEST)
  BRLCAD_ADD_TEST(NAME rt_binary_attribute COMMAND rt_binary_attribute)
endif(BRLCAD_ENABLE_BINARY_ATTRIBUTES)

# size testing
BRLCAD_ADDEXEC(db5_size db5_size.c "librt" TEST)

# datum testing
BRLCAD_ADDEXEC(rt_datum rt_datum.c "librt" TEST)

# cyclic path testing
BRLCAD_ADDEXEC(rt_cyclic cyclic.c "librt" TEST)
BRLCAD_ADD_TEST(NAME rt_cyclic_basic COMMAND rt_cyclic ${CMAKE_CURRENT_SOURCE_DIR}/cyclic_tests.g)

BRLCAD_ADDEXEC(rt_cache cache.cpp "librt" TEST)
BRLCAD_ADD_TEST(NAME rt_cache_serial_single_object COMMAND rt_cache 1)
BRLCAD_ADD_TEST(NAME rt_cache_parallel_single_object COMMAND rt_cache 2)
BRLCAD_ADD_TEST(NAME rt_cache_serial_multiple_identical_objects COMMAND rt_cache 3 10)
BRLCAD_ADD_TEST(NAME rt_cache_parallel_multiple_identical_objects  COMMAND rt_cache 4 10)
BRLCAD_ADD_TEST(NAME rt_cache_serial_multiple_different_objects COMMAND rt_cache 5 10)
BRLCAD_ADD_TEST(NAME rt_cache_parallel_multiple_different_objects  COMMAND rt_cache 6 10)
BRLCAD_ADD_TEST(NAME rt_cache_parallel_multiple_different_objects_hierarchy_1  COMMAND rt_cache 7 10)

set(distcheck_files
  CMakeLists.txt
  arb_intersect.g
  brep_boolean_tests.g
  cyclic_tests.g
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
