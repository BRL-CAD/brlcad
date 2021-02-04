BRLCAD_ADDEXEC(ged_test_tops test_tops.c libged TEST)
BRLCAD_ADD_TEST(NAME ged_test_tops_moss COMMAND ged_test_tops "${CMAKE_CURRENT_SOURCE_DIR}/ged_tests.g")

BRLCAD_ADDEXEC(ged_test_list test_list.c libged TEST)
BRLCAD_ADD_TEST(NAME ged_test_list COMMAND ged_test_list 2)

BRLCAD_ADDEXEC(ged_test_plugins plugins.cpp libged TEST)

# Tests of push functionality
BRLCAD_ADDEXEC(ged_test_push push.cpp libged TEST_USESDATA)
BRLCAD_ADD_TEST(NAME ged_test_push COMMAND ged_test_push "${CMAKE_CURRENT_SOURCE_DIR}/ppush_tests.g" "${CMAKE_CURRENT_SOURCE_DIR}/push_tests.g")
BRLCAD_ADD_TEST(NAME ged_test_xpush COMMAND ged_test_push -f "${CMAKE_CURRENT_SOURCE_DIR}/xpush_tests.g" "${CMAKE_CURRENT_SOURCE_DIR}/push_tests.g")
BRLCAD_ADD_TEST(NAME ged_test_local_xpush COMMAND ged_test_push -L -f "${CMAKE_CURRENT_SOURCE_DIR}/xpush_tests.g" "${CMAKE_CURRENT_SOURCE_DIR}/push_tests.g")

# Note: it is particularly important that the lint tests in particular are kept
# in a separate file, since its bad input examples stand an excellent chance of
# breaking commands.
CMAKEFILES(
  lint_tests.g
  obb_tests.g
  ged_tests.g
  pnts_in_out.g
  push_tests.g
  ppush_tests.g
  xpush_tests.g
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

