# Tests for the various geometry format conversions supported
# by BRL-CAD

include_directories(${GED_INCLUDE_DIRS})

if(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "-${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "")
endif(CMAKE_CONFIGURATION_TYPES)

macro(ADD_GED_Test testname depends_list)
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress${cfg_testname}-${testname}.log")
  set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${testname}${cfg_testname}.done")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-${testname}.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake" @ONLY)
  add_custom_command(
    OUTPUT "${STAMP_FILE}"
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake"
    DEPENDS ${depends_list}
    )
  add_test(NAME regress-ged-${testname} COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake")
  add_custom_target(regress-ged-${testname} DEPENDS ${depends_list} ${STAMP_FILE})
  set_target_properties(regress-ged-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  Add_Regression_Test(ged-${testname} "${depends_list}")

  add_dependencies(regress-ged regress-ged-${testname})

  DISTCLEAN(
    ${STAMP_FILE}
    ${LOG_FILE}
    ${TARGET_IMAGE}
    )
endmacro()

add_custom_target(regress-ged)
set_target_properties(regress-ged PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress-ged PROPERTIES FOLDER "BRL-CAD Regression Tests")

Add_GED_Test(mater "regress_ged_mater")

CMAKEFILES(
  CMakeLists.txt
  regress-mater.cmake.in
  )

DISTCLEAN(
  "${CMAKE_CURRENT_BINARY_DIR}/regress_ged_mater.done"
  "${CMAKE_CURRENT_BINARY_DIR}/regress_ged_mater.log"
  )
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
