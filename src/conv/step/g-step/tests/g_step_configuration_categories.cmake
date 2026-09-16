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

set(input "${OUTPUT_DIR}/g_step_configuration_categories.g")
file(REMOVE "${input}")
string(CONCAT create_command
  "in category.s sph 0 0 0 5; r category.r u category.s; "
  "attr set category.r step:source_id 200; "
  "attr set _GLOBAL "
  "STEP::AP214::CONFIGURATION::#100::TYPE PRODUCT_CATEGORY "
  "STEP::AP214::CONFIGURATION::#100::VALUE {PRODUCT_CATEGORY('part','retained root')} "
  "STEP::AP214::CONFIGURATION::#100::REFERENCES {} "
  "STEP::AP214::CONFIGURATION::#101::TYPE PRODUCT_CATEGORY "
  "STEP::AP214::CONFIGURATION::#101::VALUE {PRODUCT_CATEGORY('mechanical','retained child')} "
  "STEP::AP214::CONFIGURATION::#101::REFERENCES {} "
  "STEP::AP214::CONFIGURATION::#102::TYPE PRODUCT_CATEGORY_RELATIONSHIP "
  "STEP::AP214::CONFIGURATION::#102::VALUE {PRODUCT_CATEGORY_RELATIONSHIP('contains','retained hierarchy',#100,#103)} "
  "STEP::AP214::CONFIGURATION::#102::REFERENCES {100 103} "
  "STEP::AP214::CONFIGURATION::#103::TYPE PRODUCT_RELATED_PRODUCT_CATEGORY "
  "STEP::AP214::CONFIGURATION::#103::VALUE {PRODUCT_RELATED_PRODUCT_CATEGORY('detail','retained membership',(#200))} "
  "STEP::AP214::CONFIGURATION::#103::REFERENCES 200"
)
execute_process(
  COMMAND "${MGED}" -c "${input}" "${create_command}"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create configuration-category fixture:\n"
    "${create_output}${create_error}")
endif()

foreach(schema ap203 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  set(step "${OUTPUT_DIR}/g_step_configuration_categories_${schema}.stp")
  set(export_report
    "${OUTPUT_DIR}/g_step_configuration_categories_${schema}_export.json")
  set(roundtrip
    "${OUTPUT_DIR}/g_step_configuration_categories_${schema}.roundtrip.g")
  set(import_report
    "${OUTPUT_DIR}/g_step_configuration_categories_${schema}_import.json")
  file(REMOVE "${step}" "${export_report}" "${roundtrip}" "${import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${export_report}" -o "${step}" "${input}" category.r
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result EQUAL 0 OR NOT EXISTS "${step}")
    message(FATAL_ERROR
      "${schema} category export failed (${export_result}):\n"
      "${export_output}${export_error}")
  endif()
  file(READ "${export_report}" export_report_text)
  foreach(expected
      "\"configuration_records_seen\":4"
      "\"configuration_records_emitted\":4"
      "\"configuration_records_omitted\":0"
      "authored as a retained product-category identity"
      "authored with remapped product-category references"
      "authored with remapped source-product members")
    require_text("${export_report_text}" "${expected}"
      "${schema} category export report")
  endforeach()
  file(READ "${step}" step_text)
  foreach(expected
      "PRODUCT_CATEGORY('part','retained root')"
      "PRODUCT_CATEGORY('mechanical','retained child')"
      "PRODUCT_CATEGORY_RELATIONSHIP('contains','retained hierarchy'"
      "PRODUCT_RELATED_PRODUCT_CATEGORY('detail','retained membership'")
    require_text("${step_text}" "${expected}" "${schema} category output")
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
      "${schema} category output did not reimport (${import_result}):\n"
      "${import_output}${import_error}")
  endif()
  file(READ "${import_report}" import_report_text)
  foreach(expected
      "\"type\":\"PRODUCT_CATEGORY\""
      "\"type\":\"PRODUCT_CATEGORY_RELATIONSHIP\""
      "\"type\":\"PRODUCT_RELATED_PRODUCT_CATEGORY\"")
    require_text("${import_report_text}" "${expected}"
      "${schema} category reimport report")
  endforeach()
endforeach()

# AP203e2 contains PRODUCT_CATEGORY but not PRODUCT_CATEGORY_RELATIONSHIP.
# Preserve the two identities and product membership in permissive output,
# report the hierarchy edge, and keep strict output transactional.
set(e2_step "${OUTPUT_DIR}/g_step_configuration_categories_ap203e2.stp")
set(e2_report
  "${OUTPUT_DIR}/g_step_configuration_categories_ap203e2_export.json")
set(e2_strict_step
  "${OUTPUT_DIR}/g_step_configuration_categories_ap203e2_strict.stp")
file(REMOVE "${e2_step}" "${e2_report}" "${e2_strict_step}")
execute_process(
  COMMAND "${G_STEP}" --schema ap203e2 --report "${e2_report}"
    -o "${e2_step}" "${input}" category.r
  RESULT_VARIABLE e2_result
  OUTPUT_VARIABLE e2_output
  ERROR_VARIABLE e2_error
)
if(NOT e2_result EQUAL 1 OR NOT EXISTS "${e2_step}")
  message(FATAL_ERROR
    "AP203e2 permissive category export failed (${e2_result}):\n"
    "${e2_output}${e2_error}")
endif()
file(READ "${e2_report}" e2_report_text)
foreach(expected
    "\"configuration_records_seen\":4"
    "\"configuration_records_emitted\":3"
    "\"configuration_records_omitted\":1"
    "the target schema has no PRODUCT_CATEGORY_RELATIONSHIP entity")
  require_text("${e2_report_text}" "${expected}"
    "AP203e2 category report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap203e2 --strict
    -o "${e2_strict_step}" "${input}" category.r
  RESULT_VARIABLE e2_strict_result
  OUTPUT_VARIABLE e2_strict_output
  ERROR_VARIABLE e2_strict_error
)
if(NOT e2_strict_result EQUAL 4 OR EXISTS "${e2_strict_step}")
  message(FATAL_ERROR
    "AP203e2 strict category export was not transactional "
    "(${e2_strict_result}):\n${e2_strict_output}${e2_strict_error}")
endif()

# AP203 edition 1 requires the relationship description which the modern APs
# leave optional.
set(optional_input
  "${OUTPUT_DIR}/g_step_configuration_categories_optional.g")
set(optional_ap203_step
  "${OUTPUT_DIR}/g_step_configuration_categories_optional_ap203.stp")
set(optional_ap203_report
  "${OUTPUT_DIR}/g_step_configuration_categories_optional_ap203.json")
set(optional_ap214_step
  "${OUTPUT_DIR}/g_step_configuration_categories_optional_ap214.stp")
file(COPY_FILE "${input}" "${optional_input}")
file(REMOVE "${optional_ap203_step}" "${optional_ap203_report}"
  "${optional_ap214_step}")
execute_process(
  COMMAND "${MGED}" -c "${optional_input}"
    "attr set _GLOBAL STEP::AP214::CONFIGURATION::#102::VALUE {PRODUCT_CATEGORY_RELATIONSHIP('contains',$,#100,#103)}"
  RESULT_VARIABLE optional_create_result
)
if(NOT optional_create_result EQUAL 0)
  message(FATAL_ERROR "could not create optional-description fixture")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${optional_ap203_report}"
    -o "${optional_ap203_step}" "${optional_input}" category.r
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
require_text("${optional_ap203_report_text}"
  "AP203 edition 1 requires a product-category relationship description"
  "AP203 category-description restriction")
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --strict -o "${optional_ap214_step}"
    "${optional_input}" category.r
  RESULT_VARIABLE optional_ap214_result
  OUTPUT_VARIABLE optional_ap214_output
  ERROR_VARIABLE optional_ap214_error
)
if(NOT optional_ap214_result EQUAL 0 OR NOT EXISTS "${optional_ap214_step}")
  message(FATAL_ERROR
    "AP214 optional-description export failed (${optional_ap214_result}):\n"
    "${optional_ap214_output}${optional_ap214_error}")
endif()

# Reject all edges participating in a category cycle instead of publishing a
# graph which violates the schema's acyclic relationship rule.
set(cycle_input "${OUTPUT_DIR}/g_step_configuration_categories_cycle.g")
set(cycle_step "${OUTPUT_DIR}/g_step_configuration_categories_cycle.stp")
set(cycle_report "${OUTPUT_DIR}/g_step_configuration_categories_cycle.json")
set(cycle_strict_step
  "${OUTPUT_DIR}/g_step_configuration_categories_cycle_strict.stp")
file(COPY_FILE "${input}" "${cycle_input}")
file(REMOVE "${cycle_step}" "${cycle_report}" "${cycle_strict_step}")
execute_process(
  COMMAND "${MGED}" -c "${cycle_input}"
    "attr set _GLOBAL STEP::AP214::CONFIGURATION::#105::TYPE PRODUCT_CATEGORY_RELATIONSHIP STEP::AP214::CONFIGURATION::#105::VALUE {PRODUCT_CATEGORY_RELATIONSHIP('reverse','cycle',#103,#100)} STEP::AP214::CONFIGURATION::#105::REFERENCES {103 100}"
  RESULT_VARIABLE cycle_create_result
)
if(NOT cycle_create_result EQUAL 0)
  message(FATAL_ERROR "could not create product-category cycle fixture")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --report "${cycle_report}"
    -o "${cycle_step}" "${cycle_input}" category.r
  RESULT_VARIABLE cycle_result
  OUTPUT_VARIABLE cycle_output
  ERROR_VARIABLE cycle_error
)
if(NOT cycle_result EQUAL 1 OR NOT EXISTS "${cycle_step}")
  message(FATAL_ERROR
    "permissive category-cycle export failed (${cycle_result}):\n"
    "${cycle_output}${cycle_error}")
endif()
file(READ "${cycle_report}" cycle_report_text)
foreach(expected
    "\"configuration_records_seen\":5"
    "\"configuration_records_emitted\":3"
    "\"configuration_records_omitted\":2"
    "the retained product-category relationships contain a cycle")
  require_text("${cycle_report_text}" "${expected}"
    "product-category cycle report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --strict
    -o "${cycle_strict_step}" "${cycle_input}" category.r
  RESULT_VARIABLE cycle_strict_result
  OUTPUT_VARIABLE cycle_strict_output
  ERROR_VARIABLE cycle_strict_error
)
if(NOT cycle_strict_result EQUAL 4 OR EXISTS "${cycle_strict_step}")
  message(FATAL_ERROR
    "strict category-cycle export was not transactional "
    "(${cycle_strict_result}):\n${cycle_strict_output}${cycle_strict_error}")
endif()
