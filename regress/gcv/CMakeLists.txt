# Tests for the various geometry format conversions supported
# by BRL-CAD

if(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "-${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(cfg_testname "")
endif(CMAKE_CONFIGURATION_TYPES)

macro(ADD_GCV_Test testname depends_list)
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/regress${cfg_testname}-${testname}.log")
  set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${testname}${cfg_testname}.done")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-${testname}.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake" @ONLY)
  add_custom_command(
    OUTPUT "${STAMP_FILE}"
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake"
    DEPENDS ${depends_list}
    )
  add_test(NAME regress-gcv-${testname} COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake")
  add_custom_target(regress-gcv-${testname} DEPENDS ${depends_list} ${STAMP_FILE})
  set_target_properties(regress-gcv-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  Add_Regression_Test(gcv-${testname} "${depends_list}")

  add_dependencies(regress-gcv regress-gcv-${testname})

  DISTCLEAN(
    ${STAMP_FILE}
    ${LOG_FILE}
    ${TARGET_IMAGE}
    )
endmacro()

add_custom_target(regress-gcv)
set_target_properties(regress-gcv PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress-gcv PROPERTIES FOLDER "BRL-CAD Regression Tests")

Add_GCV_Test(fastgen "fast4-g;gdiff")
DISTCLEAN(
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen.done"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen.log"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen_unix.g"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen_dos.g"
  )
CMAKEFILES(
  fastgen_dos.fast4 
  fastgen_box.fast4 
  )


# The dem test is a bit more one-off for now...
add_subdirectory(dem)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
