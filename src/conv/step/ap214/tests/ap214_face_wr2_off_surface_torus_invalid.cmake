if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

set(reject_report "${REPORT}.reject")
set(strict_report "${REPORT}.strict")
set(strict_output "${OUTPUT}.strict")
file(REMOVE "${REPORT}" "${reject_report}" "${strict_report}"
  "${OUTPUT}" "${strict_output}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 1)
  message(FATAL_ERROR
    "invalid FACE.wr2 torus returned ${import_result}, expected 1:\n"
    "${import_output}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"requested_invalid_brep_policy\":\"preserve\""
    "\"effective_invalid_brep_policy\":\"preserve\""
    "\"outcome\":\"partial\""
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"status\":\"preserved_invalid\""
    "\"preserved_invalid_items\":1"
    "\"invalid_breps\":1,\"invalid_breps_written\":1,\"invalid_breps_rejected\":0"
    "preserved a storage-safe partial importer BREP after exact construction and valid recovery paths failed"
    "source face violates FACE.wr2: at most one FACE_OUTER_BOUND is permitted, but 2 were supplied"
    "source curve/surface separation")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "ls -a"
  OUTPUT_VARIABLE objects ERROR_VARIABLE objects_error)
set(object_text "${objects}${objects_error}")
string(REGEX MATCH "[^ \r\n]+_item" wrapper_match "${object_text}")
if(NOT wrapper_match)
  message(FATAL_ERROR
    "could not locate preserved partial BREP wrapper:\n${object_text}")
endif()
set(wrapper "${wrapper_match}")
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "attr show ${wrapper}"
  OUTPUT_VARIABLE attrs ERROR_VARIABLE attrs_error)
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "get ${wrapper}"
  OUTPUT_VARIABLE wrapper_get ERROR_VARIABLE wrapper_get_error)
set(attr_text "${attrs}${attrs_error}")
set(wrapper_text "${wrapper_get}${wrapper_get_error}")
foreach(expected
    "step:geometry_status[ ]+invalid_preserved"
    "step:import_status[ ]+invalid"
    "step:source_validity[ ]+unresolved"
    "step:invalidity[ ]+partial_construction"
    "step:invalid_reason[ ]+exact STEP BREP construction failed after producing a storage-safe partial BREP")
  if(NOT attr_text MATCHES "${expected}")
    message(FATAL_ERROR
      "preserved partial BREP attributes omit ${expected}:\n${attr_text}")
  endif()
endforeach()
if(NOT wrapper_text MATCHES "comb region no")
  message(FATAL_ERROR
    "preserved partial BREP wrapper is unexpectedly a region:\n${wrapper_text}")
endif()

foreach(forbidden
    "selected the unique whole-BREP interpretation of ambiguous supplied FACE_BOUND loops"
    "used a densely measured local OpenNURBS tolerance")
  string(FIND "${report_text}" "${forbidden}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR
      "invalid FACE.wr2 torus applied forbidden repair '${forbidden}':\n"
      "${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" -D --reject-invalid-objs --report "${reject_report}"
    "${INPUT}"
  RESULT_VARIABLE reject_result
  OUTPUT_VARIABLE reject_output
  ERROR_VARIABLE reject_error
)
if(NOT reject_result EQUAL 3)
  message(FATAL_ERROR
    "explicit partial-BREP rejection returned ${reject_result}, expected 3:\n"
    "${reject_output}${reject_error}")
endif()
file(READ "${reject_report}" reject_text)
foreach(expected
    "\"requested_invalid_brep_policy\":\"reject\""
    "\"effective_invalid_brep_policy\":\"reject\""
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1"
    "\"invalid_breps\":1,\"invalid_breps_written\":0,\"invalid_breps_rejected\":1")
  string(FIND "${reject_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "partial-BREP rejection report omits ${expected}:\n${reject_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" --strict -O "${strict_output}"
    --report "${strict_report}" "${INPUT}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_output_text
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 3 OR EXISTS "${strict_output}")
  message(FATAL_ERROR
    "strict partial-BREP rejection returned ${strict_result} or published output:\n"
    "${strict_output_text}${strict_error}")
endif()
file(READ "${strict_report}" strict_text)
if(NOT strict_text MATCHES
    "\"requested_invalid_brep_policy\":\"preserve\"" OR
   NOT strict_text MATCHES
    "\"effective_invalid_brep_policy\":\"reject\"")
  message(FATAL_ERROR
    "strict partial-BREP policy report is wrong:\n${strict_text}")
endif()
