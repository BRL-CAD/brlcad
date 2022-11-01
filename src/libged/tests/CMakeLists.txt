BRLCAD_ADDEXEC(ged_test_tops test_tops.c libged TEST)
BRLCAD_ADD_TEST(NAME ged_test_tops_moss COMMAND ged_test_tops "${CMAKE_CURRENT_SOURCE_DIR}/ged_tests.g")

BRLCAD_ADDEXEC(ged_test_list test_list.c libged TEST)
BRLCAD_ADD_TEST(NAME ged_test_list COMMAND ged_test_list 2)

BRLCAD_ADDEXEC(ged_test_plugins plugins.cpp libged TEST)
add_dependencies(ged_test_plugins ged_plugins)

BRLCAD_ADDEXEC(ged_test_material test_material.c libged TEST)
BRLCAD_ADD_TEST(NAME ged_test_material COMMAND ged_test_material "${CMAKE_CURRENT_BINARY_DIR}/ged_test_material.g")
set_property(TEST ged_test_material APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})

BRLCAD_ADDEXEC(ged_test_select test_select.c libged TEST)

# Note: it is particularly important that the lint tests in particular are kept
# in a separate file, since its bad input examples stand an excellent chance of
# breaking commands.
CMAKEFILES(
  lint_tests.g
  obb_tests.g
  ged_tests.g
  pnts_in_out.g
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

