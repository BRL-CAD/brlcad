if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED OUTPUT OR NOT DEFINED REPORT OR NOT DEFINED STRICT_REPORT OR
   NOT DEFINED TOP OR NOT DEFINED CHILD OR NOT DEFINED ASSOCIATION OR
   NOT DEFINED SCHEMA_METADATA OR NOT DEFINED TOLERANCE OR
   NOT DEFINED TRANSFORM_MARKER)
  message(FATAL_ERROR
    "STEP_G, MGED, INPUT, OUTPUT, REPORT, STRICT_REPORT, TOP, CHILD, "
    "ASSOCIATION, SCHEMA_METADATA, TOLERANCE, and TRANSFORM_MARKER are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

set(strict_output "${OUTPUT}.strict.g")
file(REMOVE "${OUTPUT}" "${REPORT}" "${strict_output}" "${STRICT_REPORT}")

execute_process(
  COMMAND "${STEP_G}" -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_log
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "mapped assembly baseline returned ${import_result} or omitted output:\n"
    "${import_log}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"exit_status\":0"
    "\"products\":2,\"occurrences\":2,\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"FACETED_BREP\":1"
    "\"MAPPED_ITEM\":2"
    "\"NEXT_ASSEMBLY_USAGE_OCCURRENCE\":2"
    "\"${ASSOCIATION}\":2"
    "\"assembly_usages\":[{"
    "\"occurrence_details\":[{"
    "\"shape_method\":\"mapped\""
    "\"product_alternatives\":[]"
    "\"usage_substitutes\":[]"
    "\"type\":\"MAPPED_ITEM\",\"status\":\"handled\""
    "\"skipped_items\":[]"
    "${SCHEMA_METADATA}"
    "\"tolerance_mm\":${TOLERANCE}")
  require_text("${report_text}" "${expected}" "mapped assembly report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get "${TOP}"
  RESULT_VARIABLE comb_result
  OUTPUT_VARIABLE comb_text
  ERROR_VARIABLE comb_error
)
if(NOT comb_result EQUAL 0)
  message(FATAL_ERROR "unable to inspect ${TOP}:\n${comb_text}${comb_error}")
endif()
set(comb_all "${comb_text}${comb_error}")
require_text("${comb_all}" "{l ${CHILD}}" "untransformed occurrence")
require_text("${comb_all}" "{l ${CHILD} ${TRANSFORM_MARKER}" "transformed occurrence")

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "attr show ${TOP}"
  RESULT_VARIABLE attributes_result
  OUTPUT_VARIABLE attributes_text
  ERROR_VARIABLE attributes_error
)
if(NOT attributes_result EQUAL 0)
  message(FATAL_ERROR
    "unable to inspect ${TOP} source identities:\n"
    "${attributes_text}${attributes_error}")
endif()
set(attributes_all "${attributes_text}${attributes_error}")
foreach(expected
    "step:source_id"
    "step:occurrence:1:source_id"
    "step:occurrence:2:source_id")
  require_text("${attributes_all}" "${expected}"
    "mapped assembly source-identity retention")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" tree "${TOP}"
  RESULT_VARIABLE tree_result
  OUTPUT_VARIABLE tree_text
  ERROR_VARIABLE tree_error
)
if(NOT tree_result EQUAL 0)
  message(FATAL_ERROR "unable to inspect ${TOP} tree:\n${tree_text}${tree_error}")
endif()
set(tree_all "${tree_text}${tree_error}")
string(REGEX MATCHALL "${CHILD}/" child_occurrences "${tree_all}")
list(LENGTH child_occurrences child_count)
if(NOT child_count EQUAL 2)
  message(FATAL_ERROR
    "expected exactly two ${CHILD} occurrences, found ${child_count}:\n${tree_all}")
endif()

# A wholly supported independent fixture must also be acceptable to strict
# mode.  Use a separate output to exercise the normal transactional publish.
execute_process(
  COMMAND "${STEP_G}" --strict -o "${strict_output}"
    --report "${STRICT_REPORT}" "${INPUT}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_log
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 0 OR NOT EXISTS "${strict_output}")
  message(FATAL_ERROR
    "strict mapped assembly import returned ${strict_result} or omitted output:\n"
    "${strict_log}${strict_error}")
endif()
file(READ "${STRICT_REPORT}" strict_text)
foreach(expected "\"strict\":true" "\"outcome\":\"complete\"" "\"skipped_items\":[]")
  require_text("${strict_text}" "${expected}" "strict mapped assembly report")
endforeach()
