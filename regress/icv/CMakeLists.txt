# Tests for the various image format conversions supported
# by BRL-CAD

macro(Add_Image_Test testname testtype depends_list)
  CMAKE_PARSE_ARGUMENTS(${testname} "" "INPUT_IMAGE;CONTROL_IMAGE;TARGET_IMAGE;LOG_FILE" "" ${ARGN})
  set(TARGET_NAME ${testname})
  if (NOT "${testtype}" STREQUAL "IMPORT" AND NOT "${testtype}" STREQUAL "EXPORT")
    message(FATAL_ERROR "Valid image test types are IMPORT and EXPORT: got ${testtype}")
  endif (NOT "${testtype}" STREQUAL "IMPORT" AND NOT "${testtype}" STREQUAL "EXPORT")
  set(INPUT_IMAGE ${${testname}_INPUT_IMAGE})
  set(CONTROL_IMAGE ${${testname}_CONTROL_IMAGE})
  set(TARGET_IMAGE ${${testname}_TARGET_IMAGE})
  set(LOG_FILE ${${testname}_LOG_FILE})
  set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${testname}.done")
  set(deps_list pixcmp icv ${depends_list})
  if ("${testtype}" STREQUAL "IMPORT")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-icv_import.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake" @ONLY)
  endif ("${testtype}" STREQUAL "IMPORT")
  if ("${testtype}" STREQUAL "EXPORT")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-icv_export.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake" @ONLY)
  endif ("${testtype}" STREQUAL "EXPORT")
  add_custom_command(
    OUTPUT ${STAMP_FILE}
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake"
    DEPENDS pixcmp icv ${deps_list}
    )

  add_test(NAME regress-icv-${testname} COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-${testname}.cmake")
  add_custom_target(regress-icv-${testname} DEPENDS ${deps_list} ${STAMP_FILE})
  set_target_properties(regress-icv-${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  Add_Regression_Test(icv-${testname} "${deps_list}")

  add_dependencies(regress-icv regress-icv-${testname})

  DISTCLEAN(
    ${STAMP_FILE}
    ${LOG_FILE}
    ${TARGET_IMAGE}
    )
endmacro()

add_custom_target(regress-icv)
set_target_properties(regress-icv PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(regress-icv PROPERTIES FOLDER "BRL-CAD Regression Tests")


Add_Image_Test(pix_pix IMPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_icv.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_pix.log"
  )

Add_Image_Test(png_pix IMPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35.png"
  CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_png.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/png_pix.log"
  )

Add_Image_Test(pix_png EXPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35.png"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_pix.png"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_png.log"
  )

# Unpack compressed ppm file
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35.ppm.tbz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35.ppm")

Add_Image_Test(ppm_pix IMPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.ppm"
  CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_ppm.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/ppm_pix.log"
  )

Add_Image_Test(pix_ppm EXPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.ppm"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_pix.ppm"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_ppm.log"
  )


# Unpack compressed dpix file
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35.dpix.tbz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35.dpix")

Add_Image_Test(dpix_pix IMPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.dpix"
  CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_dpix.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/dpix_pix.log"
  )

Add_Image_Test(pix_dpix EXPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.dpix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_pix.dpix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_dpix.log"
  )


# Unpack compressed bw file and control
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35.bw.tbz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35_bw.pix.tbz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35.bw")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_bw.pix")

Add_Image_Test(bw_pix IMPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.bw"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_bw.pix"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_bw-out.pix"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/bw_pix.log"
  )

Add_Image_Test(pix_bw EXPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.bw"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_pix.bw"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_bw.log"
  )

Add_Image_Test(bw_png EXPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.bw"
  CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_bw.png"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_bw_png.png"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/bw_png.log"
  )

Add_Image_Test(png_bw EXPORT ""
  INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_bw.png"
  CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.bw"
  TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_png_bw.bw"
  LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/png_bw.log"
  )

CMAKEFILES(
  m35.png
  m35_bw.png
  m35_bw.pix.tbz2
  m35.ppm.tbz2
  m35.dpix.tbz2
  m35.bw.tbz2
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
