if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "safe step-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"repairs\":1"
    "\"inferred_curves\":0"
    "removed an exact zero-area open-surface face bounded by reciprocal distinct STEP edges #36 and #37")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "safe report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep
    Distinct_Edge_Zero_Area_Face_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+4")
  message(FATAL_ERROR
    "zero-area face repair did not produce a valid solid\n${brep_text}")
endif()

set(strict_report "${REPORT}.strict.json")
execute_process(
  COMMAND "${STEP_G}" -D --strict --report "${strict_report}" "${INPUT}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_output
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 0)
  message(FATAL_ERROR
    "strict safe proof returned ${strict_result}\n${strict_output}\n${strict_error}")
endif()
file(READ "${strict_report}" strict_report_text)
foreach(expected
    "\"geometry_written\":1"
    "\"invalid_breps\":0"
    "\"repairs\":1"
    "\"inferred_curves\":0")
  string(FIND "${strict_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "strict report does not contain ${expected}:\n${strict_report_text}")
  endif()
endforeach()

function(expect_unrepaired_rejection policy_name)
  set(policy_report "${REPORT}.${policy_name}.json")
  file(REMOVE "${policy_report}")
  execute_process(
    COMMAND "${STEP_G}" -D ${ARGN} --reject-invalid-objs
      --report "${policy_report}" "${INPUT}"
    RESULT_VARIABLE policy_result
    OUTPUT_VARIABLE policy_output
    ERROR_VARIABLE policy_error
  )
  if(NOT policy_result EQUAL 3)
    message(FATAL_ERROR
      "${policy_name} unexpectedly accepted the unrepaired face (status ${policy_result})\n${policy_output}\n${policy_error}")
  endif()
  file(READ "${policy_report}" policy_report_text)
  foreach(expected
      "\"geometry_written\":0"
      "\"geometry_skipped\":1"
      "\"invalid_breps_rejected\":1"
      "\"repairs\":0"
      "closed STEP BREP did not validate as a solid")
    string(FIND "${policy_report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "${policy_name} report does not contain ${expected}:\n${policy_report_text}")
    endif()
  endforeach()
endfunction()

expect_unrepaired_rejection(exact --exact)
expect_unrepaired_rejection(repair_none --repair none)
