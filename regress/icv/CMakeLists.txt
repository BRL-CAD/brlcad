# Tests for the various image format conversions supported
# by BRL-CAD

# All image conversions with icv use one of two templates - import or export:
set(TIMPORT "${CMAKE_CURRENT_SOURCE_DIR}/regress-icv_import.cmake.in")
set(TEXPORT "${CMAKE_CURRENT_SOURCE_DIR}/regress-icv_export.cmake.in")

# pix-pix
set(INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_icv.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_pix.log")
BRLCAD_REGRESSION_TEST(regress-pix_pix "pixcmp;icv" TEST_SCRIPT "${TIMPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# png-pix
set(INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35.png")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_png.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/png_pix.log")
BRLCAD_REGRESSION_TEST(regress-png_pix "pixcmp;icv" TEST_SCRIPT "${TIMPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# pix-png
set(INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35.png")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_pix.png")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_png.log")
set(TARGET_NAME pix_png)
bRLCAD_REGRESSION_TEST(regress-pix_png "pixcmp;icv" TEST_SCRIPT "${TEXPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# Unpack compressed ppm file
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35.ppm.tbz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35.ppm")

# ppm_pix
set(INPUT_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.ppm")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_ppm.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/ppm_pix.log")
BRLCAD_REGRESSION_TEST(regress-ppm_pix "pixcmp;icv" TEST_SCRIPT "${TIMPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# pix_ppm
set(INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.ppm")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_pix.ppm")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_ppm.log")
set(TARGET_NAME pix_ppm)
BRLCAD_REGRESSION_TEST(regress-pix_ppm "pixcmp;icv" TEST_SCRIPT "${TEXPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# Unpack compressed dpix file
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35.dpix.tbz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35.dpix")

# dpix_pix
set(INPUT_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.dpix")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_dpix.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/dpix_pix.log")
BRLCAD_REGRESSION_TEST(regress-dpix_pix "pixcmp;icv" TEST_SCRIPT "${TIMPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# pix_dpix
set(INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.dpix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_pix.dpix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_dpix.log")
set(TARGET_NAME pix_dpix)
BRLCAD_REGRESSION_TEST(regress-pix_dpix "pixcmp;icv" TEST_SCRIPT "${TEXPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# Unpack compressed bw file and control
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35.bw.tbz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xjf "${CMAKE_CURRENT_SOURCE_DIR}/m35_bw.pix.tbz2" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35.bw")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/m35_bw.pix")

# bw_pix
set(INPUT_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.bw")
set(CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_bw.pix")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_bw-out.pix")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/bw_pix.log")
set(TARGET_NAME bw_pix)
BRLCAD_REGRESSION_TEST(regress-bw_pix "pixcmp;icv" TEST_SCRIPT "${TIMPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# pix_bw
set(INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/ref/m35.pix")
set(CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.bw")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_pix.bw")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/pix_bw.log")
set(TARGET_NAME pix_bw)
BRLCAD_REGRESSION_TEST(regress-pix_bw "pixcmp;icv" TEST_SCRIPT "${TEXPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# bw_png
set(INPUT_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.bw")
set(CONTROL_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_bw.png")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_bw_png.png")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/bw_png.log")
set(TARGET_NAME bw_png)
BRLCAD_REGRESSION_TEST(regress-bw_png "pixcmp;icv" TEST_SCRIPT "${TEXPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

# png_bw
set(INPUT_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/m35_bw.png")
set(CONTROL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35.bw")
set(TARGET_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/m35_png_bw.bw")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/png_bw.log")
set(TARGET_NAME png_bw)
BRLCAD_REGRESSION_TEST(regress-png_bw "pixcmp;icv" TEST_SCRIPT "${TEXPORT}" EXEC icv)
DISTCLEAN(${LOG_FILE} ${TARGET_IMAGE})

CMAKEFILES(
  CMakeLists.txt
  m35.png
  m35_bw.png
  m35_bw.pix.tbz2
  m35.ppm.tbz2
  m35.dpix.tbz2
  m35.bw.tbz2
  regress-icv_export.cmake.in
  regress-icv_import.cmake.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
