# Tests for the various image format conversions supported
# by BRL-CAD

macro(Add_RtWiz_Test testname input_file options depends_list)
  CMAKE_PARSE_ARGUMENTS(${testname} "" "CONTROL_IMAGE;TARGET_IMAGE;LOG_FILE" "" ${ARGN})
  set(TARGET_NAME ${testname})
  set(OPTIONS ${options})
  set(INPUT_FILE ${input_file})
  set(CONTROL_IMAGE ${${testname}_CONTROL_IMAGE})
  set(TARGET_IMAGE ${${testname}_TARGET_IMAGE})
  set(LOG_FILE ${${testname}_LOG_FILE})
  set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${testname}.done")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-rtwiz.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake" @ONLY)
  add_custom_command(
    OUTPUT ${STAMP_FILE}
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake"
    DEPENDS pixcmp rtwizard ${depends_list}
    )

  add_test(NAME regress-rtwizard-${testname} COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake")
  add_custom_target(regress-rtwizard-${testname} DEPENDS ${depends_list} ${STAMP_FILE})
  set_target_properties(regress-rtwizard-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  Add_Regression_Test(rtwizard-${testname} "${depends_list}")

  add_dependencies(regress-rtwizard regress-rtwizard-${testname})

  DISTCLEAN(
    ${STAMP_FILE}
    ${LOG_FILE}
    ${TARGET_IMAGE}
    )
endmacro()

add_custom_target(regress-rtwizard)
set_target_properties(regress-rtwizard PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress-rtwizard PROPERTIES FOLDER "BRL-CAD Regression Tests")

# Unpack compressed reference images
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35_rtwiz.tar.bz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_A_ref.pix")

Add_RtWiz_Test(rtwiz_m35_A "${CMAKE_BINARY_DIR}/${DATA_DIR}/db/m35.g" "all.g --benchmark -s 512" "m35.g"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_A_ref.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_A.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_A.log"
  )


CMAKEFILES(
  m35_rtwiz.tar.bz2
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
