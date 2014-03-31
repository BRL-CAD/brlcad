configure_file(${CMAKE_CURRENT_SOURCE_DIR}/eval_conversion.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/eval_conv.cmake COPY_ONLY)

macro(STEP_G_TEST testnum_in filename_in md5sum_in comment_in)

  set(testnum ${testnum_in})
  set(filename ${filename_in})
  set(md5_sum ${md5sum_in})
  set(comment ${comment_in})
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ap203_download.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}_download.cmake)

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}.stp
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}_download.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ap203_download.cmake.in
    )

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/step_g_${testnum}.passed
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}.g
    COMMAND step-g -o ${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}.g ${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}.stp
    COMMAND ${CMAKE_COMMAND} -DTEST_NUM=${testnum}
    -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}.g -DMD5_SUM=${CMAKE_CURRENT_SOURCE_DIR}/test_${testnum}.md5 -P ${CMAKE_CURRENT_SOURCE_DIR}/eval_conv.cmake
    DEPENDS step-g
    ${CMAKE_CURRENT_SOURCE_DIR}/eval_conversion.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/test_${testnum}.md5
    ${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}.stp
    )
  add_custom_target(step_g_ap203_${testnum} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/step_g_${testnum}.passed)
  set_target_properties(step_g_ap203_${testnum} PROPERTIES FOLDER "BRL-CAD Regression Testing")
endmacro(STEP_G_TEST)

STEP_G_TEST("001" "1797609in.stp" "833eeab8c0ef61177aa846c76eaabb31" "ST-ACIS, NIST DPPA Repository")
STEP_G_TEST("002" "2827056.stp"   "76283e455acd2ee9324e33b549bedcab" "ST-ACIS, NIST DPPA Repository")
STEP_G_TEST("003" "4pinplug.stp"  "46554b41552b4e797591dbab4c56ef9d" "ST-ACIS, STEP Tools, Inc. customer")

CMAKEFILES(test_001.md5 test_002.md5 test_003.md5)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
