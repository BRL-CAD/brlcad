
file(READ ${MD5_SUM} MD5_CONTROL)
file(MD5 ${OUTPUT_FILE} MD5_OUTPUT)

if(${MD5_CONTROL} STREQUAL ${MD5_OUTPUT})
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/test_${testnum}.passed "passed")
else(${MD5_CONTROL} STREQUAL ${MD5_OUTPUT})
  message(FATAL_ERROR "Test ${testnum}: MD5 sums differ.\n Control: ${MD5_CONTROL}\n Generated: ${MD5_OUTPUT}")
endif(${MD5_CONTROL} STREQUAL ${MD5_OUTPUT})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
