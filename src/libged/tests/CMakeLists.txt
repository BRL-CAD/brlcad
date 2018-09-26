BRLCAD_ADDEXEC(test_tops test_tops.c libged TEST)
add_test(NAME test_tops_moss COMMAND test_tops "${CMAKE_CURRENT_SOURCE_DIR}/ged_tests.g")

# Note the lint tests are kept in a separate file, since its bad input examples
# stand an excellent chance of breaking commands.
CMAKEFILES(
  lint_tests.g
  ged_tests.g
  )

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

