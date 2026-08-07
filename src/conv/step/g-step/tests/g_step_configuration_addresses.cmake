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

set(input "${OUTPUT_DIR}/g_step_configuration_addresses.g")
file(REMOVE "${input}")
string(CONCAT create_command
  "in address.s sph 0 0 0 5; r address.r u address.s; "
  "attr set _GLOBAL "
  "STEP::AP214::CONFIGURATION::#100::TYPE PERSON "
  "STEP::AP214::CONFIGURATION::#100::VALUE {PERSON('P-ADDRESS','Doe','Jane',$,$,$)} "
  "STEP::AP214::CONFIGURATION::#100::REFERENCES {} "
  "STEP::AP214::CONFIGURATION::#101::TYPE ORGANIZATION "
  "STEP::AP214::CONFIGURATION::#101::VALUE {ORGANIZATION('ORG-ADDRESS','Example Organization','retained organization')} "
  "STEP::AP214::CONFIGURATION::#101::REFERENCES {} "
  "STEP::AP214::CONFIGURATION::#102::TYPE ADDRESS "
  "STEP::AP214::CONFIGURATION::#102::VALUE {ADDRESS('Laboratory',$,$,$,$,$,$,$,$,$,$,$)} "
  "STEP::AP214::CONFIGURATION::#102::REFERENCES {} "
  "STEP::AP214::CONFIGURATION::#103::TYPE PERSONAL_ADDRESS "
  "STEP::AP214::CONFIGURATION::#103::VALUE {PERSONAL_ADDRESS($,$,$,$,$,$,$,$,$,'555-0100','jane@example.test',$,(#100),'home')} "
  "STEP::AP214::CONFIGURATION::#103::REFERENCES 100 "
  "STEP::AP214::CONFIGURATION::#104::TYPE ORGANIZATIONAL_ADDRESS "
  "STEP::AP214::CONFIGURATION::#104::VALUE {ORGANIZATIONAL_ADDRESS($,'42','Example Way',$,'Town','State','12345','US',$,$,$,$,(#101),'office')} "
  "STEP::AP214::CONFIGURATION::#104::REFERENCES 101"
)
execute_process(
  COMMAND "${MGED}" -c "${input}" "${create_command}"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create configuration-address fixture:\n"
    "${create_output}${create_error}")
endif()

foreach(schema ap203 ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  set(step "${OUTPUT_DIR}/g_step_configuration_addresses_${schema}.stp")
  set(export_report
    "${OUTPUT_DIR}/g_step_configuration_addresses_${schema}_export.json")
  set(roundtrip
    "${OUTPUT_DIR}/g_step_configuration_addresses_${schema}.roundtrip.g")
  set(import_report
    "${OUTPUT_DIR}/g_step_configuration_addresses_${schema}_import.json")
  file(REMOVE "${step}" "${export_report}" "${roundtrip}" "${import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${export_report}" -o "${step}" "${input}" address.r
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result EQUAL 0 OR NOT EXISTS "${step}")
    message(FATAL_ERROR
      "${schema} address export failed (${export_result}):\n"
      "${export_output}${export_error}")
  endif()
  file(READ "${export_report}" export_report_text)
  foreach(expected
      "\"configuration_records_seen\":5"
      "\"configuration_records_emitted\":5"
      "\"configuration_records_omitted\":0"
      "authored as a retained address"
      "authored with remapped person references"
      "authored with remapped organization references")
    require_text("${export_report_text}" "${expected}"
      "${schema} address export report")
  endforeach()
  file(READ "${step}" step_text)
  foreach(expected
      "ADDRESS('Laboratory',$,$,$,$,$,$,$,$,$,$,$)"
      "PERSONAL_ADDRESS($,$,$,$,$,$,$,$,$,'555-0100','jane@example.test',$"
      "ORGANIZATIONAL_ADDRESS($,'42','Example Way',$,'Town','State','12345','US'"
      "'home')"
      "'office')")
    require_text("${step_text}" "${expected}" "${schema} address output")
  endforeach()
  foreach(forbidden
      "(#100),'home'"
      "(#101),'office'")
    reject_text("${step_text}" "${forbidden}"
      "${schema} address source-identifier remapping")
  endforeach()

  execute_process(
    COMMAND "${STEP_G}" --schema "${schema}" --strict
      --report "${import_report}" -O "${roundtrip}" "${step}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_output
    ERROR_VARIABLE import_error
  )
  if(NOT import_result EQUAL 0 OR NOT EXISTS "${roundtrip}")
    message(FATAL_ERROR
      "${schema} address output did not reimport (${import_result}):\n"
      "${import_output}${import_error}")
  endif()
  file(READ "${import_report}" import_report_text)
  foreach(expected
      "\"type\":\"ADDRESS\""
      "\"type\":\"PERSONAL_ADDRESS\""
      "\"type\":\"ORGANIZATIONAL_ADDRESS\"")
    require_text("${import_report_text}" "${expected}"
      "${schema} address reimport report")
  endforeach()
endforeach()

# ADDRESS requires at least one populated contact or location field.  A
# malformed retained identity is omitted in permissive mode and keeps strict
# output transactional.
set(malformed_input
  "${OUTPUT_DIR}/g_step_configuration_addresses_malformed.g")
set(malformed_step
  "${OUTPUT_DIR}/g_step_configuration_addresses_malformed.stp")
set(malformed_report
  "${OUTPUT_DIR}/g_step_configuration_addresses_malformed.json")
set(malformed_strict_step
  "${OUTPUT_DIR}/g_step_configuration_addresses_malformed_strict.stp")
file(COPY_FILE "${input}" "${malformed_input}")
file(REMOVE "${malformed_step}" "${malformed_report}"
  "${malformed_strict_step}")
execute_process(
  COMMAND "${MGED}" -c "${malformed_input}"
    "attr set _GLOBAL STEP::AP214::CONFIGURATION::#105::TYPE ADDRESS STEP::AP214::CONFIGURATION::#105::VALUE {ADDRESS($,$,$,$,$,$,$,$,$,$,$,$)} STEP::AP214::CONFIGURATION::#105::REFERENCES {}"
  RESULT_VARIABLE malformed_create_result
)
if(NOT malformed_create_result EQUAL 0)
  message(FATAL_ERROR "could not create malformed address fixture")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --report "${malformed_report}"
    -o "${malformed_step}" "${malformed_input}" address.r
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_output
  ERROR_VARIABLE malformed_error
)
if(NOT malformed_result EQUAL 1 OR NOT EXISTS "${malformed_step}")
  message(FATAL_ERROR
    "permissive malformed address export failed (${malformed_result}):\n"
    "${malformed_output}${malformed_error}")
endif()
file(READ "${malformed_report}" malformed_report_text)
foreach(expected
    "\"configuration_records_seen\":6"
    "\"configuration_records_emitted\":5"
    "\"configuration_records_omitted\":1"
    "ADDRESS has an invalid retained layout")
  require_text("${malformed_report_text}" "${expected}"
    "malformed address report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --strict
    -o "${malformed_strict_step}" "${malformed_input}" address.r
  RESULT_VARIABLE malformed_strict_result
  OUTPUT_VARIABLE malformed_strict_output
  ERROR_VARIABLE malformed_strict_error
)
if(NOT malformed_strict_result EQUAL 4 OR EXISTS "${malformed_strict_step}")
  message(FATAL_ERROR
    "strict malformed address export was not transactional "
    "(${malformed_strict_result}):\n"
    "${malformed_strict_output}${malformed_strict_error}")
endif()

# AP203 edition 1 requires subtype descriptions; the modern schemas make them
# optional.
set(optional_input
  "${OUTPUT_DIR}/g_step_configuration_addresses_optional.g")
set(optional_ap203_step
  "${OUTPUT_DIR}/g_step_configuration_addresses_optional_ap203.stp")
set(optional_ap203_report
  "${OUTPUT_DIR}/g_step_configuration_addresses_optional_ap203.json")
set(optional_modern_step
  "${OUTPUT_DIR}/g_step_configuration_addresses_optional_ap242e4.stp")
file(COPY_FILE "${input}" "${optional_input}")
file(REMOVE "${optional_ap203_step}" "${optional_ap203_report}"
  "${optional_modern_step}")
execute_process(
  COMMAND "${MGED}" -c "${optional_input}"
    "attr set _GLOBAL STEP::AP214::CONFIGURATION::#103::VALUE {PERSONAL_ADDRESS($,$,$,$,$,$,$,$,$,'555-0100','jane@example.test',$,(#100),$)} STEP::AP214::CONFIGURATION::#104::VALUE {ORGANIZATIONAL_ADDRESS($,'42','Example Way',$,'Town','State','12345','US',$,$,$,$,(#101),$)}"
  RESULT_VARIABLE optional_create_result
)
if(NOT optional_create_result EQUAL 0)
  message(FATAL_ERROR "could not create optional-description fixture")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${optional_ap203_report}"
    -o "${optional_ap203_step}" "${optional_input}" address.r
  RESULT_VARIABLE optional_ap203_result
  OUTPUT_VARIABLE optional_ap203_output
  ERROR_VARIABLE optional_ap203_error
)
if(NOT optional_ap203_result EQUAL 1 OR NOT EXISTS "${optional_ap203_step}")
  message(FATAL_ERROR
    "AP203 optional-description export failed (${optional_ap203_result}):\n"
    "${optional_ap203_output}${optional_ap203_error}")
endif()
file(READ "${optional_ap203_report}" optional_ap203_report_text)
foreach(expected
    "\"configuration_records_emitted\":3"
    "\"configuration_records_omitted\":2"
    "AP203 edition 1 requires a personal address description"
    "AP203 edition 1 requires an organizational address description")
  require_text("${optional_ap203_report_text}" "${expected}"
    "AP203 address-description restriction")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --strict
    -o "${optional_modern_step}" "${optional_input}" address.r
  RESULT_VARIABLE optional_modern_result
  OUTPUT_VARIABLE optional_modern_output
  ERROR_VARIABLE optional_modern_error
)
if(NOT optional_modern_result EQUAL 0 OR NOT EXISTS "${optional_modern_step}")
  message(FATAL_ERROR
    "AP242e4 optional-description export failed (${optional_modern_result}):\n"
    "${optional_modern_output}${optional_modern_error}")
endif()
