if(NOT DEFINED MGED OR NOT DEFINED DB OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "MGED, DB, and TEST_DIR are required")
endif()

set(plot_prefix "${TEST_DIR}/gqa-prefix")
execute_process(
  COMMAND "${MGED}" -c "${DB}" "gqa -Ab -p ${plot_prefix} -g 10000 all.g"
  OUTPUT_VARIABLE gqa_output
  ERROR_VARIABLE gqa_error
  RESULT_VARIABLE gqa_result
  TIMEOUT 30
)

if(NOT gqa_result EQUAL 0)
  message(FATAL_ERROR "MGED gqa command failed:\n${gqa_output}\n${gqa_error}")
endif()

set(gqa_log "${gqa_output}\n${gqa_error}")
if(gqa_log MATCHES "rt_gettree\\(${plot_prefix}\\) FAILED|invalid syntax|Usage: gqa")
  message(FATAL_ERROR "MGED treated the -p value as geometry:\n${gqa_log}")
endif()
if(NOT gqa_log MATCHES "bounding box:")
  message(FATAL_ERROR "MGED gqa did not complete the requested analysis:\n${gqa_log}")
endif()
