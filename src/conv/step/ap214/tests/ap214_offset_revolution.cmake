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
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "offset revolution fixture failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "represented an offset swept surface by a tolerance-bounded profile "
    "within the declared STEP uncertainty")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Offset_Revolution_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
    NOT brep_text MATCHES "faces:[ ]+1")
  message(FATAL_ERROR
    "offset revolution BREP validation failed:\n${brep_text}")
endif()

# Check the represented locus as well as BREP structure.  Revolving the
# radius-10 meridian and applying the requested +1 offset must produce a
# radius-11 cylindrical face over the unchanged 0..20 profile interval.
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" bb -q -e Offset_Revolution_item.s
  RESULT_VARIABLE bbox_result
  OUTPUT_VARIABLE bbox_output
  ERROR_VARIABLE bbox_error
  TIMEOUT 30
)
set(bbox_text "${bbox_output}\n${bbox_error}")
if(NOT bbox_result EQUAL 0 OR
    NOT bbox_text MATCHES
      "min \\{-11\\.00[0-9]+ -11\\.00[0-9]+ -0\\.00[0-9]+\\} max \\{11\\.00[0-9]+ 11\\.00[0-9]+ 20\\.00[0-9]+\\}")
  message(FATAL_ERROR
    "offset revolution bounding box validation failed:\n${bbox_text}")
endif()
