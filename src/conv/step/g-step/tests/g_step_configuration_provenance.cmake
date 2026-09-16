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

set(input "${OUTPUT_DIR}/g_step_configuration_provenance.g")
file(REMOVE "${input}")
string(CONCAT create_command
  "in provenance.s sph 0 0 0 5; r provenance.r u provenance.s; "
  "attr set provenance.r step:source_id 200 "
  "step:formation_source_id 201 step:definition_source_id 202; "
  "attr set _GLOBAL "
  "STEP::AP203::CONFIGURATION::#100::TYPE ORGANIZATION "
  "STEP::AP203::CONFIGURATION::#100::VALUE {ORGANIZATION('ORG-1','Example Organization','retained organization')} "
  "STEP::AP203::CONFIGURATION::#100::REFERENCES {} "
  "STEP::AP203::CONFIGURATION::#101::TYPE PERSON "
  "STEP::AP203::CONFIGURATION::#101::VALUE {PERSON('P-1','Doe','Jane',$,$,$)} "
  "STEP::AP203::CONFIGURATION::#101::REFERENCES {} "
  "STEP::AP203::CONFIGURATION::#102::TYPE PERSON_AND_ORGANIZATION "
  "STEP::AP203::CONFIGURATION::#102::VALUE {PERSON_AND_ORGANIZATION(#101,#100)} "
  "STEP::AP203::CONFIGURATION::#102::REFERENCES {101 100} "
  "STEP::AP203::CONFIGURATION::#103::TYPE SECURITY_CLASSIFICATION_LEVEL "
  "STEP::AP203::CONFIGURATION::#103::VALUE {SECURITY_CLASSIFICATION_LEVEL('confidential')} "
  "STEP::AP203::CONFIGURATION::#103::REFERENCES {} "
  "STEP::AP203::CONFIGURATION::#104::TYPE SECURITY_CLASSIFICATION "
  "STEP::AP203::CONFIGURATION::#104::VALUE {SECURITY_CLASSIFICATION('SC-1','retained classification',#103)} "
  "STEP::AP203::CONFIGURATION::#104::REFERENCES 103 "
  "STEP::AP203::CONFIGURATION::#105::TYPE CC_DESIGN_SECURITY_CLASSIFICATION "
  "STEP::AP203::CONFIGURATION::#105::VALUE {CC_DESIGN_SECURITY_CLASSIFICATION(#104,(#201))} "
  "STEP::AP203::CONFIGURATION::#105::REFERENCES {104 201} "
  "STEP::AP203::CONFIGURATION::#110::TYPE COORDINATED_UNIVERSAL_TIME_OFFSET "
  "STEP::AP203::CONFIGURATION::#110::VALUE {COORDINATED_UNIVERSAL_TIME_OFFSET(0,$,.AHEAD.)} "
  "STEP::AP203::CONFIGURATION::#110::REFERENCES {} "
  "STEP::AP203::CONFIGURATION::#111::TYPE LOCAL_TIME "
  "STEP::AP203::CONFIGURATION::#111::VALUE {LOCAL_TIME(12,30,0.,#110)} "
  "STEP::AP203::CONFIGURATION::#111::REFERENCES 110 "
  "STEP::AP203::CONFIGURATION::#112::TYPE CALENDAR_DATE "
  "STEP::AP203::CONFIGURATION::#112::VALUE {CALENDAR_DATE(2028,1,2)} "
  "STEP::AP203::CONFIGURATION::#112::REFERENCES {} "
  "STEP::AP203::CONFIGURATION::#113::TYPE DATE_AND_TIME "
  "STEP::AP203::CONFIGURATION::#113::VALUE {DATE_AND_TIME(#112,#111)} "
  "STEP::AP203::CONFIGURATION::#113::REFERENCES {112 111} "
  "STEP::AP203::CONFIGURATION::#114::TYPE DATE_TIME_ROLE "
  "STEP::AP203::CONFIGURATION::#114::VALUE {DATE_TIME_ROLE('creation_date')} "
  "STEP::AP203::CONFIGURATION::#114::REFERENCES {} "
  "STEP::AP203::CONFIGURATION::#115::TYPE CC_DESIGN_DATE_AND_TIME_ASSIGNMENT "
  "STEP::AP203::CONFIGURATION::#115::VALUE {CC_DESIGN_DATE_AND_TIME_ASSIGNMENT(#113,#114,(#202))} "
  "STEP::AP203::CONFIGURATION::#115::REFERENCES {113 114 202} "
  "STEP::AP203::CONFIGURATION::#120::TYPE APPROVAL_STATUS "
  "STEP::AP203::CONFIGURATION::#120::VALUE {APPROVAL_STATUS('approved')} "
  "STEP::AP203::CONFIGURATION::#120::REFERENCES {} "
  "STEP::AP203::CONFIGURATION::#121::TYPE APPROVAL "
  "STEP::AP203::CONFIGURATION::#121::VALUE {APPROVAL(#120,'retained release')} "
  "STEP::AP203::CONFIGURATION::#121::REFERENCES 120 "
  "STEP::AP203::CONFIGURATION::#122::TYPE CC_DESIGN_APPROVAL "
  "STEP::AP203::CONFIGURATION::#122::VALUE {CC_DESIGN_APPROVAL(#121,(#202))} "
  "STEP::AP203::CONFIGURATION::#122::REFERENCES {121 202} "
  "STEP::AP203::CONFIGURATION::#123::TYPE APPROVAL_ROLE "
  "STEP::AP203::CONFIGURATION::#123::VALUE {APPROVAL_ROLE('authorizer')} "
  "STEP::AP203::CONFIGURATION::#123::REFERENCES {} "
  "STEP::AP203::CONFIGURATION::#124::TYPE APPROVAL_PERSON_ORGANIZATION "
  "STEP::AP203::CONFIGURATION::#124::VALUE {APPROVAL_PERSON_ORGANIZATION(#102,#121,#123)} "
  "STEP::AP203::CONFIGURATION::#124::REFERENCES {102 121 123} "
  "STEP::AP203::CONFIGURATION::#125::TYPE APPROVAL_DATE_TIME "
  "STEP::AP203::CONFIGURATION::#125::VALUE {APPROVAL_DATE_TIME(#113,#121)} "
  "STEP::AP203::CONFIGURATION::#125::REFERENCES {113 121}"
)
execute_process(
  COMMAND "${MGED}" -c "${input}" "${create_command}"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create configuration-provenance fixture:\n"
    "${create_output}${create_error}")
endif()

foreach(schema ap203 ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  if(schema STREQUAL "ap203")
    set(date_assignment_type "CC_DESIGN_DATE_AND_TIME_ASSIGNMENT")
    set(approval_assignment_type "CC_DESIGN_APPROVAL")
  else()
    set(date_assignment_type "APPLIED_DATE_AND_TIME_ASSIGNMENT")
    set(approval_assignment_type "APPLIED_APPROVAL_ASSIGNMENT")
  endif()
  set(step "${OUTPUT_DIR}/g_step_configuration_provenance_${schema}.stp")
  set(export_report
    "${OUTPUT_DIR}/g_step_configuration_provenance_${schema}_export.json")
  set(roundtrip
    "${OUTPUT_DIR}/g_step_configuration_provenance_${schema}.roundtrip.g")
  set(import_report
    "${OUTPUT_DIR}/g_step_configuration_provenance_${schema}_import.json")
  file(REMOVE "${step}" "${export_report}" "${roundtrip}" "${import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${export_report}" -o "${step}" "${input}" provenance.r
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result EQUAL 0 OR NOT EXISTS "${step}")
    message(FATAL_ERROR
      "${schema} provenance export failed (${export_result}):\n"
      "${export_output}${export_error}")
  endif()
  file(READ "${export_report}" export_report_text)
  foreach(expected
      "\"configuration_records_seen\":18"
      "\"configuration_records_emitted\":18"
      "\"configuration_records_omitted\":0"
      "authored as a retained date-time role"
      "with remapped date-time, role, and item references"
      "authored as a retained approval role"
      "with remapped identity, approval, and role references"
      "with remapped date-and-time and approval references")
    require_text("${export_report_text}" "${expected}"
      "${schema} provenance export report")
  endforeach()
  file(READ "${step}" step_text)
  foreach(expected
      "DATE_TIME_ROLE('creation_date')"
      "${date_assignment_type}("
      "APPROVAL_ROLE('authorizer')"
      "APPROVAL_PERSON_ORGANIZATION("
      "APPROVAL_DATE_TIME("
      "${approval_assignment_type}(")
    require_text("${step_text}" "${expected}" "${schema} provenance output")
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
      "${schema} provenance output did not reimport (${import_result}):\n"
      "${import_output}${import_error}")
  endif()
  file(READ "${import_report}" import_report_text)
  foreach(expected
      "\"type\":\"DATE_TIME_ROLE\""
      "\"type\":\"${date_assignment_type}\""
      "\"type\":\"APPROVAL_ROLE\""
      "\"type\":\"APPROVAL_PERSON_ORGANIZATION\""
      "\"type\":\"APPROVAL_DATE_TIME\""
      "\"type\":\"${approval_assignment_type}\"")
    require_text("${import_report_text}" "${expected}"
      "${schema} provenance reimport report")
  endforeach()
endforeach()

# AP203 requires exactly one date record for each approval.  Two otherwise
# valid retained dates are both omitted in permissive output so the AP203
# administrative finalizer can supply one deterministic replacement; strict
# output remains transactional.
set(duplicate_input
  "${OUTPUT_DIR}/g_step_configuration_provenance_duplicate.g")
set(duplicate_step
  "${OUTPUT_DIR}/g_step_configuration_provenance_duplicate.stp")
set(duplicate_report
  "${OUTPUT_DIR}/g_step_configuration_provenance_duplicate.json")
set(duplicate_strict_step
  "${OUTPUT_DIR}/g_step_configuration_provenance_duplicate_strict.stp")
file(COPY_FILE "${input}" "${duplicate_input}")
file(REMOVE "${duplicate_step}" "${duplicate_report}"
  "${duplicate_strict_step}")
execute_process(
  COMMAND "${MGED}" -c "${duplicate_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#126::TYPE APPROVAL_DATE_TIME STEP::AP203::CONFIGURATION::#126::VALUE {APPROVAL_DATE_TIME(#113,#121)} STEP::AP203::CONFIGURATION::#126::REFERENCES {113 121}"
  RESULT_VARIABLE duplicate_create_result
)
if(NOT duplicate_create_result EQUAL 0)
  message(FATAL_ERROR "could not create duplicate approval-date fixture")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${duplicate_report}"
    -o "${duplicate_step}" "${duplicate_input}" provenance.r
  RESULT_VARIABLE duplicate_result
  OUTPUT_VARIABLE duplicate_output
  ERROR_VARIABLE duplicate_error
)
if(NOT duplicate_result EQUAL 1 OR NOT EXISTS "${duplicate_step}")
  message(FATAL_ERROR
    "permissive duplicate approval-date export failed (${duplicate_result}):\n"
    "${duplicate_output}${duplicate_error}")
endif()
file(READ "${duplicate_report}" duplicate_report_text)
foreach(expected
    "\"configuration_records_seen\":19"
    "\"configuration_records_emitted\":17"
    "\"configuration_records_omitted\":2"
    "AP203 requires exactly one date-and-time record per approval")
  require_text("${duplicate_report_text}" "${expected}"
    "duplicate approval-date report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict
    -o "${duplicate_strict_step}" "${duplicate_input}" provenance.r
  RESULT_VARIABLE duplicate_strict_result
  OUTPUT_VARIABLE duplicate_strict_output
  ERROR_VARIABLE duplicate_strict_error
)
if(NOT duplicate_strict_result EQUAL 4 OR EXISTS "${duplicate_strict_step}")
  message(FATAL_ERROR
    "strict duplicate approval-date export was not transactional "
    "(${duplicate_strict_result}):\n"
    "${duplicate_strict_output}${duplicate_strict_error}")
endif()

# The modern approval SELECT accepts PERSON directly; AP203 edition 1 requires
# PERSON_AND_ORGANIZATION.  Verify that cross-schema translation respects that
# difference rather than widening AP203 illegally.
set(select_input "${OUTPUT_DIR}/g_step_configuration_provenance_select.g")
set(select_ap203_step
  "${OUTPUT_DIR}/g_step_configuration_provenance_select_ap203.stp")
set(select_ap203_report
  "${OUTPUT_DIR}/g_step_configuration_provenance_select_ap203.json")
set(select_ap242_step
  "${OUTPUT_DIR}/g_step_configuration_provenance_select_ap242.stp")
set(select_ap242_report
  "${OUTPUT_DIR}/g_step_configuration_provenance_select_ap242.json")
file(COPY_FILE "${input}" "${select_input}")
file(REMOVE "${select_ap203_step}" "${select_ap203_report}"
  "${select_ap242_step}" "${select_ap242_report}")
execute_process(
  COMMAND "${MGED}" -c "${select_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#124::VALUE {APPROVAL_PERSON_ORGANIZATION(#101,#121,#123)} STEP::AP203::CONFIGURATION::#124::REFERENCES {101 121 123}"
  RESULT_VARIABLE select_create_result
)
if(NOT select_create_result EQUAL 0)
  message(FATAL_ERROR "could not create approval SELECT fixture")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${select_ap203_report}"
    -o "${select_ap203_step}" "${select_input}" provenance.r
  RESULT_VARIABLE select_ap203_result
)
if(NOT select_ap203_result EQUAL 1 OR NOT EXISTS "${select_ap203_step}")
  message(FATAL_ERROR "AP203 approval SELECT restriction was not reported")
endif()
file(READ "${select_ap203_report}" select_ap203_report_text)
foreach(expected
    "\"configuration_records_emitted\":17"
    "\"configuration_records_omitted\":1"
    "an approval person/organization dependency was not emitted")
  require_text("${select_ap203_report_text}" "${expected}"
    "AP203 approval SELECT report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --strict
    --report "${select_ap242_report}" -o "${select_ap242_step}"
    "${select_input}" provenance.r
  RESULT_VARIABLE select_ap242_result
  OUTPUT_VARIABLE select_ap242_output
  ERROR_VARIABLE select_ap242_error
)
if(NOT select_ap242_result EQUAL 0 OR NOT EXISTS "${select_ap242_step}")
  message(FATAL_ERROR
    "AP242 legal approval PERSON SELECT failed (${select_ap242_result}):\n"
    "${select_ap242_output}${select_ap242_error}")
endif()
file(READ "${select_ap242_report}" select_ap242_report_text)
foreach(expected
    "\"configuration_records_emitted\":18"
    "\"configuration_records_omitted\":0")
  require_text("${select_ap242_report_text}" "${expected}"
    "AP242 approval SELECT report")
endforeach()
