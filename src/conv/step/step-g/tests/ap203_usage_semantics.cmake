if(NOT DEFINED STEP_G OR NOT DEFINED G_STEP OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, G_STEP, MGED, INPUT, REPORT, and OUTPUT are required")
endif()
file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "usage-semantics import returned ${result}:\n${log}${error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"products\":5"
    "\"occurrences\":5"
    "\"shape_method\":\"replicated\""
    "\"type\":\"SPECIFIED_HIGHER_USAGE_OCCURRENCE\""
    "\"upper_usage_id\":200,\"next_usage_id\":202"
    "\"upper_usage_id\":201,\"next_usage_id\":203"
    "\"quantified\":true"
    "\"promissory\":true"
    "\"quantity\":4"
    "\"quantity\":8"
    "\"base_product_id\":40,\"alternate_product_id\":50"
    "\"base_usage_id\":202,\"substitute_usage_id\":204"
    "\"configuration_records\":[{"
    "\"entity_id\":701,\"type\":\"APPROVAL\",\"component_types\":[],\"value\":\"APPROVAL(#700,'release A')\",\"references\":[700]"
    "STEP::AP203::APPROVAL::#701"
    "STEP::AP203::CONFIGURATION::#701::TYPE"
    "STEP::AP203::SECURITY_CLASSIFICATION::#703"
    "STEP::AP203::ORGANIZATION::#704"
    "STEP::AP203::DOCUMENT::#708"
    "STEP::AP203::DATED_EFFECTIVITY::#714")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "usage-semantics report omits ${expected}:\n${report_text}")
  endif()
endforeach()
execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "tree Usage_Root"
  OUTPUT_VARIABLE tree_output ERROR_VARIABLE tree_error)
set(tree_text "${tree_output}${tree_error}")
foreach(expected "Branch_A" "Branch_B" "Usage_Leaf" "Alternate_Leaf")
  string(FIND "${tree_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "usage hierarchy tree omits ${expected}:\n${tree_text}")
  endif()
endforeach()
execute_process(COMMAND "${MGED}" -c "${OUTPUT}"
  "attr get _GLOBAL STEP::AP203::APPROVAL::#701"
  OUTPUT_VARIABLE attribute_output ERROR_VARIABLE attribute_error)
if(NOT "${attribute_output}${attribute_error}" MATCHES "APPROVAL\\(#700,'release A'\\)")
  message(FATAL_ERROR "_GLOBAL approval metadata was not retained:\n${attribute_output}${attribute_error}")
endif()

execute_process(COMMAND "${MGED}" -c "${OUTPUT}"
  "attr get _GLOBAL STEP::AP203::CONFIGURATION::#701::TYPE"
  OUTPUT_VARIABLE structured_output ERROR_VARIABLE structured_error)
if(NOT "${structured_output}${structured_error}" MATCHES "APPROVAL")
  message(FATAL_ERROR
    "structured _GLOBAL configuration record was not retained:\n"
    "${structured_output}${structured_error}")
endif()

# The exporter reconstructs and reports the stable graph.  Identity,
# classification, document, alternate-product, product-category, and date/time
# subgraphs are authored with remapped references.  The orphan approval has no
# legal AP203 assignment and is reported as an explicit loss.  This fixture's
# imported reference geometry cannot be re-exported, so its usages are not
# emitted and the usage substitute remains an explicit loss.  Permissive mode
# publishes the useful partial result and strict mode is transactional.
set(export_output "${OUTPUT}.configuration.stp")
set(export_report "${REPORT}.configuration-export.json")
set(strict_output "${OUTPUT}.configuration-strict.stp")
set(strict_report "${REPORT}.configuration-strict.json")
file(REMOVE "${export_output}" "${export_report}" "${strict_output}"
  "${strict_report}")
execute_process(COMMAND "${G_STEP}" --schema ap203 --report "${export_report}"
    -o "${export_output}" "${OUTPUT}" Usage_Root
  RESULT_VARIABLE export_result OUTPUT_VARIABLE export_log
  ERROR_VARIABLE export_error)
if(NOT export_result EQUAL 1 OR NOT EXISTS "${export_output}")
  message(FATAL_ERROR
    "configuration-aware permissive export returned ${export_result}:\n"
    "${export_log}${export_error}")
endif()
file(READ "${export_report}" export_report_text)
foreach(expected
    "\"outcome\":\"partial\""
    "\"configuration_records_seen\":17"
    "\"configuration_records_emitted\":13"
    "\"configuration_records_omitted\":4"
    "\"schema\":\"AP203\",\"entity_id\":701,\"type\":\"APPROVAL\""
    "\"references\":[700],\"valid\":true,\"status\":\"unsupported\""
    "AP203 APPROVAL has no authorable CC_DESIGN_APPROVAL assignment"
    "\"entity_id\":703,\"type\":\"SECURITY_CLASSIFICATION\""
    "\"entity_id\":706,\"type\":\"PERSON_AND_ORGANIZATION\""
    "\"entity_id\":708,\"type\":\"DOCUMENT\""
    "the source usages do not map to distinct emitted usages")
  string(FIND "${export_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "configuration export report omits ${expected}:\n${export_report_text}")
  endif()
endforeach()
execute_process(COMMAND "${G_STEP}" --schema ap203 --strict
    --report "${strict_report}" -o "${strict_output}" "${OUTPUT}" Usage_Root
  RESULT_VARIABLE strict_result OUTPUT_VARIABLE strict_log
  ERROR_VARIABLE strict_error)
if(NOT strict_result EQUAL 4 OR EXISTS "${strict_output}")
  message(FATAL_ERROR
    "strict configuration export was not transactional (${strict_result}):\n"
    "${strict_log}${strict_error}")
endif()

execute_process(COMMAND "${MGED}" -c "${OUTPUT}"
  "attr set _GLOBAL STEP::AP203::CONFIGURATION::#999::TYPE APPROVAL"
  RESULT_VARIABLE malformed_create_result OUTPUT_VARIABLE malformed_create_log
  ERROR_VARIABLE malformed_create_error)
if(NOT malformed_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not add a malformed configuration node:\n"
    "${malformed_create_log}${malformed_create_error}")
endif()
set(malformed_output "${OUTPUT}.configuration-malformed.stp")
set(malformed_report "${REPORT}.configuration-malformed.json")
set(malformed_strict_output
  "${OUTPUT}.configuration-malformed-strict.stp")
set(malformed_strict_report
  "${REPORT}.configuration-malformed-strict.json")
file(REMOVE "${malformed_output}" "${malformed_report}"
  "${malformed_strict_output}" "${malformed_strict_report}")
execute_process(COMMAND "${G_STEP}" --schema ap203
    --report "${malformed_report}" -o "${malformed_output}" "${OUTPUT}"
    Usage_Root
  RESULT_VARIABLE malformed_result OUTPUT_VARIABLE malformed_log
  ERROR_VARIABLE malformed_error)
if(NOT malformed_result EQUAL 1 OR NOT EXISTS "${malformed_output}")
  message(FATAL_ERROR
    "malformed configuration export returned ${malformed_result}:\n"
    "${malformed_log}${malformed_error}")
endif()
file(READ "${malformed_report}" malformed_report_text)
foreach(expected
    "\"configuration_records_seen\":18"
    "\"configuration_records_emitted\":13"
    "\"configuration_records_omitted\":5"
    "\"entity_id\":999,\"type\":\"APPROVAL\",\"component_types\":[],\"value\":\"\",\"references\":[],\"valid\":false,\"status\":\"malformed\""
    "configuration record has no Part 21 value")
  string(FIND "${malformed_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "malformed configuration report omits ${expected}:\n"
      "${malformed_report_text}")
  endif()
endforeach()
execute_process(COMMAND "${G_STEP}" --schema ap203 --strict
    --report "${malformed_strict_report}" -o "${malformed_strict_output}"
    "${OUTPUT}" Usage_Root
  RESULT_VARIABLE malformed_strict_result
  OUTPUT_VARIABLE malformed_strict_log ERROR_VARIABLE malformed_strict_error)
if(NOT malformed_strict_result EQUAL 4 OR
   EXISTS "${malformed_strict_output}")
  message(FATAL_ERROR
    "strict malformed configuration export was not transactional "
    "(${malformed_strict_result}):\n"
    "${malformed_strict_log}${malformed_strict_error}")
endif()

# Older imported databases have only the compatibility key.  Recover its
# graph and references so they receive the same explicit loss policy.
set(legacy_database "${OUTPUT}.configuration-legacy.g")
set(legacy_output "${OUTPUT}.configuration-legacy.stp")
set(legacy_report "${REPORT}.configuration-legacy.json")
file(REMOVE "${legacy_database}" "${legacy_output}" "${legacy_report}")
execute_process(COMMAND "${MGED}" -c "${legacy_database}"
  "in legacy.s sph 0 0 0 5; r legacy.r u legacy.s; attr set _GLOBAL STEP::AP203::APPROVAL::#701 {APPROVAL(#700,'release A')}"
  RESULT_VARIABLE legacy_create_result OUTPUT_VARIABLE legacy_create_log
  ERROR_VARIABLE legacy_create_error)
if(NOT legacy_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create a compatibility configuration fixture:\n"
    "${legacy_create_log}${legacy_create_error}")
endif()
execute_process(COMMAND "${G_STEP}" --schema ap203 --report "${legacy_report}"
    -o "${legacy_output}" "${legacy_database}" legacy.r
  RESULT_VARIABLE legacy_result OUTPUT_VARIABLE legacy_log
  ERROR_VARIABLE legacy_error)
if(NOT legacy_result EQUAL 1 OR NOT EXISTS "${legacy_output}")
  message(FATAL_ERROR
    "compatibility configuration export returned ${legacy_result}:\n"
    "${legacy_log}${legacy_error}")
endif()
file(READ "${legacy_report}" legacy_report_text)
foreach(expected
    "\"configuration_records_seen\":1"
    "\"configuration_records_emitted\":0"
    "\"configuration_records_omitted\":1"
    "\"schema\":\"AP203\",\"entity_id\":701,\"type\":\"APPROVAL\""
    "\"references\":[700],\"valid\":true,\"status\":\"unsupported\""
    "AP203 APPROVAL has no authorable CC_DESIGN_APPROVAL assignment")
  string(FIND "${legacy_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "compatibility configuration report omits ${expected}:\n"
      "${legacy_report_text}")
  endif()
endforeach()
