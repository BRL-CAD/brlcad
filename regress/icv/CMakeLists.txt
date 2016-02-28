# Tests for the various image format conversions supported
# by BRL-CAD

macro(Add_Image_Test testname depends_list)
  CMAKE_PARSE_ARGUMENTS(${testname} "" "INPUT_IMAGE;CONTROL_IMAGE;TARGET_IMAGE;LOG_FILE" "" ${ARGN})
  set(INPUT_IMAGE ${${testname}_INPUT_IMAGE})
  set(CONTROL_IMAGE ${${testname}_CONTROL_IMAGE})
  set(TARGET_IMAGE ${${testname}_TARGET_IMAGE})
  set(LOG_FILE ${${testname}_LOG_FILE})
  set(STAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/${testname}.done)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/regress-icv.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake @ONLY)
  add_custom_command(
    OUTPUT ${STAMP_FILE}
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake
    DEPENDS pixcmp icv ${depends_list} ${INPUT_IMAGE} ${CONTROL_IMAGE}
    )

  add_test(NAME regress-${testname} COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake)
  add_custom_target(regress-${testname} DEPENDS ${depends_list} ${STAMP_FILE})
  set_target_properties(regress-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  Add_Regression_Test(${testname} "${depends_list}")

  DISTCLEAN(
    ${STAMP_FILE}
    ${LOG_FILE}
    ${CMAKE_CURRENT_BINARY_DIR}/${testname}.done
    )
endmacro()

Add_Image_Test(pix_pix ""
  INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bench/ref/m35.pix"
  CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bench/ref/m35.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_icv.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/m35_icv.log"
  )


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
