if(NOT DEFINED STEP_G OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR "STEP_G, INPUT, and OUTPUT_DIR are required")
endif()

function(require_text text needle context)
  string(FIND "${text}" "${needle}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "${context}: missing '${needle}':\n${text}")
  endif()
endfunction()

set(report "${OUTPUT_DIR}/g_step_ap203_complex_retention.json")
file(REMOVE "${report}")
execute_process(
  COMMAND "${STEP_G}" --schema ap203 --dry-run --report "${report}" "${INPUT}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

# This focused metadata fixture has no shape representation, so empty geometry
# is the expected result.  Its purpose is to exercise the Part 21 complex
# index and dependency retention without manufacturing unrelated geometry.
if(NOT result EQUAL 6 OR NOT EXISTS "${report}")
  message(FATAL_ERROR
    "AP203 complex retention failed (${result}):\n${output}${error}")
endif()

file(READ "${report}" report_text)
foreach(expected
    "\"CONFIGURATION_EFFECTIVITY\":1"
    "\"DATED_EFFECTIVITY\":1"
    "\"EFFECTIVITY\":1"
    "\"PRODUCT_DEFINITION_EFFECTIVITY\":1"
    "\"entity_id\":1,\"type\":\"APPLICATION_CONTEXT\""
    "\"entity_id\":2,\"type\":\"PRODUCT_CONCEPT_CONTEXT\""
    "\"entity_id\":3,\"type\":\"PRODUCT_CONCEPT\""
    "\"entity_id\":4,\"type\":\"CONFIGURATION_ITEM\""
    "\"entity_id\":16,\"type\":\"COMPLEX\",\"component_types\":[\"CONFIGURATION_EFFECTIVITY\",\"DATED_EFFECTIVITY\",\"EFFECTIVITY\",\"PRODUCT_DEFINITION_EFFECTIVITY\"]"
    "(CONFIGURATION_EFFECTIVITY(#11) DATED_EFFECTIVITY(#15,$) EFFECTIVITY('effectivity') PRODUCT_DEFINITION_EFFECTIVITY(#10))"
    "STEP::AP203::CONFIGURATION::#16::COMPONENT_TYPES"
    "CONFIGURATION_EFFECTIVITY DATED_EFFECTIVITY EFFECTIVITY PRODUCT_DEFINITION_EFFECTIVITY")
  require_text("${report_text}" "${expected}" "AP203 complex retention report")
endforeach()
