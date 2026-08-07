if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()
set(reject_report "${REPORT}.reject")
set(strict_report "${REPORT}.strict")
set(strict_output "${OUTPUT}.strict")
file(REMOVE "${REPORT}" "${reject_report}" "${strict_report}" "${OUTPUT}" "${strict_output}")

execute_process(COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE default_result OUTPUT_VARIABLE default_log ERROR_VARIABLE default_error)
if(NOT default_result EQUAL 1)
  message(FATAL_ERROR "AP203 preservation returned ${default_result}:\n${default_log}${default_error}")
endif()
file(READ "${REPORT}" default_text)
foreach(expected
    "\"requested_invalid_brep_policy\":\"preserve\""
    "\"effective_invalid_brep_policy\":\"preserve\""
    "\"outcome\":\"partial\""
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"status\":\"preserved_invalid\""
    "\"preserved_invalid_items\":1"
    "\"invalid_breps\":1,\"invalid_breps_written\":1,\"invalid_breps_rejected\":0"
    "preserved as an unresolved invalid import"
    "preserved unresolved importer BREP")
  string(FIND "${default_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "AP203 preservation report omits ${expected}:\n${default_text}")
  endif()
endforeach()
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "ls -a"
  OUTPUT_VARIABLE objects ERROR_VARIABLE objects_error)
set(object_text "${objects}${objects_error}")
string(REGEX MATCH "[^ \r\n]+_item" wrapper_match "${object_text}")
if(NOT wrapper_match)
  message(FATAL_ERROR "could not locate preserved AP203 wrapper:\n${objects}${objects_error}")
endif()
set(wrapper "${wrapper_match}")
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "attr show ${wrapper}"
  OUTPUT_VARIABLE attrs ERROR_VARIABLE attrs_error)
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "get ${wrapper}"
  OUTPUT_VARIABLE wrapper_get ERROR_VARIABLE wrapper_get_error)
set(attr_text "${attrs}${attrs_error}")
set(wrapper_text "${wrapper_get}${wrapper_get_error}")
if(NOT attr_text MATCHES "step:geometry_status[ ]+invalid_preserved" OR
   NOT attr_text MATCHES "step:import_status[ ]+invalid" OR
   NOT attr_text MATCHES "step:source_validity[ ]+unresolved" OR
   NOT attr_text MATCHES "step:invalidity[ ]+opennurbs_structure" OR
   NOT wrapper_text MATCHES "comb region no")
  message(FATAL_ERROR "AP203 preserved wrapper metadata/type is wrong:\n${attrs}${attrs_error}${wrapper_get}${wrapper_get_error}")
endif()

execute_process(COMMAND "${STEP_G}" -D --reject-invalid-objs --report "${reject_report}" "${INPUT}"
  RESULT_VARIABLE reject_result OUTPUT_VARIABLE reject_log ERROR_VARIABLE reject_error)
if(NOT reject_result EQUAL 3)
  message(FATAL_ERROR "AP203 rejection returned ${reject_result}:\n${reject_log}${reject_error}")
endif()
file(READ "${reject_report}" reject_text)
if(NOT reject_text MATCHES "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1" OR
   NOT reject_text MATCHES "\"invalid_breps\":1,\"invalid_breps_written\":0,\"invalid_breps_rejected\":1")
  message(FATAL_ERROR "AP203 rejection report is wrong:\n${reject_text}")
endif()

execute_process(COMMAND "${STEP_G}" --strict -O "${strict_output}" --report "${strict_report}" "${INPUT}"
  RESULT_VARIABLE strict_result OUTPUT_VARIABLE strict_log ERROR_VARIABLE strict_error)
if(NOT strict_result EQUAL 3 OR EXISTS "${strict_output}")
  message(FATAL_ERROR "AP203 strict mode returned ${strict_result} or published output:\n${strict_log}${strict_error}")
endif()
file(READ "${strict_report}" strict_text)
if(NOT strict_text MATCHES "\"requested_invalid_brep_policy\":\"preserve\"" OR
   NOT strict_text MATCHES "\"effective_invalid_brep_policy\":\"reject\"")
  message(FATAL_ERROR "AP203 strict policy report is wrong:\n${strict_text}")
endif()
