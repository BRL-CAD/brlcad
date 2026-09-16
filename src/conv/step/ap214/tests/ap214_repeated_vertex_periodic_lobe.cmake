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
    "permissive repeated-vertex lobe import returned ${import_result}:\n"
    "${import_output}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"inferred_curves\":2"
    "noncontractible_repeated_vertex_lobe_removal"
    "retained the contractible face boundary")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive periodic-lobe report lacks ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep
    Repeated_Vertex_Periodic_Lobe_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+4")
  message(FATAL_ERROR
    "periodic-lobe inference did not retain one valid cylinder patch:\n"
    "${brep_text}")
endif()

foreach(object
    Repeated_Vertex_Periodic_Lobe_item.s
    Repeated_Vertex_Periodic_Lobe_item)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" attr show "${object}"
    OUTPUT_VARIABLE attr_output
    ERROR_VARIABLE attr_error
  )
  set(attr_text "${attr_output}\n${attr_error}")
  foreach(expected
      "step:geometry_status[ ]+inferred"
      "step:inferred_curve_ids[ ]+40 41"
      "40=noncontractible_repeated_vertex_lobe_removal"
      "41=noncontractible_repeated_vertex_lobe_removal"
      "exact 1-turn cycle")
    if(NOT attr_text MATCHES "${expected}")
      message(FATAL_ERROR
        "${object} lobe-inference provenance lacks ${expected}:\n${attr_text}")
    endif()
  endforeach()
endforeach()

function(expect_invalid_preserved policy_name expected_status)
  set(policy_report "${REPORT}.${policy_name}.json")
  file(REMOVE "${policy_report}")
  execute_process(
    COMMAND "${STEP_G}" -D ${ARGN} --report "${policy_report}" "${INPUT}"
    RESULT_VARIABLE policy_result
    OUTPUT_VARIABLE policy_output
    ERROR_VARIABLE policy_error
  )
  if(NOT policy_result EQUAL expected_status)
    message(FATAL_ERROR
      "${policy_name} returned ${policy_result}, expected ${expected_status}:\n"
      "${policy_output}${policy_error}")
  endif()
  file(READ "${policy_report}" policy_text)
  foreach(expected
      "\"geometry_written\":1"
      "\"geometry_skipped\":0"
      "\"invalid_breps\":1"
      "\"invalid_breps_written\":1"
      "\"inferred_curves\":0")
    string(FIND "${policy_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "${policy_name} report lacks ${expected}:\n${policy_text}")
    endif()
  endforeach()
endfunction()

expect_invalid_preserved(exact 1 --exact)
expect_invalid_preserved(none 1 --repair none)

set(strict_report "${REPORT}.strict.json")
execute_process(
  COMMAND "${STEP_G}" -D --strict --report "${strict_report}" "${INPUT}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_output
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 3)
  message(FATAL_ERROR
    "strict mode returned ${strict_result}, expected rejection:\n"
    "${strict_output}${strict_error}")
endif()
file(READ "${strict_report}" strict_text)
if(NOT strict_text MATCHES "\"geometry_written\":0" OR
   NOT strict_text MATCHES "\"geometry_skipped\":1" OR
   strict_text MATCHES "noncontractible_repeated_vertex_lobe_removal")
  message(FATAL_ERROR
    "strict mode accepted or tagged lobe inference:\n${strict_text}")
endif()
