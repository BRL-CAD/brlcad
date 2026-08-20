if(NOT DEFINED G_STEP OR NOT DEFINED MGED OR NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR "G_STEP, MGED, and OUTPUT_DIR are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

function(reject_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "${description}: unexpectedly contains '${needle}':\n${text}")
  endif()
endfunction()

set(input "${OUTPUT_DIR}/g_step_coverage_input.g")
file(REMOVE "${input}")
execute_process(
  COMMAND "${MGED}" -c "${input}"
    "in box.s rpp 0 10 0 20 0 30; in marker.s grip 0 0 0 0 0 1 5; put mixed.g comb region no tree {u {l box.s} {l marker.s}}"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create export-coverage fixture:\n${create_output}${create_error}")
endif()

foreach(schema ap203 ap203e2 ap214 ap242)
  set(report_schema "${schema}")
  if(schema STREQUAL "ap242")
    set(report_schema "ap242e4")
  endif()
  set(unsupported_step "${OUTPUT_DIR}/g_step_coverage_${schema}_unsupported.stp")
  set(unsupported_report "${OUTPUT_DIR}/g_step_coverage_${schema}_unsupported.json")
  set(partial_step "${OUTPUT_DIR}/g_step_coverage_${schema}_partial.stp")
  set(partial_report "${OUTPUT_DIR}/g_step_coverage_${schema}_partial.json")
  set(strict_step "${OUTPUT_DIR}/g_step_coverage_${schema}_strict.stp")
  set(strict_report "${OUTPUT_DIR}/g_step_coverage_${schema}_strict.json")
  file(REMOVE "${unsupported_step}" "${unsupported_report}"
    "${partial_step}" "${partial_report}" "${strict_step}" "${strict_report}")

  # GRIP advertises ft_brep but explicitly returns NULL.  It must not acquire
  # an empty ADVANCED_BREP product or a successful result.
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --report "${unsupported_report}"
      -o "${unsupported_step}" "${input}" marker.s
    RESULT_VARIABLE unsupported_result
    OUTPUT_VARIABLE unsupported_output
    ERROR_VARIABLE unsupported_error
  )
  if(NOT unsupported_result EQUAL 4 OR EXISTS "${unsupported_step}")
    message(FATAL_ERROR
      "${schema} unsupported export returned ${unsupported_result} or published output:\n"
      "${unsupported_output}${unsupported_error}")
  endif()
  file(READ "${unsupported_report}" unsupported_text)
  foreach(expected
      "\"schema\":\"${report_schema}\""
      "\"exit_status\":4"
      "\"outcome\":\"failed\""
      "\"name\":\"marker.s\",\"primitive_type\":22,\"combination\":false,\"status\":\"unsupported\""
      "BRL-CAD could not construct a BRep fallback")
    require_text("${unsupported_text}" "${expected}"
      "${schema} unsupported export report")
  endforeach()

  # Permissive mode publishes the usable box and reports both the exact
  # unsupported leaf and its consequently incomplete parent.
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --report "${partial_report}"
      -o "${partial_step}" "${input}" mixed.g
    RESULT_VARIABLE partial_result
    OUTPUT_VARIABLE partial_output
    ERROR_VARIABLE partial_error
  )
  if(NOT partial_result EQUAL 1 OR NOT EXISTS "${partial_step}")
    message(FATAL_ERROR
      "${schema} permissive partial export returned ${partial_result} or omitted output:\n"
      "${partial_output}${partial_error}")
  endif()
  file(READ "${partial_report}" partial_text)
  foreach(expected
      "\"outcome\":\"partial\""
      "\"name\":\"mixed.g\",\"primitive_type\":31,\"combination\":true,\"status\":\"skipped\""
      "\"name\":\"box.s\",\"primitive_type\":4,\"combination\":false,\"status\":\"handled\""
      "\"name\":\"marker.s\",\"primitive_type\":22,\"combination\":false,\"status\":\"unsupported\"")
    require_text("${partial_text}" "${expected}" "${schema} partial export report")
  endforeach()
  file(READ "${partial_step}" partial_step_text)
  require_text("${partial_step_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION("
    "${schema} partial STEP output")
  reject_text("${partial_step_text}" "PRODUCT('marker.s'"
    "${schema} partial STEP output")

  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict --report "${strict_report}"
      -o "${strict_step}" "${input}" mixed.g
    RESULT_VARIABLE strict_result
    OUTPUT_VARIABLE strict_output
    ERROR_VARIABLE strict_error
  )
  if(NOT strict_result EQUAL 4 OR EXISTS "${strict_step}")
    message(FATAL_ERROR
      "${schema} strict partial export returned ${strict_result} or published output:\n"
      "${strict_output}${strict_error}")
  endif()
  file(READ "${strict_report}" strict_text)
  foreach(expected "\"strict\":true" "\"outcome\":\"partial\""
      "\"name\":\"marker.s\",\"primitive_type\":22,\"combination\":false,\"status\":\"unsupported\"")
    require_text("${strict_text}" "${expected}" "${schema} strict export report")
  endforeach()
endforeach()
