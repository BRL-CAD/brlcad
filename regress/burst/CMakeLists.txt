set(BURST_INPUT_FILES
  ktank.b
  ktank_air.ids
  ktank_armor.ids
  ktank_crit.ids
  ktank_colors.ids
  )

if (TARGET ktank.g)
  BRLCAD_REGRESSION_TEST(regress-burst "burst;ktank.g" EXEC burst)
  BRLCAD_REGRESSION_TEST(regress-nburst "nburst;ktank.g" EXEC nburst)

  set(BURST_CLEAN_FILES
    ktank_burst.plot
    ktank_burst.pix
    ktank.burst
    ktank.g
    ktank_burst.shotlines
    burst_ktank.log
    regress-burst.log
    regress-nburst.log
    )
  DISTCLEAN(${BURST_CLEAN_FILES})

  # If we copied the input files, clear them out as well
  if(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
    DISTCLEAN(${BURST_INPUT_FILES})
  endif(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
endif (TARGET ktank.g)

CMAKEFILES(
  CMakeLists.txt
  echo.b
  regress-burst.cmake.in
  regress-nburst.cmake.in
  ${BURST_INPUT_FILES}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
