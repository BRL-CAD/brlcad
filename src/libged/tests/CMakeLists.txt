BRLCAD_ADDEXEC(ged_test_tops test_tops.c libged TEST)
add_test(NAME ged_test_tops_moss COMMAND ged_test_tops "${CMAKE_CURRENT_SOURCE_DIR}/ged_tests.g")

BRLCAD_ADDEXEC(ged_test_mater mater.c libged TEST)

# Note: it is particularly important that the lint tests in particular are kept
# in a separate file, since its bad input examples stand an excellent chance of
# breaking commands.
CMAKEFILES(
  lint_tests.g
  obb_tests.g
  ged_tests.g
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

