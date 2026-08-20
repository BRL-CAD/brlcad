if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR
    NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, INPUT, REPORT, and OUTPUT are required")
endif()

# This fixture intentionally retains only the two faces needed to reproduce
# the toroidal keyhole branch interaction.  It must therefore be rejected as
# a non-solid, but its two loops must complete OpenNURBS structural validation.
file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --no-item-budget --reject-invalid-objs -e 4000002 -O "${OUTPUT}"
    --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 30
)
if(NOT import_result EQUAL 3)
  message(FATAL_ERROR
    "doubly-periodic remap fixture returned ${import_result}, expected 3\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "\"reason\":\"closed STEP BREP did not validate as a solid\""
    "rolled back native-domain edge remaps which opened an exact cyclic join"
    "\"opennurbs_structural_validation\":{\"calls\":1")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

string(FIND "${report_text}" "OpenNURBS structural validation failed"
  structural_failure)
if(NOT structural_failure EQUAL -1)
  message(FATAL_ERROR
    "representative torus loops regressed structurally:\n${report_text}")
endif()
