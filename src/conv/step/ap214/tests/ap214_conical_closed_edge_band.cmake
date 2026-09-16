if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
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
  message(FATAL_ERROR "step-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"repairs\":0"
    "\"brep_unrepaired_preflight\":{")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
# This valid exact periodic band used to be modified by safe-mode seam repair:
# the larger larger conical-band cases then failed even though --repair none was
# valid.  Require the default safe policy to accept the raw candidate without
# applying any repair, so the small fixture guards that ordering regression.
foreach(unexpected
    "aligned a closed surface seam with an exact pole cut"
    "inserted an exact paired seam cut to a surface pole"
    "\"opennurbs_initial_validation\":"
    "\"opennurbs_prevalidation\":"
    "\"opennurbs_structural_validation\":")
  string(FIND "${report_text}" "${unexpected}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "valid source-faithful conical band entered an unnecessary repair/validation stage (${unexpected}):\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Conical_Closed_Edge_Band_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1")
  message(FATAL_ERROR "conical closed-edge band BREP validation failed\n${brep_text}")
endif()
