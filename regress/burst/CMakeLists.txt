set(BURST_INPUT_FILES
  ktank.b
  ktank_air.ids
  ktank_armor.ids
  ktank_crit.ids
  )

BRLCAD_REGRESSION_TEST(regress-burst "burst;ktank.g" EXEC burst)

set(BURST_CLEAN_FILES
  ktank_burst.plot
  ktank_burst.pix
  ktank.burst
  ktank.g
  ktank_burst.shotlines
  burst_ktank.log
  )
foreach(BCF ${BURST_CLEAN_FILES})
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${BCF}")
endforeach(BCF ${BURST_CLEAN_FILES})
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/regress-burst.log")

# If we copied the input files, clear them out as well
if(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  foreach(BIF ${BURST_INPUT_FILES})
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${BCF}")
  endforeach(BIF ${BURST_INPUT_FILES})
endif(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")

CMAKEFILES(
  CMakeLists.txt
  ${BURST_INPUT_FILES}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
