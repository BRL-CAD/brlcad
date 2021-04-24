BRLCAD_REGRESSION_TEST(regress-fastgen "fast4-g;gdiff" EXEC fast4-g)

DISTCLEAN(
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen.log"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen.fast4"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen_unix.g"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen_dos.g"
  )

CMAKEFILES(
  CMakeLists.txt
  fastgen_unix.tbz2
  fastgen_dos.tbz2
  regress-fastgen.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
