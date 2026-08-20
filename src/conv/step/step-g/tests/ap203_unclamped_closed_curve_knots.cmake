if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

function(check_import report output exact)
  file(REMOVE "${report}" "${output}")
  if(exact)
    set(mode --exact)
  else()
    set(mode)
  endif()
  execute_process(
    COMMAND "${STEP_G}" ${mode} --report "${report}" "${INPUT}" "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
      "unclamped closed-curve import failed (${result}, exact=${exact})\n${stdout}\n${stderr}")
  endif()

  file(READ "${report}" report_text)
  foreach(expected
      "\"B_SPLINE_CURVE_WITH_KNOTS\":1"
      "\"geometry_attempted\":1"
      "\"geometry_written\":1"
      "\"geometry_skipped\":0"
      "\"invalid_breps\":0")
    string(FIND "${report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "report does not contain ${expected} (exact=${exact}):\n${report_text}")
    endif()
  endforeach()

  execute_process(
    COMMAND "${MGED}" -c "${output}"
      brep AP203_Unclamped_Closed_Curve_Knots_item.s info
    RESULT_VARIABLE brep_result
    OUTPUT_VARIABLE brep_output
    ERROR_VARIABLE brep_error
  )
  set(brep_text "${brep_output}\n${brep_error}")
  if(NOT brep_result EQUAL 0 OR
      NOT brep_text MATCHES "Valid: YES, Solid: NO, Plate mode: YES" OR
      NOT brep_text MATCHES "faces:[ ]+1" OR
      NOT brep_text MATCHES "edges:[ ]+1" OR
      NOT brep_text MATCHES "vertices:[ ]+1" OR
      NOT brep_text MATCHES "loops:[ ]+1" OR
      NOT brep_text MATCHES "trims:[ ]+1")
    message(FATAL_ERROR
      "unclamped closed-curve geometry validation failed (exact=${exact})\n${brep_text}")
  endif()
endfunction()

check_import("${REPORT}" "${OUTPUT}" FALSE)
check_import("${REPORT}.exact" "${OUTPUT}.exact.g" TRUE)
