if(NOT DEFINED G_STEP OR NOT DEFINED STEP_G OR NOT DEFINED MGED OR
   NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR "G_STEP, STEP_G, MGED, and OUTPUT_DIR are required")
endif()

function(require_text text needle context)
  string(FIND "${text}" "${needle}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "${context}: missing '${needle}':\n${text}")
  endif()
endfunction()

function(reject_text text needle context)
  string(FIND "${text}" "${needle}" position)
  if(NOT position EQUAL -1)
    message(FATAL_ERROR "${context}: unexpectedly contains '${needle}'")
  endif()
endfunction()

set(input "${OUTPUT_DIR}/g_step_ap203_complex_export.g")
set(step "${OUTPUT_DIR}/g_step_ap203_complex_export.stp")
set(export_report "${OUTPUT_DIR}/g_step_ap203_complex_export.json")
set(roundtrip "${OUTPUT_DIR}/g_step_ap203_complex_export_roundtrip.g")
set(import_report "${OUTPUT_DIR}/g_step_ap203_complex_import.json")
set(serial_step "${OUTPUT_DIR}/g_step_ap203_complex_serial.stp")
set(serial_report "${OUTPUT_DIR}/g_step_ap203_complex_serial.json")
set(lot_step "${OUTPUT_DIR}/g_step_ap203_complex_lot.stp")
set(lot_report "${OUTPUT_DIR}/g_step_ap203_complex_lot.json")
set(multi_input "${OUTPUT_DIR}/g_step_ap203_complex_multi.g")
set(multi_step "${OUTPUT_DIR}/g_step_ap203_complex_multi.stp")
set(multi_report "${OUTPUT_DIR}/g_step_ap203_complex_multi.json")
set(malformed_step "${OUTPUT_DIR}/g_step_ap203_complex_malformed.stp")
set(malformed_report "${OUTPUT_DIR}/g_step_ap203_complex_malformed.json")
set(strict_step "${OUTPUT_DIR}/g_step_ap203_complex_malformed_strict.stp")
file(REMOVE "${input}" "${step}" "${export_report}" "${roundtrip}"
  "${import_report}" "${serial_step}" "${serial_report}" "${lot_step}"
  "${lot_report}" "${multi_input}" "${multi_step}" "${multi_report}"
  "${malformed_step}" "${malformed_report}" "${strict_step}")

execute_process(
  COMMAND "${MGED}" -c "${input}"
    "in part.s sph 0 0 0 5; r part.r u part.s; g all.g part.r; attr set part.r step:source_id 6 step:formation_source_id 7 step:definition_source_id 9; attr set all.g step:source_id 20 step:formation_source_id 21 step:definition_source_id 22 step:occurrence:1:source_id 10; attr set _GLOBAL STEP::AP203::CONFIGURATION::#1::TYPE APPLICATION_CONTEXT STEP::AP203::CONFIGURATION::#1::VALUE {APPLICATION_CONTEXT('configuration controlled 3d designs of mechanical parts and assemblies')} STEP::AP203::CONFIGURATION::#1::REFERENCES {} STEP::AP203::CONFIGURATION::#2::TYPE PRODUCT_CONCEPT_CONTEXT STEP::AP203::CONFIGURATION::#2::VALUE {PRODUCT_CONCEPT_CONTEXT('market',#1,'general')} STEP::AP203::CONFIGURATION::#2::REFERENCES 1 STEP::AP203::CONFIGURATION::#3::TYPE PRODUCT_CONCEPT STEP::AP203::CONFIGURATION::#3::VALUE {PRODUCT_CONCEPT('concept','Concept','retention fixture',#2)} STEP::AP203::CONFIGURATION::#3::REFERENCES 2 STEP::AP203::CONFIGURATION::#4::TYPE CONFIGURATION_ITEM STEP::AP203::CONFIGURATION::#4::VALUE {CONFIGURATION_ITEM('configuration','Configuration',$,#3,$)} STEP::AP203::CONFIGURATION::#4::REFERENCES 3 STEP::AP203::CONFIGURATION::#11::TYPE CONFIGURATION_DESIGN STEP::AP203::CONFIGURATION::#11::VALUE {CONFIGURATION_DESIGN(#4,#7)} STEP::AP203::CONFIGURATION::#11::REFERENCES {4 7} STEP::AP203::CONFIGURATION::#12::TYPE CALENDAR_DATE STEP::AP203::CONFIGURATION::#12::VALUE {CALENDAR_DATE(2028,1,1)} STEP::AP203::CONFIGURATION::#12::REFERENCES {} STEP::AP203::CONFIGURATION::#13::TYPE COORDINATED_UNIVERSAL_TIME_OFFSET STEP::AP203::CONFIGURATION::#13::VALUE {COORDINATED_UNIVERSAL_TIME_OFFSET(0,$,.AHEAD.)} STEP::AP203::CONFIGURATION::#13::REFERENCES {} STEP::AP203::CONFIGURATION::#14::TYPE LOCAL_TIME STEP::AP203::CONFIGURATION::#14::VALUE {LOCAL_TIME(0,$,$,#13)} STEP::AP203::CONFIGURATION::#14::REFERENCES 13 STEP::AP203::CONFIGURATION::#15::TYPE DATE_AND_TIME STEP::AP203::CONFIGURATION::#15::VALUE {DATE_AND_TIME(#12,#14)} STEP::AP203::CONFIGURATION::#15::REFERENCES {12 14} STEP::AP203::CONFIGURATION::#16::TYPE COMPLEX STEP::AP203::CONFIGURATION::#16::COMPONENT_TYPES {CONFIGURATION_EFFECTIVITY DATED_EFFECTIVITY EFFECTIVITY PRODUCT_DEFINITION_EFFECTIVITY} STEP::AP203::CONFIGURATION::#16::VALUE {(CONFIGURATION_EFFECTIVITY(#11) DATED_EFFECTIVITY(#15,$) EFFECTIVITY('effectivity') PRODUCT_DEFINITION_EFFECTIVITY(#10))} STEP::AP203::CONFIGURATION::#16::REFERENCES {11 15 10} STEP::AP203::CONFIGURATION::#40::TYPE PRODUCT_RELATED_PRODUCT_CATEGORY STEP::AP203::CONFIGURATION::#40::VALUE {PRODUCT_RELATED_PRODUCT_CATEGORY('machined','retained process category',(#6))} STEP::AP203::CONFIGURATION::#40::REFERENCES 6"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create AP203 complex export fixture:\n"
    "${create_output}${create_error}")
endif()

execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict --report "${export_report}"
    -o "${step}" "${input}" all.g
  RESULT_VARIABLE export_result
  OUTPUT_VARIABLE export_output
  ERROR_VARIABLE export_error
)
if(NOT export_result EQUAL 0 OR NOT EXISTS "${step}")
  message(FATAL_ERROR
    "AP203 complex export failed (${export_result}):\n"
    "${export_output}${export_error}")
endif()

file(READ "${export_report}" export_report_text)
foreach(expected
    "\"configuration_records_seen\":11"
    "\"configuration_records_emitted\":11"
    "\"configuration_records_omitted\":0"
    "authored as an AP203 complex configuration effectivity")
  require_text("${export_report_text}" "${expected}"
    "AP203 complex export report")
endforeach()

# Two independent retained application/configuration graphs must not collapse
# onto one APPLICATION_CONTEXT.  This also verifies that shared date and
# occurrence dependencies may feed more than one complex instance.
file(COPY_FILE "${input}" "${multi_input}")
execute_process(
  COMMAND "${MGED}" -c "${multi_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#41::TYPE APPLICATION_CONTEXT STEP::AP203::CONFIGURATION::#41::VALUE {APPLICATION_CONTEXT('secondary configuration context')} STEP::AP203::CONFIGURATION::#41::REFERENCES {} STEP::AP203::CONFIGURATION::#42::TYPE PRODUCT_CONCEPT_CONTEXT STEP::AP203::CONFIGURATION::#42::VALUE {PRODUCT_CONCEPT_CONTEXT('secondary market',#41,'secondary segment')} STEP::AP203::CONFIGURATION::#42::REFERENCES 41 STEP::AP203::CONFIGURATION::#43::TYPE PRODUCT_CONCEPT STEP::AP203::CONFIGURATION::#43::VALUE {PRODUCT_CONCEPT('concept-2','Concept 2','second retained concept',#42)} STEP::AP203::CONFIGURATION::#43::REFERENCES 42 STEP::AP203::CONFIGURATION::#44::TYPE CONFIGURATION_ITEM STEP::AP203::CONFIGURATION::#44::VALUE {CONFIGURATION_ITEM('configuration-2','Configuration 2',$,#43,$)} STEP::AP203::CONFIGURATION::#44::REFERENCES 43 STEP::AP203::CONFIGURATION::#45::TYPE CONFIGURATION_DESIGN STEP::AP203::CONFIGURATION::#45::VALUE {CONFIGURATION_DESIGN(#44,#7)} STEP::AP203::CONFIGURATION::#45::REFERENCES {44 7} STEP::AP203::CONFIGURATION::#46::TYPE COMPLEX STEP::AP203::CONFIGURATION::#46::COMPONENT_TYPES {CONFIGURATION_EFFECTIVITY DATED_EFFECTIVITY EFFECTIVITY PRODUCT_DEFINITION_EFFECTIVITY} STEP::AP203::CONFIGURATION::#46::VALUE {(CONFIGURATION_EFFECTIVITY(#45) DATED_EFFECTIVITY(#15,$) EFFECTIVITY('second effectivity') PRODUCT_DEFINITION_EFFECTIVITY(#10))} STEP::AP203::CONFIGURATION::#46::REFERENCES {45 15 10}"
  RESULT_VARIABLE multi_create_result
  OUTPUT_VARIABLE multi_create_output
  ERROR_VARIABLE multi_create_error
)
if(NOT multi_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create multi-effectivity AP203 fixture:\n"
    "${multi_create_output}${multi_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict --report "${multi_report}"
    -o "${multi_step}" "${multi_input}" all.g
  RESULT_VARIABLE multi_result
  OUTPUT_VARIABLE multi_output
  ERROR_VARIABLE multi_error
)
if(NOT multi_result EQUAL 0 OR NOT EXISTS "${multi_step}")
  message(FATAL_ERROR
    "multi-effectivity AP203 export failed (${multi_result}):\n"
    "${multi_output}${multi_error}")
endif()
file(READ "${multi_report}" multi_report_text)
foreach(expected
    "\"configuration_records_seen\":17"
    "\"configuration_records_emitted\":17"
    "\"configuration_records_omitted\":0")
  require_text("${multi_report_text}" "${expected}"
    "multi-effectivity AP203 report")
endforeach()
file(READ "${multi_step}" multi_step_text)
foreach(expected
    "APPLICATION_CONTEXT('configuration controlled 3d designs of mechanical parts and assemblies')"
    "APPLICATION_CONTEXT('secondary configuration context')"
    "PRODUCT_CONCEPT('concept-2','Concept 2','second retained concept'"
    "EFFECTIVITY('second effectivity')")
  require_text("${multi_step_text}" "${expected}"
    "multi-effectivity AP203 output")
endforeach()
string(REGEX MATCHALL "CONFIGURATION_EFFECTIVITY\\(" multi_effectivities
  "${multi_step_text}")
list(LENGTH multi_effectivities multi_effectivity_count)
if(NOT multi_effectivity_count EQUAL 2)
  message(FATAL_ERROR
    "multi-effectivity AP203 output has ${multi_effectivity_count} complex instances")
endif()
string(REGEX MATCHALL "APPLICATION_PROTOCOL_DEFINITION\\(" multi_protocols
  "${multi_step_text}")
list(LENGTH multi_protocols multi_protocol_count)
if(NOT multi_protocol_count EQUAL 2)
  message(FATAL_ERROR
    "multi-effectivity AP203 output has ${multi_protocol_count} protocol definitions")
endif()

# The other two AP203 mandatory effectivity subtypes share the same complex
# configuration/product-definition components but have distinct local values.
execute_process(
  COMMAND "${MGED}" -c "${input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#16::COMPONENT_TYPES {CONFIGURATION_EFFECTIVITY EFFECTIVITY PRODUCT_DEFINITION_EFFECTIVITY SERIAL_NUMBERED_EFFECTIVITY} STEP::AP203::CONFIGURATION::#16::VALUE {(CONFIGURATION_EFFECTIVITY(#11) EFFECTIVITY('serial effectivity') PRODUCT_DEFINITION_EFFECTIVITY(#10) SERIAL_NUMBERED_EFFECTIVITY('SN-100','SN-199'))} STEP::AP203::CONFIGURATION::#16::REFERENCES {11 10}"
  RESULT_VARIABLE serial_mutate_result
  OUTPUT_VARIABLE serial_mutate_output
  ERROR_VARIABLE serial_mutate_error
)
if(NOT serial_mutate_result EQUAL 0)
  message(FATAL_ERROR
    "could not create AP203 serial complex fixture:\n"
    "${serial_mutate_output}${serial_mutate_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict --report "${serial_report}"
    -o "${serial_step}" "${input}" all.g
  RESULT_VARIABLE serial_result
  OUTPUT_VARIABLE serial_output
  ERROR_VARIABLE serial_error
)
if(NOT serial_result EQUAL 0 OR NOT EXISTS "${serial_step}")
  message(FATAL_ERROR
    "AP203 serial complex export failed (${serial_result}):\n"
    "${serial_output}${serial_error}")
endif()
file(READ "${serial_step}" serial_step_text)
foreach(expected
    "CONFIGURATION_EFFECTIVITY("
    "EFFECTIVITY('serial effectivity')"
    "PRODUCT_DEFINITION_EFFECTIVITY("
    "SERIAL_NUMBERED_EFFECTIVITY('SN-100','SN-199')")
  require_text("${serial_step_text}" "${expected}"
    "AP203 serial complex output")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#30::TYPE DIMENSIONAL_EXPONENTS STEP::AP203::CONFIGURATION::#30::VALUE {DIMENSIONAL_EXPONENTS(0.,0.,0.,0.,0.,0.,0.)} STEP::AP203::CONFIGURATION::#30::REFERENCES {} STEP::AP203::CONFIGURATION::#31::TYPE CONTEXT_DEPENDENT_UNIT STEP::AP203::CONFIGURATION::#31::VALUE {CONTEXT_DEPENDENT_UNIT(#30,'EA')} STEP::AP203::CONFIGURATION::#31::REFERENCES 30 STEP::AP203::CONFIGURATION::#32::TYPE MEASURE_WITH_UNIT STEP::AP203::CONFIGURATION::#32::VALUE {MEASURE_WITH_UNIT(COUNT_MEASURE(250.),#31)} STEP::AP203::CONFIGURATION::#32::REFERENCES 31 STEP::AP203::CONFIGURATION::#16::COMPONENT_TYPES {CONFIGURATION_EFFECTIVITY EFFECTIVITY LOT_EFFECTIVITY PRODUCT_DEFINITION_EFFECTIVITY} STEP::AP203::CONFIGURATION::#16::VALUE {(CONFIGURATION_EFFECTIVITY(#11) EFFECTIVITY('lot effectivity') LOT_EFFECTIVITY('BATCH-42',#32) PRODUCT_DEFINITION_EFFECTIVITY(#10))} STEP::AP203::CONFIGURATION::#16::REFERENCES {11 32 10}"
  RESULT_VARIABLE lot_mutate_result
  OUTPUT_VARIABLE lot_mutate_output
  ERROR_VARIABLE lot_mutate_error
)
if(NOT lot_mutate_result EQUAL 0)
  message(FATAL_ERROR
    "could not create AP203 lot complex fixture:\n"
    "${lot_mutate_output}${lot_mutate_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict --report "${lot_report}"
    -o "${lot_step}" "${input}" all.g
  RESULT_VARIABLE lot_result
  OUTPUT_VARIABLE lot_output
  ERROR_VARIABLE lot_error
)
if(NOT lot_result EQUAL 0 OR NOT EXISTS "${lot_step}")
  message(FATAL_ERROR
    "AP203 lot complex export failed (${lot_result}):\n"
    "${lot_output}${lot_error}")
endif()
file(READ "${lot_step}" lot_step_text)
foreach(expected
    "CONFIGURATION_EFFECTIVITY("
    "EFFECTIVITY('lot effectivity')"
    "LOT_EFFECTIVITY('BATCH-42',#"
    "PRODUCT_DEFINITION_EFFECTIVITY("
    "MEASURE_WITH_UNIT(COUNT_MEASURE(250.)")
  require_text("${lot_step_text}" "${expected}"
    "AP203 lot complex output")
endforeach()

file(READ "${step}" step_text)
foreach(expected
    "APPLICATION_PROTOCOL_DEFINITION("
    "PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE("
    "PRODUCT_RELATED_PRODUCT_CATEGORY('machined','retained process category'"
    "PRODUCT_RELATED_PRODUCT_CATEGORY('detail','generated AP203 product classification'"
    "PRODUCT_CONCEPT_CONTEXT('market'"
    "PRODUCT_CONCEPT('concept','Concept','retention fixture'"
    "CONFIGURATION_ITEM('configuration','Configuration'"
    "CONFIGURATION_DESIGN("
    "CONFIGURATION_EFFECTIVITY("
    "DATED_EFFECTIVITY("
    "EFFECTIVITY('effectivity')"
    "PRODUCT_DEFINITION_EFFECTIVITY("
    "CC_DESIGN_APPROVAL("
    "PERSON_AND_ORGANIZATION_ROLE('configuration_manager')")
  require_text("${step_text}" "${expected}" "AP203 complex output")
endforeach()
foreach(forbidden
    "CONFIGURATION_DESIGN(#4,#7)"
    "CONFIGURATION_EFFECTIVITY(#11)"
    "DATED_EFFECTIVITY(#15,$)"
    "PRODUCT_DEFINITION_EFFECTIVITY(#10)")
  reject_text("${step_text}" "${forbidden}"
    "AP203 source-identifier remapping")
endforeach()

execute_process(
  COMMAND "${STEP_G}" --schema ap203 --strict -O "${roundtrip}"
    --report "${import_report}" "${step}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${roundtrip}")
  message(FATAL_ERROR
    "AP203 complex output did not reimport (${import_result}):\n"
    "${import_output}${import_error}")
endif()
file(READ "${import_report}" import_report_text)
foreach(expected
    "\"products\":2"
    "\"occurrences\":1"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"type\":\"COMPLEX\",\"component_types\":[\"CONFIGURATION_EFFECTIVITY\",\"DATED_EFFECTIVITY\",\"EFFECTIVITY\",\"PRODUCT_DEFINITION_EFFECTIVITY\"]")
  require_text("${import_report_text}" "${expected}"
    "AP203 complex reimport report")
endforeach()

# A malformed mandatory subtype must be reported without leaving a partial
# complex in permissive output; strict mode must publish no file.
execute_process(
  COMMAND "${MGED}" -c "${input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#16::COMPONENT_TYPES {CONFIGURATION_EFFECTIVITY DATED_EFFECTIVITY EFFECTIVITY PRODUCT_DEFINITION_EFFECTIVITY} STEP::AP203::CONFIGURATION::#16::VALUE {(CONFIGURATION_EFFECTIVITY(#11) DATED_EFFECTIVITY($,$) EFFECTIVITY('effectivity') PRODUCT_DEFINITION_EFFECTIVITY(#10))} STEP::AP203::CONFIGURATION::#16::REFERENCES {11 10}"
  RESULT_VARIABLE mutate_result
  OUTPUT_VARIABLE mutate_output
  ERROR_VARIABLE mutate_error
)
if(NOT mutate_result EQUAL 0)
  message(FATAL_ERROR
    "could not create malformed AP203 complex fixture:\n"
    "${mutate_output}${mutate_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${malformed_report}"
    -o "${malformed_step}" "${input}" all.g
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_output
  ERROR_VARIABLE malformed_error
)
if(NOT malformed_result EQUAL 1 OR NOT EXISTS "${malformed_step}")
  message(FATAL_ERROR
    "malformed AP203 complex policy failed (${malformed_result}):\n"
    "${malformed_output}${malformed_error}")
endif()
file(READ "${malformed_report}" malformed_report_text)
require_text("${malformed_report_text}"
  "AP203 DATED_EFFECTIVITY component has invalid bounds"
  "malformed AP203 complex report")
file(READ "${malformed_step}" malformed_step_text)
reject_text("${malformed_step_text}" "CONFIGURATION_EFFECTIVITY("
  "malformed AP203 complex output")

execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict -o "${strict_step}"
    "${input}" all.g
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_output
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 4 OR EXISTS "${strict_step}")
  message(FATAL_ERROR
    "strict malformed AP203 complex export was not transactional "
    "(${strict_result}):\n${strict_output}${strict_error}")
endif()
