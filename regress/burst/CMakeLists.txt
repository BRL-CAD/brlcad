if(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "-${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "")
endif(CMAKE_CONFIGURATION_TYPES)

set(BURST_INPUT_FILES
  ktank.b
  ktank_air.ids
  ktank_armor.ids
  ktank_crit.ids
  )

set(BURST_CLEAN_FILES
  ktank_burst.plot
  ktank_burst.pix
  ktank.burst
  ktank.g
  ktank_burst.shotlines
  burst_ktank.log
  )

set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress${cfg_testname}-burst-ktank.log")
set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/burst-ktank${cfg_testname}.done")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-burst-ktank.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-burst-ktank.cmake" @ONLY)
add_custom_command(
  OUTPUT "${STAMP_FILE}"
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-burst-ktank.cmake"
  DEPENDS ${depends_list}
  )
add_test(NAME regress-burst-ktank COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-burst-ktank.cmake")
add_custom_target(regress-burst-ktank DEPENDS ktank.g burst "${STAMP_FILE}")
set_target_properties(regress-burst-ktank PROPERTIES FOLDER "BRL-CAD Regression Tests")
Add_Regression_Test(burst-ktank "ktank.g;burst")

CMAKEFILES(
  CMakeLists.txt
  ${BURST_INPUT_FILES}
  )

DISTCLEAN(
  ${STAMP_FILE}
  ${LOG_FILE}
  ${TARGET_IMAGE}
  )
foreach(BCF ${BURST_CLEAN_FILES})
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${BCF}")
endforeach(BCF ${BURST_CLEAN_FILES})
if(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  foreach(BIF ${BURST_INPUT_FILES})
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${BCF}")
  endforeach(BIF ${BURST_INPUT_FILES})
endif(NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
