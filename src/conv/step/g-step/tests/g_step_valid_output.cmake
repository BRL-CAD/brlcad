if(NOT DEFINED G_STEP OR NOT DEFINED STEP_G OR NOT DEFINED MGED OR
   NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR "G_STEP, STEP_G, MGED, and OUTPUT_DIR are required")
endif()

set(input "${OUTPUT_DIR}/g_step_valid_output_input.g")
set(step "${OUTPUT_DIR}/g_step_valid_output.stp")
set(roundtrip "${OUTPUT_DIR}/g_step_valid_output_roundtrip.g")
set(report "${OUTPUT_DIR}/g_step_valid_output_roundtrip.json")
file(REMOVE "${input}" "${step}" "${roundtrip}" "${report}")

execute_process(
  COMMAND "${MGED}" -c "${input}"
    "in box.s rpp 0 10 0 20 0 30; r box.r u box.s; put all.g comb region no tree {u {l box.r} {l box.r {1 0 0 20  0 1 0 0  0 0 1 0  0 0 0 1}}}"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create the g-step input database (${create_result}):\n"
    "${create_output}${create_error}")
endif()

execute_process(
  COMMAND "${G_STEP}" -o "${step}" "${input}"
  RESULT_VARIABLE export_result
  OUTPUT_VARIABLE export_output
  ERROR_VARIABLE export_error
)
if(NOT export_result EQUAL 0)
  message(FATAL_ERROR
    "g-step export returned ${export_result}:\n${export_output}${export_error}")
endif()

file(READ "${step}" step_text)
foreach(expected
    "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));\nENDSEC;\nDATA;"
    "NEXT_ASSEMBLY_USAGE_OCCURRENCE("
    "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION("
    "APPLICATION_PROTOCOL_DEFINITION("
    "PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE("
    "CC_DESIGN_APPROVAL("
    "APPROVAL_DATE_TIME("
    "APPROVAL_PERSON_ORGANIZATION("
    "CC_DESIGN_DATE_AND_TIME_ASSIGNMENT("
    "CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT("
    "CC_DESIGN_SECURITY_CLASSIFICATION("
    "PRODUCT_RELATED_PRODUCT_CATEGORY(")
  string(FIND "${step_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "g-step output omits '${expected}':\n${step_text}")
  endif()
endforeach()
string(FIND "${step_text}" "PRODUCT_DEFINITION_FORMATION(" plain_formation)
if(NOT plain_formation EQUAL -1)
  message(FATAL_ERROR
    "AP203 output used the globally forbidden plain product formation")
endif()
if(step_text MATCHES "#[+]?[0]+=")
  message(FATAL_ERROR "g-step emitted the invalid Part 21 instance identifier #0")
endif()

execute_process(
  COMMAND "${STEP_G}" -O "${roundtrip}" --report "${report}" "${step}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "step-g could not reimport g-step output (${import_result}):\n"
    "${import_output}${import_error}")
endif()

file(READ "${report}" report_text)
foreach(expected
    "\"products\":2"
    "\"occurrences\":2"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "round-trip report omits ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${roundtrip}" "get all_g"
  RESULT_VARIABLE tree_result
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
string(APPEND tree_output "${tree_error}")
string(FIND "${tree_output}" "tree {u {l box_r} {l box_r {" duplicate_found)
string(FIND "${tree_output}" "1 0 0 20" transform_found)
if(NOT tree_result EQUAL 0 OR duplicate_found EQUAL -1 OR
   transform_found EQUAL -1)
  message(FATAL_ERROR
    "round trip did not retain two box occurrences and the second transform:\n"
    "result=${tree_result}, duplicate=${duplicate_found}, transform=${transform_found}\n"
    "${tree_output}")
endif()

# The schema-neutral host must apply the same Part 21 and physical-measure
# rules to every enabled mechanical exporter, and each emitted file must be
# consumable through the corresponding import plugin.
foreach(schema ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  set(schema_step "${OUTPUT_DIR}/g_step_valid_output_${schema}.stp")
  set(schema_roundtrip "${OUTPUT_DIR}/g_step_valid_output_${schema}_roundtrip.g")
  set(schema_report "${OUTPUT_DIR}/g_step_valid_output_${schema}_roundtrip.json")
  file(REMOVE "${schema_step}" "${schema_roundtrip}" "${schema_report}")

  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" -o "${schema_step}" "${input}" all.g
    RESULT_VARIABLE schema_export_result
    OUTPUT_VARIABLE schema_export_output
    ERROR_VARIABLE schema_export_error
  )
  if(NOT schema_export_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} export returned ${schema_export_result}:\n"
      "${schema_export_output}${schema_export_error}")
  endif()

  file(READ "${schema_step}" schema_step_text)
  foreach(expected
      "DATA;"
      "UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(0.05)"
      "ADVANCED_BREP_SHAPE_REPRESENTATION(")
    string(FIND "${schema_step_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "${schema} output omits '${expected}':\n${schema_step_text}")
    endif()
  endforeach()
  if(schema_step_text MATCHES "#[+]?[0]+=")
    message(FATAL_ERROR "${schema} emitted the invalid Part 21 instance identifier #0")
  endif()
  if(schema STREQUAL "ap242e4")
    if(NOT schema_step_text MATCHES
        "ADVANCED_BREP_SHAPE_REPRESENTATION\\('[^']*',\\(#[0-9]+\\),#[0-9]+\\)")
      message(FATAL_ERROR
        "AP242e4 output did not populate its narrowed representation.items SELECT")
    endif()
    if(NOT schema_step_text MATCHES
        "ADVANCED_FACE\\('',\\(#[0-9]+\\),#[0-9]+,\\.[TF]\\.\\)")
      message(FATAL_ERROR
        "AP242e4 output did not populate its narrowed face_geometry SELECT")
    endif()
  endif()

  execute_process(
    COMMAND "${STEP_G}" --schema "${schema}" -O "${schema_roundtrip}"
      --report "${schema_report}" "${schema_step}"
    RESULT_VARIABLE schema_import_result
    OUTPUT_VARIABLE schema_import_output
    ERROR_VARIABLE schema_import_error
  )
  if(NOT schema_import_result EQUAL 0)
    message(FATAL_ERROR
      "step-g could not reimport ${schema} output (${schema_import_result}):\n"
      "${schema_import_output}${schema_import_error}")
  endif()

  file(READ "${schema_report}" schema_report_text)
  string(FIND "${schema_report_text}"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    geometry_found)
  if(geometry_found EQUAL -1)
    message(FATAL_ERROR
      "${schema} round-trip did not preserve one geometry item:\n${schema_report_text}")
  endif()
  string(FIND "${schema_report_text}" "\"occurrences\":2" occurrences_found)
  if(occurrences_found EQUAL -1)
    message(FATAL_ERROR
      "${schema} round-trip did not preserve both assembly occurrences:\n"
      "${schema_report_text}")
  endif()
endforeach()
