if (BRLCAD_ENABLE_TCL)
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress-gchecker.log")
  set(GCHECKER_CLEAN_FILE ${CMAKE_CURRENT_BINARY_DIR}/gchecker_test.g.ck/ck.gchecker_test.g.overlaps)

  BRLCAD_REGRESSION_TEST(regress-gchecker "gchecker;mged;gqa;rtcheck" EXEC gchecker)

  DISTCLEAN(${LOG_FILE})
  DISTCLEAN(${GCHECKER_CLEAN_FILE})
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  regress-gchecker.cmake.in
  gchecker_test.g
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
