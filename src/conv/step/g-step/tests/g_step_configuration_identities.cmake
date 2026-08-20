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

set(input "${OUTPUT_DIR}/g_step_configuration_identities.g")
file(REMOVE "${input}")
execute_process(
  COMMAND "${MGED}" -c "${input}"
    "in identity.s sph 0 0 0 5; r identity.r u identity.s; attr set identity.r step:source_id 200 step:formation_source_id 201 step:definition_source_id 202; attr set _GLOBAL STEP::AP203::CONFIGURATION::#100::TYPE ORGANIZATION STEP::AP203::CONFIGURATION::#100::VALUE {ORGANIZATION('ORG-1','Example Organization','retained organization')} STEP::AP203::CONFIGURATION::#100::REFERENCES {} STEP::AP203::CONFIGURATION::#101::TYPE PERSON STEP::AP203::CONFIGURATION::#101::VALUE {PERSON('P-1','O''Brien','Anne',('Middle'),('Dr'),('III'))} STEP::AP203::CONFIGURATION::#101::REFERENCES {} STEP::AP203::CONFIGURATION::#102::TYPE PERSON_AND_ORGANIZATION STEP::AP203::CONFIGURATION::#102::VALUE {PERSON_AND_ORGANIZATION(#101,#100)} STEP::AP203::CONFIGURATION::#102::REFERENCES {101 100} STEP::AP203::CONFIGURATION::#103::TYPE SECURITY_CLASSIFICATION_LEVEL STEP::AP203::CONFIGURATION::#103::VALUE {SECURITY_CLASSIFICATION_LEVEL('confidential')} STEP::AP203::CONFIGURATION::#103::REFERENCES {} STEP::AP203::CONFIGURATION::#104::TYPE SECURITY_CLASSIFICATION STEP::AP203::CONFIGURATION::#104::VALUE {SECURITY_CLASSIFICATION('SC-1','retained classification',#103)} STEP::AP203::CONFIGURATION::#104::REFERENCES 103 STEP::AP203::CONFIGURATION::#105::TYPE DOCUMENT_TYPE STEP::AP203::CONFIGURATION::#105::VALUE {DOCUMENT_TYPE('design_specification')} STEP::AP203::CONFIGURATION::#105::REFERENCES {} STEP::AP203::CONFIGURATION::#106::TYPE DOCUMENT STEP::AP203::CONFIGURATION::#106::VALUE {DOCUMENT('DOC-1','Fixture','retained document',#105)} STEP::AP203::CONFIGURATION::#106::REFERENCES 105 STEP::AP203::CONFIGURATION::#107::TYPE PERSON_AND_ORGANIZATION_ROLE STEP::AP203::CONFIGURATION::#107::VALUE {PERSON_AND_ORGANIZATION_ROLE('design_owner')} STEP::AP203::CONFIGURATION::#107::REFERENCES {} STEP::AP203::CONFIGURATION::#108::TYPE CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT STEP::AP203::CONFIGURATION::#108::VALUE {CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT(#102,#107,(#200))} STEP::AP203::CONFIGURATION::#108::REFERENCES {102 107 200} STEP::AP203::CONFIGURATION::#109::TYPE CC_DESIGN_SECURITY_CLASSIFICATION STEP::AP203::CONFIGURATION::#109::VALUE {CC_DESIGN_SECURITY_CLASSIFICATION(#104,(#201))} STEP::AP203::CONFIGURATION::#109::REFERENCES {104 201} STEP::AP203::CONFIGURATION::#110::TYPE CC_DESIGN_SPECIFICATION_REFERENCE STEP::AP203::CONFIGURATION::#110::VALUE {CC_DESIGN_SPECIFICATION_REFERENCE(#106,'design source',(#202))} STEP::AP203::CONFIGURATION::#110::REFERENCES {106 202}"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create configuration-identity fixture:\n"
    "${create_output}${create_error}")
endif()

foreach(schema ap203 ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  if(schema STREQUAL "ap203")
    set(person_assignment_type
      "CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT")
    set(security_assignment_type "CC_DESIGN_SECURITY_CLASSIFICATION")
    set(document_reference_type "CC_DESIGN_SPECIFICATION_REFERENCE")
  else()
    set(person_assignment_type
      "APPLIED_PERSON_AND_ORGANIZATION_ASSIGNMENT")
    set(security_assignment_type
      "APPLIED_SECURITY_CLASSIFICATION_ASSIGNMENT")
    set(document_reference_type "APPLIED_DOCUMENT_REFERENCE")
  endif()
  set(step "${OUTPUT_DIR}/g_step_configuration_identities_${schema}.stp")
  set(export_report
    "${OUTPUT_DIR}/g_step_configuration_identities_${schema}_export.json")
  set(roundtrip
    "${OUTPUT_DIR}/g_step_configuration_identities_${schema}.roundtrip.g")
  set(import_report
    "${OUTPUT_DIR}/g_step_configuration_identities_${schema}_import.json")
  file(REMOVE "${step}" "${export_report}" "${roundtrip}"
    "${import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${export_report}" -o "${step}" "${input}" identity.r
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result EQUAL 0 OR NOT EXISTS "${step}")
    message(FATAL_ERROR
      "${schema} identity export failed (${export_result}):\n"
      "${export_output}${export_error}")
  endif()
  file(READ "${export_report}" export_report_text)
  foreach(expected
      "\"configuration_records_seen\":11"
      "\"configuration_records_emitted\":11"
      "\"configuration_records_omitted\":0"
      "\"entity_id\":100,\"type\":\"ORGANIZATION\""
      "\"entity_id\":102,\"type\":\"PERSON_AND_ORGANIZATION\""
      "\"entity_id\":104,\"type\":\"SECURITY_CLASSIFICATION\""
      "\"entity_id\":106,\"type\":\"DOCUMENT\""
      "\"entity_id\":108,\"type\":\"CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT\""
      "\"entity_id\":109,\"type\":\"CC_DESIGN_SECURITY_CLASSIFICATION\""
      "\"entity_id\":110,\"type\":\"CC_DESIGN_SPECIFICATION_REFERENCE\""
      "authored with remapped person and organization references"
      "authored with a remapped security-level reference"
      "authored with a remapped document-type reference"
      "with remapped identity, role, and item references"
      "with remapped classification and item references"
      "with remapped document and item references")
    require_text("${export_report_text}" "${expected}"
      "${schema} identity export report")
  endforeach()
  file(READ "${step}" step_text)
  foreach(expected
      "ORGANIZATION('ORG-1','Example Organization','retained organization')"
      "PERSON('P-1','O''Brien','Anne',('Middle'),('Dr'),('III'))"
      "SECURITY_CLASSIFICATION_LEVEL('confidential')"
      "SECURITY_CLASSIFICATION('SC-1','retained classification'"
      "DOCUMENT_TYPE('design_specification')"
      "DOCUMENT('DOC-1','Fixture','retained document'"
      "PERSON_AND_ORGANIZATION_ROLE('design_owner')"
      "${person_assignment_type}("
      "${security_assignment_type}("
      "${document_reference_type}(")
    require_text("${step_text}" "${expected}"
      "${schema} identity output")
  endforeach()
  foreach(forbidden
      "PERSON_AND_ORGANIZATION(#101,#100)"
      "SECURITY_CLASSIFICATION('SC-1','retained classification',#103)"
      "DOCUMENT('DOC-1','Fixture','retained document',#105)"
      "${person_assignment_type}(#102,#107,(#200))"
      "${security_assignment_type}(#104,(#201))"
      "${document_reference_type}(#106,'design source',(#202))")
    reject_text("${step_text}" "${forbidden}"
      "${schema} source-identifier remapping")
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
      "${schema} identity output did not reimport (${import_result}):\n"
      "${import_output}${import_error}")
  endif()
  file(READ "${import_report}" import_report_text)
  foreach(expected
      "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
      "\"type\":\"ORGANIZATION\""
      "\"type\":\"PERSON\""
      "\"type\":\"PERSON_AND_ORGANIZATION\""
      "\"type\":\"SECURITY_CLASSIFICATION_LEVEL\""
      "\"type\":\"SECURITY_CLASSIFICATION\""
      "\"type\":\"DOCUMENT_TYPE\""
      "\"type\":\"DOCUMENT\""
      "\"type\":\"PERSON_AND_ORGANIZATION_ROLE\""
      "\"type\":\"${person_assignment_type}\""
      "\"type\":\"${security_assignment_type}\""
      "\"type\":\"${document_reference_type}\""
      "O''Brien")
    require_text("${import_report_text}" "${expected}"
      "${schema} identity reimport report")
  endforeach()
endforeach()

# A syntactically complete PERSON with neither required name is malformed.
# Permissive output must retain the eleven valid records, while strict output
# remains transactional.
set(malformed_input
  "${OUTPUT_DIR}/g_step_configuration_identities_malformed.g")
set(malformed_step
  "${OUTPUT_DIR}/g_step_configuration_identities_malformed.stp")
set(malformed_report
  "${OUTPUT_DIR}/g_step_configuration_identities_malformed.json")
set(malformed_strict_step
  "${OUTPUT_DIR}/g_step_configuration_identities_malformed_strict.stp")
file(COPY_FILE "${input}" "${malformed_input}")
file(REMOVE "${malformed_step}" "${malformed_report}"
  "${malformed_strict_step}")
execute_process(
  COMMAND "${MGED}" -c "${malformed_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#111::TYPE PERSON STEP::AP203::CONFIGURATION::#111::VALUE {PERSON('P-BAD',$,$,$,$,$)} STEP::AP203::CONFIGURATION::#111::REFERENCES {}"
  RESULT_VARIABLE malformed_create_result
  OUTPUT_VARIABLE malformed_create_output
  ERROR_VARIABLE malformed_create_error
)
if(NOT malformed_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create malformed identity fixture:\n"
    "${malformed_create_output}${malformed_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --report "${malformed_report}"
    -o "${malformed_step}" "${malformed_input}" identity.r
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_output
  ERROR_VARIABLE malformed_error
)
if(NOT malformed_result EQUAL 1 OR NOT EXISTS "${malformed_step}")
  message(FATAL_ERROR
    "permissive malformed identity export failed (${malformed_result}):\n"
    "${malformed_output}${malformed_error}")
endif()
file(READ "${malformed_report}" malformed_report_text)
foreach(expected
    "\"configuration_records_seen\":12"
    "\"configuration_records_emitted\":11"
    "\"configuration_records_omitted\":1"
    "\"entity_id\":111,\"type\":\"PERSON\""
    "\"status\":\"malformed\",\"reason\":\"PERSON has an invalid retained layout\"")
  require_text("${malformed_report_text}" "${expected}"
    "malformed identity export report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --strict
    -o "${malformed_strict_step}" "${malformed_input}" identity.r
  RESULT_VARIABLE malformed_strict_result
  OUTPUT_VARIABLE malformed_strict_output
  ERROR_VARIABLE malformed_strict_error
)
if(NOT malformed_strict_result EQUAL 4 OR EXISTS "${malformed_strict_step}")
  message(FATAL_ERROR
    "strict malformed identity export was not transactional "
    "(${malformed_strict_result}):\n"
    "${malformed_strict_output}${malformed_strict_error}")
endif()

# Modern schemas permit optional descriptions and producer-defined document
# and classification labels.  AP203 edition 1 instead has closed vocabularies
# and mandatory descriptions, and its dependent type records may not be left
# orphaned when their consumer is omitted.
set(restricted_input
  "${OUTPUT_DIR}/g_step_configuration_identities_restricted.g")
set(restricted_ap203_step
  "${OUTPUT_DIR}/g_step_configuration_identities_restricted_ap203.stp")
set(restricted_ap203_report
  "${OUTPUT_DIR}/g_step_configuration_identities_restricted_ap203.json")
set(restricted_ap203_strict_step
  "${OUTPUT_DIR}/g_step_configuration_identities_restricted_ap203_strict.stp")
set(restricted_modern_step
  "${OUTPUT_DIR}/g_step_configuration_identities_restricted_ap242e4.stp")
set(restricted_modern_report
  "${OUTPUT_DIR}/g_step_configuration_identities_restricted_ap242e4.json")
file(COPY_FILE "${input}" "${restricted_input}")
file(REMOVE "${restricted_ap203_step}" "${restricted_ap203_report}"
  "${restricted_ap203_strict_step}" "${restricted_modern_step}"
  "${restricted_modern_report}")
execute_process(
  COMMAND "${MGED}" -c "${restricted_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#100::VALUE {ORGANIZATION('ORG-1','Example Organization',$)} STEP::AP203::CONFIGURATION::#103::VALUE {SECURITY_CLASSIFICATION_LEVEL('restricted')} STEP::AP203::CONFIGURATION::#105::VALUE {DOCUMENT_TYPE('memo')} STEP::AP203::CONFIGURATION::#106::VALUE {DOCUMENT('DOC-1','Fixture',$,#105)}"
  RESULT_VARIABLE restricted_create_result
  OUTPUT_VARIABLE restricted_create_output
  ERROR_VARIABLE restricted_create_error
)
if(NOT restricted_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create target-restriction fixture:\n"
    "${restricted_create_output}${restricted_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${restricted_ap203_report}"
    -o "${restricted_ap203_step}" "${restricted_input}" identity.r
  RESULT_VARIABLE restricted_ap203_result
  OUTPUT_VARIABLE restricted_ap203_output
  ERROR_VARIABLE restricted_ap203_error
)
if(NOT restricted_ap203_result EQUAL 1 OR
   NOT EXISTS "${restricted_ap203_step}")
  message(FATAL_ERROR
    "AP203 restriction export failed (${restricted_ap203_result}):\n"
    "${restricted_ap203_output}${restricted_ap203_error}")
endif()
file(READ "${restricted_ap203_report}" restricted_ap203_report_text)
foreach(expected
    "\"configuration_records_seen\":11"
    "\"configuration_records_emitted\":1"
    "\"configuration_records_omitted\":10"
    "AP203 edition 1 requires an organization description"
    "AP203 edition 1 forbids the retained security level name"
    "the retained security-level dependency was not emitted"
    "AP203 edition 1 forbids the retained document type name"
    "AP203 edition 1 requires a document description")
  require_text("${restricted_ap203_report_text}" "${expected}"
    "AP203 restriction report")
endforeach()
file(READ "${restricted_ap203_step}" restricted_ap203_text)
foreach(forbidden
    "Example Organization" "SECURITY_CLASSIFICATION_LEVEL('restricted')"
    "DOCUMENT_TYPE('memo')")
  reject_text("${restricted_ap203_text}" "${forbidden}"
    "AP203 restricted identity output")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict
    -o "${restricted_ap203_strict_step}" "${restricted_input}" identity.r
  RESULT_VARIABLE restricted_strict_result
  OUTPUT_VARIABLE restricted_strict_output
  ERROR_VARIABLE restricted_strict_error
)
if(NOT restricted_strict_result EQUAL 4 OR
   EXISTS "${restricted_ap203_strict_step}")
  message(FATAL_ERROR
    "strict AP203 restriction export was not transactional "
    "(${restricted_strict_result}):\n"
    "${restricted_strict_output}${restricted_strict_error}")
endif()

execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --strict
    --report "${restricted_modern_report}" -o "${restricted_modern_step}"
    "${restricted_input}" identity.r
  RESULT_VARIABLE restricted_modern_result
  OUTPUT_VARIABLE restricted_modern_output
  ERROR_VARIABLE restricted_modern_error
)
if(NOT restricted_modern_result EQUAL 0 OR
   NOT EXISTS "${restricted_modern_step}")
  message(FATAL_ERROR
    "AP242e4 legal identity export failed (${restricted_modern_result}):\n"
    "${restricted_modern_output}${restricted_modern_error}")
endif()
file(READ "${restricted_modern_report}" restricted_modern_report_text)
foreach(expected
    "\"configuration_records_emitted\":11"
    "\"configuration_records_omitted\":0")
  require_text("${restricted_modern_report_text}" "${expected}"
    "AP242e4 identity restriction report")
endforeach()
file(READ "${restricted_modern_step}" restricted_modern_text)
foreach(expected
    "ORGANIZATION('ORG-1','Example Organization',$)"
    "SECURITY_CLASSIFICATION_LEVEL('restricted')"
    "DOCUMENT_TYPE('memo')"
    "DOCUMENT('DOC-1','Fixture',$")
  require_text("${restricted_modern_text}" "${expected}"
    "AP242e4 identity restriction output")
endforeach()
