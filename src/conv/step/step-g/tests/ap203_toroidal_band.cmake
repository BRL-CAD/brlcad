if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "STEP_G, INPUT, and REPORT are required")
endif()

file(REMOVE "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -D --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
  TIMEOUT 60
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "implicit toroidal band import failed (${result})\n${stdout}\n${stderr}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "inserted an exact OpenNURBS seam for an implicit periodic STEP face band")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

# The fixture deliberately violates FACE.wr2 by declaring both physical band
# boundaries as FACE_OUTER_BOUND.  Inferring the intrinsic torus band is a
# safe repair, never compatibility behavior: repair-none must retain the
# invalid source interpretation and reject it.
set(no_repair_report "${REPORT}.repair-none")
file(REMOVE "${no_repair_report}")
execute_process(
  COMMAND "${STEP_G}" -D --repair none --report "${no_repair_report}" "${INPUT}"
  RESULT_VARIABLE no_repair_result
  OUTPUT_VARIABLE no_repair_stdout
  ERROR_VARIABLE no_repair_stderr
  TIMEOUT 60
)
if(NOT no_repair_result EQUAL 3)
  message(FATAL_ERROR
    "invalid FACE.wr2 band unexpectedly imported without repair (${no_repair_result})\n${no_repair_stdout}\n${no_repair_stderr}")
endif()
file(READ "${no_repair_report}" no_repair_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "\"invalid_breps\":1")
  string(FIND "${no_repair_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "repair-none report does not contain ${expected}:\n${no_repair_text}")
  endif()
endforeach()
