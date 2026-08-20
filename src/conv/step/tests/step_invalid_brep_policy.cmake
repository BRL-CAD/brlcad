if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED TEMPLATE OR
   NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT OR
   NOT DEFINED SCHEMA OR NOT DEFINED SCHEMA_LABEL OR NOT DEFINED FILE_NAME OR
   NOT DEFINED APPLICATION OR NOT DEFINED PRODUCT_ID OR NOT DEFINED PRODUCT_NAME OR
   NOT DEFINED ASSOCIATION)
  message(FATAL_ERROR "all invalid BREP policy fixture parameters are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

configure_file("${TEMPLATE}" "${INPUT}" @ONLY)
set(reject_report "${REPORT}.reject")
set(strict_report "${REPORT}.strict")
set(strict_output "${OUTPUT}.strict")
file(REMOVE "${REPORT}" "${reject_report}" "${strict_report}"
  "${OUTPUT}" "${strict_output}")

# The permissive default retains the unresolved BREP in a clearly marked
# wrapper, giving a user access to the geometry and the precise importer
# validation result without claiming that the source model is invalid.
execute_process(
  COMMAND "${STEP_G}" -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE default_result
  OUTPUT_VARIABLE default_log
  ERROR_VARIABLE default_error
)
if(NOT default_result EQUAL 1 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "${SCHEMA_LABEL} invalid-BREP preservation returned ${default_result} or omitted output:\n"
    "${default_log}${default_error}")
endif()
file(READ "${REPORT}" default_text)
foreach(expected
    "\"requested_invalid_brep_policy\":\"preserve\""
    "\"effective_invalid_brep_policy\":\"preserve\""
    "\"outcome\":\"partial\""
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":1,\"invalid_breps_written\":1,\"invalid_breps_rejected\":0"
    "\"status\":\"preserved_invalid\""
    "\"preserved_invalid_items\":1"
    "preserved as an unresolved invalid import"
    "preserved unresolved importer BREP"
    "\"${ASSOCIATION}\":1")
  require_text("${default_text}" "${expected}" "${SCHEMA_LABEL} preservation report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "ls -a"
  RESULT_VARIABLE list_result
  OUTPUT_VARIABLE objects
  ERROR_VARIABLE objects_error
)
set(object_text "${objects}${objects_error}")
string(REGEX MATCH "[^ \r\n]+_item" wrapper "${object_text}")
if(NOT list_result EQUAL 0 OR NOT wrapper)
  message(FATAL_ERROR
    "could not locate preserved ${SCHEMA_LABEL} wrapper:\n${object_text}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "attr show ${wrapper}"
  OUTPUT_VARIABLE attrs
  ERROR_VARIABLE attrs_error
)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "get ${wrapper}"
  OUTPUT_VARIABLE wrapper_get
  ERROR_VARIABLE wrapper_get_error
)
set(attr_text "${attrs}${attrs_error}")
set(wrapper_text "${wrapper_get}${wrapper_get_error}")
foreach(expected
    "step:geometry_status" "invalid_preserved"
    "step:import_status" "invalid"
    "step:source_validity" "unresolved"
    "step:invalidity" "opennurbs_structure")
  require_text("${attr_text}" "${expected}" "${SCHEMA_LABEL} invalid wrapper metadata")
endforeach()
require_text("${wrapper_text}" "comb region no" "${SCHEMA_LABEL} invalid wrapper type")

# Explicit rejection and strict mode both prevent publication, but the report
# distinguishes the user's requested policy from strict's effective policy.
execute_process(
  COMMAND "${STEP_G}" -D --reject-invalid-objs --report "${reject_report}" "${INPUT}"
  RESULT_VARIABLE reject_result
  OUTPUT_VARIABLE reject_log
  ERROR_VARIABLE reject_error
)
if(NOT reject_result EQUAL 3)
  message(FATAL_ERROR
    "${SCHEMA_LABEL} explicit rejection returned ${reject_result}:\n${reject_log}${reject_error}")
endif()
file(READ "${reject_report}" reject_text)
foreach(expected
    "\"requested_invalid_brep_policy\":\"reject\""
    "\"effective_invalid_brep_policy\":\"reject\""
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1"
    "\"invalid_breps\":1,\"invalid_breps_written\":0,\"invalid_breps_rejected\":1")
  require_text("${reject_text}" "${expected}" "${SCHEMA_LABEL} rejection report")
endforeach()

execute_process(
  COMMAND "${STEP_G}" --strict -o "${strict_output}"
    --report "${strict_report}" "${INPUT}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_log
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 3 OR EXISTS "${strict_output}")
  message(FATAL_ERROR
    "${SCHEMA_LABEL} strict invalid import returned ${strict_result} or published output:\n"
    "${strict_log}${strict_error}")
endif()
file(READ "${strict_report}" strict_text)
foreach(expected
    "\"strict\":true"
    "\"requested_invalid_brep_policy\":\"preserve\""
    "\"effective_invalid_brep_policy\":\"reject\""
    "\"outcome\":\"failed\"")
  require_text("${strict_text}" "${expected}" "${SCHEMA_LABEL} strict invalid report")
endforeach()
