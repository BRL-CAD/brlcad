if(TARGET librt AND BUILD_TESTING)
  brlcad_addexec(rt_primitives rt_primitives.c "librt;libbu;libbn;${M_LIBRARY}" NO_INSTALL)

  # Registers only once primitives.g (generated from db/primitives.asc) exists
  # as a target; runs rt_primitives against the built share/db/primitives.g.
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-rt_primitives.log")
  brlcad_regression_test(regress-rt_primitives "rt_primitives;primitives.g" EXEC rt_primitives)
  distclean("${LOG_FILE}")
endif(TARGET librt AND BUILD_TESTING)

cmakefiles(
  CMakeLists.txt
  rt_primitives.c
  rt_report.h
  regress-rt_primitives.cmake.in
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
