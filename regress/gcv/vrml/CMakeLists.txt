BRLCAD_REGRESSION_TEST(regress-vrml "gcv;gcv-vrml" EXEC gcv)

DISTCLEAN(
  "${CMAKE_CURRENT_BINARY_DIR}/regress-vrml.log"
  "${CMAKE_CURRENT_BINARY_DIR}/vrml_example.g"
  )

CMAKEFILES(
  CMakeLists.txt
  vrml_example.wrl
  regress-vrml.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
