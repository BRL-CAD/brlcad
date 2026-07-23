if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT OR NOT DEFINED EXACT_REPORT OR
   NOT DEFINED EXACT_OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, OUTPUT, EXACT_REPORT, and EXACT_OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}" "${EXACT_REPORT}" "${EXACT_OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE safe_result
  OUTPUT_VARIABLE safe_output
  ERROR_VARIABLE safe_error
)
execute_process(
  COMMAND "${AP214_G}" --exact -O "${EXACT_OUTPUT}"
    --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
)
if(NOT safe_result EQUAL 0)
  message(FATAL_ERROR "safe two-side singular import failed (${safe_result}):\n${safe_output}${safe_error}")
endif()
if(NOT exact_result EQUAL 0)
  message(FATAL_ERROR "exact two-side singular import failed (${exact_result}):\n${exact_output}${exact_error}")
endif()

foreach(result_file IN ITEMS "${REPORT}" "${EXACT_REPORT}")
  file(READ "${result_file}" report_text)
  foreach(expected
      "\"geometry_attempted\":1"
      "\"geometry_written\":1"
      "\"geometry_skipped\":0"
      "\"invalid_breps\":0"
      "inserted an exact two-side singular corner chain")
    string(FIND "${report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "${result_file} does not contain ${expected}:\n${report_text}")
    endif()
  endforeach()
endforeach()

foreach(database IN ITEMS "${OUTPUT}" "${EXACT_OUTPUT}")
  execute_process(
    COMMAND "${MGED}" -c "${database}"
      brep Two_Side_Singular_Corner_item.s info
    OUTPUT_VARIABLE brep_output
    ERROR_VARIABLE brep_error
  )
  set(brep_text "${brep_output}\n${brep_error}")
  if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
     NOT brep_text MATCHES "faces:[ ]+1" OR
     NOT brep_text MATCHES "trims:[ ]+4")
    message(FATAL_ERROR "two-side singular BREP validation failed\n${brep_text}")
  endif()
endforeach()
