brlcad_regression_test(regress-fastgen "fast4-g;gdiff" EXEC fast4-g)

# Test fast4-g on the Business_Jet.fg4 FASTGEN v4 model (i.e. a real world
# file) under a bounded timeout and verify a non-empty .g is produced.
brlcad_regression_test(regress-fastgen-faa "fast4-g" EXEC fast4-g)

distclean(
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen.log"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen.fast4"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen_unix.g"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen_dos.g"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen-faa.log"
  "${CMAKE_CURRENT_BINARY_DIR}/Business_Jet.g"
)

cmakefiles(
  CMakeLists.txt
  fastgen_unix.tbz2
  fastgen_dos.tbz2
  regress-fastgen.cmake.in
  regress-fastgen-faa.cmake.in
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
