if(NOT DEFINED G_STEP OR NOT DEFINED STEP_G OR NOT DEFINED MGED OR
   NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR "G_STEP, STEP_G, MGED, and OUTPUT_DIR are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

function(reject_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "${description}: unexpectedly contains '${needle}'")
  endif()
endfunction()

set(input "${OUTPUT_DIR}/g_step_metadata_input.g")
file(REMOVE "${input}")
execute_process(
  COMMAND "${MGED}" -c "${input}"
    "in widget.s rpp 0 10 0 20 0 30; r widget.r u widget.s; attr set widget.r step:original_name {Widget O'Brien} step:product_id P-42 step:description {Machined demo} step:revision REV-B step:revision_description {Released revision} step:definition_id DEF-9 step:definition_description {Design definition} step:style_name {Widget finish} step:color_rgb {0.25 0.5 0.75} step:transparency 0.2 step:layers {Machined;Visible}"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create metadata fixture (${create_result}):\n"
    "${create_output}${create_error}")
endif()

foreach(schema ap203 ap203e2 ap214 ap242)
  set(step "${OUTPUT_DIR}/g_step_metadata_${schema}.stp")
  set(export_report "${OUTPUT_DIR}/g_step_metadata_${schema}_export.json")
  set(roundtrip "${OUTPUT_DIR}/g_step_metadata_${schema}.g")
  set(import_report "${OUTPUT_DIR}/g_step_metadata_${schema}_import.json")
  file(REMOVE "${step}" "${export_report}" "${roundtrip}" "${import_report}")

  set(strict_argument --strict)
  set(expected_export_result 0)
  if(schema STREQUAL "ap203")
    set(strict_argument)
    set(expected_export_result 1)
  endif()

  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" ${strict_argument}
      --report "${export_report}" -o "${step}" "${input}" widget.r
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result EQUAL expected_export_result)
    message(FATAL_ERROR
      "${schema} metadata export failed (${export_result}):\n"
      "${export_output}${export_error}")
  endif()

  file(READ "${step}" step_text)
  foreach(expected
      "PRODUCT('P-42','Widget O''Brien','Machined demo'"
      "PRODUCT_DEFINITION('DEF-9','Design definition'")
    require_text("${step_text}" "${expected}" "${schema} product metadata")
  endforeach()
  if(schema STREQUAL "ap203")
    require_text("${step_text}"
      "PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('REV-B','Released revision'"
      "${schema} mandatory formation subtype")
  else()
    require_text("${step_text}"
      "PRODUCT_DEFINITION_FORMATION('REV-B','Released revision'"
      "${schema} product metadata")
  endif()
  file(READ "${export_report}" export_report_text)
  require_text("${export_report_text}" "\"products_updated\":1"
    "${schema} metadata report")

  if(schema STREQUAL "ap203")
    reject_text("${step_text}" "STYLED_ITEM(" "AP203 presentation policy")
    require_text("${export_report_text}" "\"presentation_omitted\":1"
      "AP203 presentation loss report")

    set(strict_step "${OUTPUT_DIR}/g_step_metadata_ap203_strict.stp")
    set(strict_report "${OUTPUT_DIR}/g_step_metadata_ap203_strict.json")
    file(REMOVE "${strict_step}" "${strict_report}")
    execute_process(
      COMMAND "${G_STEP}" --schema ap203 --strict --report "${strict_report}"
        -o "${strict_step}" "${input}" widget.r
      RESULT_VARIABLE strict_result
      OUTPUT_VARIABLE strict_output
      ERROR_VARIABLE strict_error
    )
    if(NOT strict_result EQUAL 4 OR EXISTS "${strict_step}")
      message(FATAL_ERROR
        "AP203 strict export did not transactionally reject presentation loss "
        "(${strict_result}):\n${strict_output}${strict_error}")
    endif()
    continue()
  endif()

  foreach(expected
      "COLOUR_RGB('Widget finish',0.25,0.5,0.75)"
      "SURFACE_STYLE_TRANSPARENT(0.2)"
      "STYLED_ITEM('Widget finish'"
      "PRESENTATION_LAYER_ASSIGNMENT('Machined'"
      "PRESENTATION_LAYER_ASSIGNMENT('Visible'")
    require_text("${step_text}" "${expected}" "${schema} presentation metadata")
  endforeach()
  foreach(expected
      "\"styled_items_emitted\":1"
      "\"layers_emitted\":2"
      "\"presentation_omitted\":0")
    require_text("${export_report_text}" "${expected}"
      "${schema} presentation report")
  endforeach()

  execute_process(
    COMMAND "${STEP_G}" --strict -O "${roundtrip}"
      --report "${import_report}" "${step}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_output
    ERROR_VARIABLE import_error
  )
  if(NOT import_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} metadata reimport failed (${import_result}):\n"
      "${import_output}${import_error}")
  endif()
  file(READ "${import_report}" import_report_text)
  foreach(expected
      "\"styles_extracted\":1"
      "\"styles_applied\":1"
      "\"layers_extracted\":2"
      "\"geometry_written\":1")
    require_text("${import_report_text}" "${expected}"
      "${schema} metadata import report")
  endforeach()

  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}"
      "attr show Widget_O_Brien"
    RESULT_VARIABLE inspect_result
    OUTPUT_VARIABLE inspect_output
    ERROR_VARIABLE inspect_error
  )
  string(APPEND inspect_output "${inspect_error}")
  if(NOT inspect_result EQUAL 0)
    message(FATAL_ERROR
      "could not inspect ${schema} metadata round trip:\n${inspect_output}")
  endif()
  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}"
      "attr show Widget_O_Brien_item"
    RESULT_VARIABLE style_inspect_result
    OUTPUT_VARIABLE style_inspect_output
    ERROR_VARIABLE style_inspect_error
  )
  string(APPEND inspect_output "${style_inspect_output}${style_inspect_error}")
  if(NOT style_inspect_result EQUAL 0)
    message(FATAL_ERROR
      "could not inspect ${schema} presentation round trip:\n${inspect_output}")
  endif()
  foreach(expected P-42 "Machined demo" REV-B DEF-9 "Widget finish"
      "0.25 0.5 0.75" "0.200000" "Machined;Visible")
    require_text("${inspect_output}" "${expected}"
      "${schema} metadata attributes")
  endforeach()
endforeach()

# Exercise the retained configuration subgraphs through every supported
# exporter: approvals and their assignment/application edges, usage-bound
# effectivity, alternate products, usage substitutes, and product categories.
# Verify old entity numbers are not copied and reimport the resulting graph.
set(configuration_input "${OUTPUT_DIR}/g_step_configuration_input.g")
file(REMOVE "${configuration_input}")
execute_process(
  COMMAND "${MGED}" -c "${configuration_input}"
    "in base.s sph -10 0 0 5; r base.r u base.s; in alternate.s sph 10 0 0 5; r alternate.r u alternate.s; r approved.r u base.r u alternate.r; attr set base.r step:source_id 4000 step:formation_source_id 4001 step:definition_source_id 4002; attr set alternate.r step:source_id 5000; attr set approved.r step:occurrence:1:source_id 2002 step:occurrence:2:source_id 2004; attr set _GLOBAL STEP::AP203::CONFIGURATION::#700::TYPE APPROVAL_STATUS STEP::AP203::CONFIGURATION::#700::VALUE {APPROVAL_STATUS('approved')} STEP::AP203::CONFIGURATION::#700::REFERENCES {} STEP::AP203::CONFIGURATION::#701::TYPE APPROVAL STEP::AP203::CONFIGURATION::#701::VALUE {APPROVAL(#700,'release A')} STEP::AP203::CONFIGURATION::#701::REFERENCES 700 STEP::AP203::CONFIGURATION::#702::TYPE APPROVAL_STATUS STEP::AP203::CONFIGURATION::#702::VALUE {APPROVAL_STATUS('superseded')} STEP::AP203::CONFIGURATION::#702::REFERENCES {} STEP::AP203::CONFIGURATION::#703::TYPE APPROVAL STEP::AP203::CONFIGURATION::#703::VALUE {APPROVAL(#702,'release B')} STEP::AP203::CONFIGURATION::#703::REFERENCES 702 STEP::AP203::CONFIGURATION::#720::TYPE ALTERNATE_PRODUCT_RELATIONSHIP STEP::AP203::CONFIGURATION::#720::VALUE {ALTERNATE_PRODUCT_RELATIONSHIP('','',#5000,#4000,'first available')} STEP::AP203::CONFIGURATION::#720::REFERENCES {5000 4000} STEP::AP203::CONFIGURATION::#721::TYPE ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE STEP::AP203::CONFIGURATION::#721::VALUE {ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE('','',#2002,#2004)} STEP::AP203::CONFIGURATION::#721::REFERENCES {2002 2004} STEP::AP203::CONFIGURATION::#722::TYPE PRODUCT_RELATED_PRODUCT_CATEGORY STEP::AP203::CONFIGURATION::#722::VALUE {PRODUCT_RELATED_PRODUCT_CATEGORY('assembly','fixture category',(#4000,#5000))} STEP::AP203::CONFIGURATION::#722::REFERENCES {4000 5000} STEP::AP203::CONFIGURATION::#723::TYPE CC_DESIGN_APPROVAL STEP::AP203::CONFIGURATION::#723::VALUE {CC_DESIGN_APPROVAL(#701,(#4001,#4002))} STEP::AP203::CONFIGURATION::#723::REFERENCES {701 4001 4002} STEP::AP203::CONFIGURATION::#724::TYPE APPROVAL_RELATIONSHIP STEP::AP203::CONFIGURATION::#724::VALUE {APPROVAL_RELATIONSHIP('revision','release succession',#701,#703)} STEP::AP203::CONFIGURATION::#724::REFERENCES {701 703} STEP::AP203::CONFIGURATION::#725::TYPE PRODUCT_DEFINITION_EFFECTIVITY STEP::AP203::CONFIGURATION::#725::VALUE {PRODUCT_DEFINITION_EFFECTIVITY('EFF-A',#2002)} STEP::AP203::CONFIGURATION::#725::REFERENCES 2002 STEP::AP203::CONFIGURATION::#726::TYPE PRODUCT_DEFINITION_EFFECTIVITY STEP::AP203::CONFIGURATION::#726::VALUE {PRODUCT_DEFINITION_EFFECTIVITY('EFF-B',#2004)} STEP::AP203::CONFIGURATION::#726::REFERENCES 2004 STEP::AP203::CONFIGURATION::#727::TYPE EFFECTIVITY_RELATIONSHIP STEP::AP203::CONFIGURATION::#727::VALUE {EFFECTIVITY_RELATIONSHIP('sequence','successive usages',#725,#726)} STEP::AP203::CONFIGURATION::#727::REFERENCES {725 726}"
  RESULT_VARIABLE configuration_create_result
  OUTPUT_VARIABLE configuration_create_output
  ERROR_VARIABLE configuration_create_error
)
if(NOT configuration_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create configuration fixture:\n"
    "${configuration_create_output}${configuration_create_error}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${configuration_input}"
    "attr set _GLOBAL STEP::AP203e2::CONFIGURATION::#728::TYPE SERIAL_NUMBERED_EFFECTIVITY STEP::AP203e2::CONFIGURATION::#728::VALUE {SERIAL_NUMBERED_EFFECTIVITY('SERIAL-A','SN-100','SN-199')} STEP::AP203e2::CONFIGURATION::#728::REFERENCES {} STEP::AP203e2::CONFIGURATION::#729::TYPE APPLIED_EFFECTIVITY_ASSIGNMENT STEP::AP203e2::CONFIGURATION::#729::VALUE {APPLIED_EFFECTIVITY_ASSIGNMENT(#728,(#4000,#4002))} STEP::AP203e2::CONFIGURATION::#729::REFERENCES {728 4000 4002}"
  RESULT_VARIABLE serial_effectivity_create_result
  OUTPUT_VARIABLE serial_effectivity_create_output
  ERROR_VARIABLE serial_effectivity_create_error
)
if(NOT serial_effectivity_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not add serial effectivity fixture:\n"
    "${serial_effectivity_create_output}${serial_effectivity_create_error}")
endif()

foreach(schema ap203 ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  set(configuration_step
    "${OUTPUT_DIR}/g_step_configuration_${schema}.stp")
  set(configuration_export_report
    "${OUTPUT_DIR}/g_step_configuration_${schema}_export.json")
  set(configuration_roundtrip
    "${OUTPUT_DIR}/g_step_configuration_${schema}.g")
  set(configuration_import_report
    "${OUTPUT_DIR}/g_step_configuration_${schema}_import.json")
  file(REMOVE "${configuration_step}" "${configuration_export_report}"
    "${configuration_roundtrip}" "${configuration_import_report}")
  set(configuration_strict_argument --strict)
  set(configuration_expected_result 0)
  if(schema STREQUAL "ap203" OR schema STREQUAL "ap214")
    set(configuration_strict_argument)
    set(configuration_expected_result 1)
  endif()
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" ${configuration_strict_argument}
      --report "${configuration_export_report}" -o "${configuration_step}"
      "${configuration_input}" approved.r
    RESULT_VARIABLE configuration_export_result
    OUTPUT_VARIABLE configuration_export_output
    ERROR_VARIABLE configuration_export_error
  )
  if(NOT configuration_export_result EQUAL configuration_expected_result)
    message(FATAL_ERROR
      "${schema} configuration export failed "
      "(${configuration_export_result}):\n"
      "${configuration_export_output}${configuration_export_error}")
  endif()
  file(READ "${configuration_export_report}"
    configuration_export_report_text)
  foreach(expected
      "\"configuration_records_seen\":14"
      "\"entity_id\":700,\"type\":\"APPROVAL_STATUS\""
      "\"entity_id\":701,\"type\":\"APPROVAL\""
      "\"entity_id\":728,\"type\":\"SERIAL_NUMBERED_EFFECTIVITY\""
      "\"entity_id\":729,\"type\":\"APPLIED_EFFECTIVITY_ASSIGNMENT\""
      "\"status\":\"emitted\"")
    require_text("${configuration_export_report_text}" "${expected}"
      "${schema} configuration export report")
  endforeach()
  if(schema STREQUAL "ap203")
    foreach(expected
        "\"outcome\":\"partial\""
        "\"configuration_records_emitted\":6"
        "\"configuration_records_omitted\":8"
        "AP203 APPROVAL has no authorable CC_DESIGN_APPROVAL assignment"
        "AP203 edition 1 requires effectivity as a complex"
        "AP203 edition 1 has no APPLIED_EFFECTIVITY_ASSIGNMENT entity"
        "an EFFECTIVITY_RELATIONSHIP dependency was not emitted")
      require_text("${configuration_export_report_text}" "${expected}"
        "${schema} configuration export report")
    endforeach()
  elseif(schema STREQUAL "ap214")
    foreach(expected
        "\"outcome\":\"partial\""
        "\"configuration_records_emitted\":12"
        "\"configuration_records_omitted\":2"
        "AP214 forbids serial, lot, and product-definition effectivities in APPLIED_EFFECTIVITY_ASSIGNMENT"
        "AP214 forbids serial, lot, and product-definition effectivities in EFFECTIVITY_RELATIONSHIP")
      require_text("${configuration_export_report_text}" "${expected}"
        "${schema} configuration export report")
    endforeach()
  else()
    foreach(expected
        "\"outcome\":\"complete\""
        "\"configuration_records_emitted\":14"
        "\"configuration_records_omitted\":0")
      require_text("${configuration_export_report_text}" "${expected}"
        "${schema} configuration export report")
    endforeach()
  endif()
  file(READ "${configuration_step}" configuration_step_text)
  require_text("${configuration_step_text}" "APPROVAL_STATUS('approved')"
    "${schema} configuration entities")
  require_text("${configuration_step_text}" "'release A')"
    "${schema} configuration entities")
  foreach(expected
      "ALTERNATE_PRODUCT_RELATIONSHIP('',''"
      "ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE('',''"
      "PRODUCT_RELATED_PRODUCT_CATEGORY('assembly','fixture category'")
    require_text("${configuration_step_text}" "${expected}"
      "${schema} product configuration entities")
  endforeach()
  if(schema STREQUAL "ap203")
    reject_text("${configuration_step_text}"
      "APPROVAL_RELATIONSHIP('revision','release succession'"
      "${schema} incomplete approval relationship")
  else()
    require_text("${configuration_step_text}"
      "APPROVAL_RELATIONSHIP('revision','release succession'"
      "${schema} approval relationship")
  endif()
  if(schema STREQUAL "ap203")
    reject_text("${configuration_step_text}" "PRODUCT_DEFINITION_EFFECTIVITY("
      "${schema} complex-effectivity policy")
  else()
    foreach(expected
        "PRODUCT_DEFINITION_EFFECTIVITY('EFF-A'"
        "PRODUCT_DEFINITION_EFFECTIVITY('EFF-B'")
      require_text("${configuration_step_text}" "${expected}"
        "${schema} product configuration entities")
    endforeach()
  endif()
  if(NOT schema STREQUAL "ap203")
    require_text("${configuration_step_text}"
      "SERIAL_NUMBERED_EFFECTIVITY('SERIAL-A','SN-100','SN-199')"
      "${schema} serial effectivity")
  else()
    reject_text("${configuration_step_text}" "SERIAL_NUMBERED_EFFECTIVITY("
      "${schema} serial effectivity policy")
  endif()
  if(schema STREQUAL "ap203e2" OR schema MATCHES "^ap242e[1-4]$")
    require_text("${configuration_step_text}"
      "APPLIED_EFFECTIVITY_ASSIGNMENT("
      "${schema} effectivity assignment")
  else()
    reject_text("${configuration_step_text}"
      "APPLIED_EFFECTIVITY_ASSIGNMENT("
      "${schema} effectivity-assignment policy")
  endif()
  if(schema STREQUAL "ap203" OR schema STREQUAL "ap214")
    reject_text("${configuration_step_text}" "EFFECTIVITY_RELATIONSHIP("
      "${schema} effectivity-relationship policy")
  else()
    require_text("${configuration_step_text}"
      "EFFECTIVITY_RELATIONSHIP('sequence','successive usages'"
      "${schema} effectivity relationship")
  endif()
  if(schema STREQUAL "ap203")
    require_text("${configuration_step_text}" "CC_DESIGN_APPROVAL("
      "${schema} approval assignment")
  else()
    require_text("${configuration_step_text}" "APPLIED_APPROVAL_ASSIGNMENT("
      "${schema} approval assignment")
  endif()
  reject_text("${configuration_step_text}" "APPROVAL(#700,'release A')"
    "${schema} source entity-number remapping")
  reject_text("${configuration_step_text}"
    "ALTERNATE_PRODUCT_RELATIONSHIP('','',#5000,#4000"
    "${schema} source product-number remapping")
  reject_text("${configuration_step_text}"
    "ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE('','',#2002,#2004)"
    "${schema} source usage-number remapping")
  reject_text("${configuration_step_text}" "(#4001,#4002)"
    "${schema} source definition-number remapping")
  reject_text("${configuration_step_text}"
    "APPROVAL_RELATIONSHIP('revision','release succession',#701,#703)"
    "${schema} source approval-number remapping")
  reject_text("${configuration_step_text}"
    "PRODUCT_DEFINITION_EFFECTIVITY('EFF-A',#2002)"
    "${schema} source effectivity-usage remapping")
  reject_text("${configuration_step_text}"
    "EFFECTIVITY_RELATIONSHIP('sequence','successive usages',#725,#726)"
    "${schema} source effectivity-number remapping")
  reject_text("${configuration_step_text}"
    "APPLIED_EFFECTIVITY_ASSIGNMENT(#728,(#4000,#4002))"
    "${schema} source effectivity-assignment number remapping")

  execute_process(
    COMMAND "${STEP_G}" --schema "${schema}" --strict
      -O "${configuration_roundtrip}"
      --report "${configuration_import_report}" "${configuration_step}"
    RESULT_VARIABLE configuration_import_result
    OUTPUT_VARIABLE configuration_import_output
    ERROR_VARIABLE configuration_import_error
  )
  if(NOT configuration_import_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} configuration reimport failed "
      "(${configuration_import_result}):\n"
      "${configuration_import_output}${configuration_import_error}")
  endif()
  file(READ "${configuration_import_report}"
    configuration_import_report_text)
  foreach(expected
      "\"type\":\"APPROVAL_STATUS\""
      "\"type\":\"APPROVAL\""
      "\"type\":\"ALTERNATE_PRODUCT_RELATIONSHIP\""
      "\"type\":\"ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE\""
      "\"type\":\"PRODUCT_RELATED_PRODUCT_CATEGORY\""
      "\"product_alternatives\":[{"
      "\"usage_substitutes\":[{"
      "\"geometry_written\":2")
    require_text("${configuration_import_report_text}" "${expected}"
      "${schema} configuration reimport report")
  endforeach()
  if(NOT schema STREQUAL "ap203")
    require_text("${configuration_import_report_text}"
      "\"type\":\"APPROVAL_RELATIONSHIP\""
      "${schema} approval-relationship reimport report")
  endif()
  if(NOT schema STREQUAL "ap203")
    require_text("${configuration_import_report_text}"
      "\"type\":\"PRODUCT_DEFINITION_EFFECTIVITY\""
      "${schema} product-definition-effectivity reimport report")
  endif()
  if(NOT schema STREQUAL "ap203")
    require_text("${configuration_import_report_text}"
      "\"type\":\"SERIAL_NUMBERED_EFFECTIVITY\""
      "${schema} serial-effectivity reimport report")
  endif()
  if(schema STREQUAL "ap203e2" OR schema MATCHES "^ap242e[1-4]$")
    require_text("${configuration_import_report_text}"
      "\"type\":\"APPLIED_EFFECTIVITY_ASSIGNMENT\""
      "${schema} effectivity-assignment reimport report")
  endif()
  if(NOT schema STREQUAL "ap203" AND NOT schema STREQUAL "ap214")
    require_text("${configuration_import_report_text}"
      "\"type\":\"EFFECTIVITY_RELATIONSHIP\""
      "${schema} effectivity-relationship reimport report")
  endif()
  if(schema STREQUAL "ap203")
    require_text("${configuration_import_report_text}"
      "\"type\":\"CC_DESIGN_APPROVAL\""
      "${schema} approval-assignment reimport report")
  else()
    require_text("${configuration_import_report_text}"
      "\"type\":\"APPLIED_APPROVAL_ASSIGNMENT\""
      "${schema} approval-assignment reimport report")
  endif()
endforeach()

# A retained AP203 dated-effectivity component orders start before optional
# end.  Modern schemas reverse those physical attributes.  Verify that export
# assigns by target attribute name, remaps the date/time chain, and does not
# replay the source argument positions or instance numbers.
set(dated_input "${OUTPUT_DIR}/g_step_dated_effectivity_input.g")
file(REMOVE "${dated_input}")
execute_process(
  COMMAND "${MGED}" -c "${dated_input}"
    "in dated.s sph 0 0 0 5; r dated.r u dated.s; attr set dated.r step:source_id 8100; attr set _GLOBAL STEP::AP203::CONFIGURATION::#800::TYPE COORDINATED_UNIVERSAL_TIME_OFFSET STEP::AP203::CONFIGURATION::#800::VALUE {COORDINATED_UNIVERSAL_TIME_OFFSET(5,30,.BEHIND.)} STEP::AP203::CONFIGURATION::#800::REFERENCES {} STEP::AP203::CONFIGURATION::#801::TYPE CALENDAR_DATE STEP::AP203::CONFIGURATION::#801::VALUE {CALENDAR_DATE(2028,29,2)} STEP::AP203::CONFIGURATION::#801::REFERENCES {} STEP::AP203::CONFIGURATION::#802::TYPE LOCAL_TIME STEP::AP203::CONFIGURATION::#802::VALUE {LOCAL_TIME(23,59,60.,#800)} STEP::AP203::CONFIGURATION::#802::REFERENCES 800 STEP::AP203::CONFIGURATION::#803::TYPE DATE_AND_TIME STEP::AP203::CONFIGURATION::#803::VALUE {DATE_AND_TIME(#801,#802)} STEP::AP203::CONFIGURATION::#803::REFERENCES {801 802} STEP::AP203::CONFIGURATION::#804::TYPE DATED_EFFECTIVITY STEP::AP203::CONFIGURATION::#804::VALUE {DATED_EFFECTIVITY('DATE-E1',#803,$)} STEP::AP203::CONFIGURATION::#804::REFERENCES 803"
  RESULT_VARIABLE dated_create_result
  OUTPUT_VARIABLE dated_create_output
  ERROR_VARIABLE dated_create_error
)
if(NOT dated_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create dated-effectivity fixture:\n"
    "${dated_create_output}${dated_create_error}")
endif()

foreach(schema ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  set(dated_step "${OUTPUT_DIR}/g_step_dated_effectivity_${schema}.stp")
  set(dated_report
    "${OUTPUT_DIR}/g_step_dated_effectivity_${schema}_export.json")
  set(dated_roundtrip
    "${OUTPUT_DIR}/g_step_dated_effectivity_${schema}.g")
  set(dated_import_report
    "${OUTPUT_DIR}/g_step_dated_effectivity_${schema}_import.json")
  file(REMOVE "${dated_step}" "${dated_report}" "${dated_roundtrip}"
    "${dated_import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${dated_report}" -o "${dated_step}" "${dated_input}" dated.r
    RESULT_VARIABLE dated_export_result
    OUTPUT_VARIABLE dated_export_output
    ERROR_VARIABLE dated_export_error
  )
  if(NOT dated_export_result EQUAL 0 OR NOT EXISTS "${dated_step}")
    message(FATAL_ERROR
      "${schema} dated-effectivity export failed "
      "(${dated_export_result}):\n"
      "${dated_export_output}${dated_export_error}")
  endif()
  file(READ "${dated_report}" dated_report_text)
  foreach(expected
      "\"configuration_records_seen\":5"
      "\"configuration_records_emitted\":5"
      "\"configuration_records_omitted\":0")
    require_text("${dated_report_text}" "${expected}"
      "${schema} dated-effectivity export report")
  endforeach()
  file(READ "${dated_step}" dated_step_text)
  foreach(expected
      "COORDINATED_UNIVERSAL_TIME_OFFSET(5,30,.BEHIND.)"
      "CALENDAR_DATE(2028,29,2)"
      "LOCAL_TIME(23,59,60."
      "DATE_AND_TIME("
      "DATED_EFFECTIVITY('DATE-E1',$,#")
    require_text("${dated_step_text}" "${expected}"
      "${schema} dated-effectivity graph")
  endforeach()
  reject_text("${dated_step_text}" "DATED_EFFECTIVITY('DATE-E1',#803,$)"
    "${schema} dated-effectivity source references")
  reject_text("${dated_step_text}" "DATED_EFFECTIVITY('DATE-E1',#"
    "${schema} dated-effectivity source positional order")
  execute_process(
    COMMAND "${STEP_G}" --schema "${schema}" --strict
      --report "${dated_import_report}" -O "${dated_roundtrip}"
      "${dated_step}"
    RESULT_VARIABLE dated_import_result
    OUTPUT_VARIABLE dated_import_output
    ERROR_VARIABLE dated_import_error
  )
  if(NOT dated_import_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} dated-effectivity reimport failed "
      "(${dated_import_result}):\n"
      "${dated_import_output}${dated_import_error}")
  endif()
  file(READ "${dated_import_report}" dated_import_report_text)
  foreach(expected
      "\"type\":\"CALENDAR_DATE\""
      "\"type\":\"DATE_AND_TIME\""
      "\"type\":\"DATED_EFFECTIVITY\""
      "\"geometry_written\":1")
    require_text("${dated_import_report_text}" "${expected}"
      "${schema} dated-effectivity reimport report")
  endforeach()
endforeach()

set(dated_ap203_step
  "${OUTPUT_DIR}/g_step_dated_effectivity_ap203.stp")
set(dated_ap203_report
  "${OUTPUT_DIR}/g_step_dated_effectivity_ap203_export.json")
set(dated_ap203_strict
  "${OUTPUT_DIR}/g_step_dated_effectivity_ap203_strict.stp")
file(REMOVE "${dated_ap203_step}" "${dated_ap203_report}"
  "${dated_ap203_strict}")
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${dated_ap203_report}"
    -o "${dated_ap203_step}" "${dated_input}" dated.r
  RESULT_VARIABLE dated_ap203_result
  OUTPUT_VARIABLE dated_ap203_output
  ERROR_VARIABLE dated_ap203_error
)
if(NOT dated_ap203_result EQUAL 1 OR NOT EXISTS "${dated_ap203_step}")
  message(FATAL_ERROR
    "AP203 dated-effectivity policy failed (${dated_ap203_result}):\n"
    "${dated_ap203_output}${dated_ap203_error}")
endif()
file(READ "${dated_ap203_report}" dated_ap203_report_text)
foreach(expected
    "\"configuration_records_emitted\":4"
    "\"configuration_records_omitted\":1"
    "AP203 edition 1 requires effectivity as a complex")
  require_text("${dated_ap203_report_text}" "${expected}"
    "AP203 dated-effectivity policy report")
endforeach()
file(READ "${dated_ap203_step}" dated_ap203_step_text)
reject_text("${dated_ap203_step_text}" "DATED_EFFECTIVITY("
  "AP203 incomplete complex effectivity")
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict -o "${dated_ap203_strict}"
    "${dated_input}" dated.r
  RESULT_VARIABLE dated_ap203_strict_result
  OUTPUT_VARIABLE dated_ap203_strict_output
  ERROR_VARIABLE dated_ap203_strict_error
)
if(NOT dated_ap203_strict_result EQUAL 4 OR EXISTS "${dated_ap203_strict}")
  message(FATAL_ERROR
    "strict AP203 dated effectivity was not transactional "
    "(${dated_ap203_strict_result}):\n"
    "${dated_ap203_strict_output}${dated_ap203_strict_error}")
endif()

# AP242 permits an end-only dated effectivity, while AP203e2 and AP214 require
# the start bound.  Preserve the AP242 semantics and make the lossy target
# constraint visible instead of silently moving the end date into start.
set(dated_end_input
  "${OUTPUT_DIR}/g_step_dated_effectivity_end_only_input.g")
set(dated_end_ap242_step
  "${OUTPUT_DIR}/g_step_dated_effectivity_end_only_ap242e4.stp")
file(REMOVE "${dated_end_input}" "${dated_end_ap242_step}")
execute_process(
  COMMAND "${MGED}" -c "${dated_end_input}"
    "in dated_end.s sph 0 0 0 5; r dated_end.r u dated_end.s; attr set dated_end.r step:source_id 8200; attr set _GLOBAL STEP::AP242e4::CONFIGURATION::#820::TYPE COORDINATED_UNIVERSAL_TIME_OFFSET STEP::AP242e4::CONFIGURATION::#820::VALUE {COORDINATED_UNIVERSAL_TIME_OFFSET(0,$,.EXACT.)} STEP::AP242e4::CONFIGURATION::#820::REFERENCES {} STEP::AP242e4::CONFIGURATION::#821::TYPE CALENDAR_DATE STEP::AP242e4::CONFIGURATION::#821::VALUE {CALENDAR_DATE(2030,1,1)} STEP::AP242e4::CONFIGURATION::#821::REFERENCES {} STEP::AP242e4::CONFIGURATION::#822::TYPE LOCAL_TIME STEP::AP242e4::CONFIGURATION::#822::VALUE {LOCAL_TIME(0,$,$,#820)} STEP::AP242e4::CONFIGURATION::#822::REFERENCES 820 STEP::AP242e4::CONFIGURATION::#823::TYPE DATE_AND_TIME STEP::AP242e4::CONFIGURATION::#823::VALUE {DATE_AND_TIME(#821,#822)} STEP::AP242e4::CONFIGURATION::#823::REFERENCES {821 822} STEP::AP242e4::CONFIGURATION::#824::TYPE DATED_EFFECTIVITY STEP::AP242e4::CONFIGURATION::#824::VALUE {DATED_EFFECTIVITY('DATE-END',#823,$)} STEP::AP242e4::CONFIGURATION::#824::REFERENCES 823"
  RESULT_VARIABLE dated_end_create_result
  OUTPUT_VARIABLE dated_end_create_output
  ERROR_VARIABLE dated_end_create_error
)
if(NOT dated_end_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create end-only dated-effectivity fixture:\n"
    "${dated_end_create_output}${dated_end_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --strict
    -o "${dated_end_ap242_step}" "${dated_end_input}" dated_end.r
  RESULT_VARIABLE dated_end_ap242_result
  OUTPUT_VARIABLE dated_end_ap242_output
  ERROR_VARIABLE dated_end_ap242_error
)
if(NOT dated_end_ap242_result EQUAL 0 OR
    NOT EXISTS "${dated_end_ap242_step}")
  message(FATAL_ERROR
    "AP242 end-only dated-effectivity export failed "
    "(${dated_end_ap242_result}):\n"
    "${dated_end_ap242_output}${dated_end_ap242_error}")
endif()
file(READ "${dated_end_ap242_step}" dated_end_ap242_text)
require_text("${dated_end_ap242_text}"
  "DATED_EFFECTIVITY('DATE-END',#" "AP242 end-only dated effectivity")
reject_text("${dated_end_ap242_text}"
  "DATED_EFFECTIVITY('DATE-END',$,#" "AP242 end-only positional order")

foreach(schema ap203e2 ap214)
  set(dated_end_step
    "${OUTPUT_DIR}/g_step_dated_effectivity_end_only_${schema}.stp")
  set(dated_end_report
    "${OUTPUT_DIR}/g_step_dated_effectivity_end_only_${schema}.json")
  set(dated_end_strict
    "${OUTPUT_DIR}/g_step_dated_effectivity_end_only_${schema}_strict.stp")
  file(REMOVE "${dated_end_step}" "${dated_end_report}"
    "${dated_end_strict}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --report "${dated_end_report}"
      -o "${dated_end_step}" "${dated_end_input}" dated_end.r
    RESULT_VARIABLE dated_end_result
    OUTPUT_VARIABLE dated_end_output
    ERROR_VARIABLE dated_end_error
  )
  if(NOT dated_end_result EQUAL 1 OR NOT EXISTS "${dated_end_step}")
    message(FATAL_ERROR
      "${schema} end-only dated-effectivity policy failed "
      "(${dated_end_result}):\n${dated_end_output}${dated_end_error}")
  endif()
  file(READ "${dated_end_report}" dated_end_report_text)
  foreach(expected
      "\"configuration_records_emitted\":4"
      "\"configuration_records_omitted\":1"
      "the target schema requires a dated-effectivity start bound")
    require_text("${dated_end_report_text}" "${expected}"
      "${schema} end-only dated-effectivity report")
  endforeach()
  file(READ "${dated_end_step}" dated_end_step_text)
  reject_text("${dated_end_step_text}" "DATED_EFFECTIVITY("
    "${schema} unsupported end-only dated effectivity")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      -o "${dated_end_strict}" "${dated_end_input}" dated_end.r
    RESULT_VARIABLE dated_end_strict_result
    OUTPUT_VARIABLE dated_end_strict_output
    ERROR_VARIABLE dated_end_strict_error
  )
  if(NOT dated_end_strict_result EQUAL 4 OR EXISTS "${dated_end_strict}")
    message(FATAL_ERROR
      "strict ${schema} end-only dated effectivity was not transactional "
      "(${dated_end_strict_result}):\n"
      "${dated_end_strict_output}${dated_end_strict_error}")
  endif()
endforeach()

# Invalid Gregorian dates are rejected before STEPcode receives them.  Their
# dependent DATE_AND_TIME and DATED_EFFECTIVITY records are then explicit
# omissions, and strict mode still publishes no partial file.
set(dated_bad_step "${OUTPUT_DIR}/g_step_dated_effectivity_bad.stp")
set(dated_bad_report "${OUTPUT_DIR}/g_step_dated_effectivity_bad.json")
set(dated_bad_strict
  "${OUTPUT_DIR}/g_step_dated_effectivity_bad_strict.stp")
file(REMOVE "${dated_bad_step}" "${dated_bad_report}" "${dated_bad_strict}")
execute_process(
  COMMAND "${MGED}" -c "${dated_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#801::VALUE {CALENDAR_DATE(2027,29,2)}"
  RESULT_VARIABLE dated_bad_create_result
  OUTPUT_VARIABLE dated_bad_create_output
  ERROR_VARIABLE dated_bad_create_error
)
if(NOT dated_bad_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create malformed dated-effectivity fixture:\n"
    "${dated_bad_create_output}${dated_bad_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --report "${dated_bad_report}"
    -o "${dated_bad_step}" "${dated_input}" dated.r
  RESULT_VARIABLE dated_bad_result
  OUTPUT_VARIABLE dated_bad_output
  ERROR_VARIABLE dated_bad_error
)
if(NOT dated_bad_result EQUAL 1 OR NOT EXISTS "${dated_bad_step}")
  message(FATAL_ERROR
    "malformed dated-effectivity policy failed (${dated_bad_result}):\n"
    "${dated_bad_output}${dated_bad_error}")
endif()
file(READ "${dated_bad_report}" dated_bad_report_text)
foreach(expected
    "\"configuration_records_emitted\":2"
    "\"configuration_records_omitted\":3"
    "CALENDAR_DATE is not a valid calendar day"
    "a retained DATE_AND_TIME dependency was not emitted"
    "a retained dated-effectivity bound was not emitted")
  require_text("${dated_bad_report_text}" "${expected}"
    "malformed dated-effectivity report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --strict -o "${dated_bad_strict}"
    "${dated_input}" dated.r
  RESULT_VARIABLE dated_bad_strict_result
  OUTPUT_VARIABLE dated_bad_strict_output
  ERROR_VARIABLE dated_bad_strict_error
)
if(NOT dated_bad_strict_result EQUAL 4 OR EXISTS "${dated_bad_strict}")
  message(FATAL_ERROR
    "strict malformed dated effectivity was not transactional "
    "(${dated_bad_strict_result}):\n"
    "${dated_bad_strict_output}${dated_bad_strict_error}")
endif()

# Lot effectivity owns a measure-with-unit dependency graph.  Exercise the
# source-reference closure, typed count measure, remapping, and assignment
# rules without retaining unrelated measure records from the rest of a file.
set(lot_input "${OUTPUT_DIR}/g_step_lot_effectivity_input.g")
file(REMOVE "${lot_input}")
execute_process(
  COMMAND "${MGED}" -c "${lot_input}"
    "in lot.s sph 0 0 0 5; r lot.r u lot.s; attr set lot.r step:source_id 9100; attr set _GLOBAL STEP::AP203e2::CONFIGURATION::#900::TYPE DIMENSIONAL_EXPONENTS STEP::AP203e2::CONFIGURATION::#900::VALUE {DIMENSIONAL_EXPONENTS(0.,0.,0.,0.,0.,0.,0.)} STEP::AP203e2::CONFIGURATION::#900::REFERENCES {} STEP::AP203e2::CONFIGURATION::#901::TYPE CONTEXT_DEPENDENT_UNIT STEP::AP203e2::CONFIGURATION::#901::VALUE {CONTEXT_DEPENDENT_UNIT(#900,'EA')} STEP::AP203e2::CONFIGURATION::#901::REFERENCES 900 STEP::AP203e2::CONFIGURATION::#902::TYPE MEASURE_WITH_UNIT STEP::AP203e2::CONFIGURATION::#902::VALUE {MEASURE_WITH_UNIT(COUNT_MEASURE(250.),#901)} STEP::AP203e2::CONFIGURATION::#902::REFERENCES 901 STEP::AP203e2::CONFIGURATION::#903::TYPE LOT_EFFECTIVITY STEP::AP203e2::CONFIGURATION::#903::VALUE {LOT_EFFECTIVITY('LOT-E1','BATCH-42',#902)} STEP::AP203e2::CONFIGURATION::#903::REFERENCES 902 STEP::AP203e2::CONFIGURATION::#904::TYPE APPLIED_EFFECTIVITY_ASSIGNMENT STEP::AP203e2::CONFIGURATION::#904::VALUE {APPLIED_EFFECTIVITY_ASSIGNMENT(#903,(#9100))} STEP::AP203e2::CONFIGURATION::#904::REFERENCES {903 9100}"
  RESULT_VARIABLE lot_create_result
  OUTPUT_VARIABLE lot_create_output
  ERROR_VARIABLE lot_create_error
)
if(NOT lot_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create lot-effectivity fixture:\n"
    "${lot_create_output}${lot_create_error}")
endif()

foreach(schema ap203e2 ap242e1 ap242e2 ap242e3 ap242e4)
  set(lot_step "${OUTPUT_DIR}/g_step_lot_effectivity_${schema}.stp")
  set(lot_report
    "${OUTPUT_DIR}/g_step_lot_effectivity_${schema}_export.json")
  set(lot_roundtrip "${OUTPUT_DIR}/g_step_lot_effectivity_${schema}.g")
  set(lot_import_report
    "${OUTPUT_DIR}/g_step_lot_effectivity_${schema}_import.json")
  file(REMOVE "${lot_step}" "${lot_report}" "${lot_roundtrip}"
    "${lot_import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict --report "${lot_report}"
      -o "${lot_step}" "${lot_input}" lot.r
    RESULT_VARIABLE lot_export_result
    OUTPUT_VARIABLE lot_export_output
    ERROR_VARIABLE lot_export_error
  )
  if(NOT lot_export_result EQUAL 0 OR NOT EXISTS "${lot_step}")
    message(FATAL_ERROR
      "${schema} lot-effectivity export failed (${lot_export_result}):\n"
      "${lot_export_output}${lot_export_error}")
  endif()
  file(READ "${lot_report}" lot_report_text)
  foreach(expected
      "\"configuration_records_seen\":5"
      "\"configuration_records_emitted\":5"
      "\"configuration_records_omitted\":0")
    require_text("${lot_report_text}" "${expected}"
      "${schema} lot-effectivity report")
  endforeach()
  file(READ "${lot_step}" lot_step_text)
  foreach(expected
      "DIMENSIONAL_EXPONENTS(0.,0.,0.,0.,0.,0.,0.)"
      "CONTEXT_DEPENDENT_UNIT("
      "'EA')"
      "MEASURE_WITH_UNIT(COUNT_MEASURE(250.)"
      "LOT_EFFECTIVITY('LOT-E1','BATCH-42',#"
      "APPLIED_EFFECTIVITY_ASSIGNMENT(")
    require_text("${lot_step_text}" "${expected}"
      "${schema} lot-effectivity graph")
  endforeach()
  foreach(forbidden
      "CONTEXT_DEPENDENT_UNIT(#900,'EA')"
      "MEASURE_WITH_UNIT(COUNT_MEASURE(250.),#901)"
      "LOT_EFFECTIVITY('LOT-E1','BATCH-42',#902)"
      "APPLIED_EFFECTIVITY_ASSIGNMENT(#903,(#9100))")
    reject_text("${lot_step_text}" "${forbidden}"
      "${schema} lot-effectivity source references")
  endforeach()
  execute_process(
    COMMAND "${STEP_G}" --schema "${schema}" --strict
      --report "${lot_import_report}" -O "${lot_roundtrip}" "${lot_step}"
    RESULT_VARIABLE lot_import_result
    OUTPUT_VARIABLE lot_import_output
    ERROR_VARIABLE lot_import_error
  )
  if(NOT lot_import_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} lot-effectivity reimport failed (${lot_import_result}):\n"
      "${lot_import_output}${lot_import_error}")
  endif()
  file(READ "${lot_import_report}" lot_import_report_text)
  foreach(expected
      "\"type\":\"DIMENSIONAL_EXPONENTS\""
      "\"type\":\"CONTEXT_DEPENDENT_UNIT\""
      "\"type\":\"MEASURE_WITH_UNIT\""
      "\"type\":\"LOT_EFFECTIVITY\""
      "\"type\":\"APPLIED_EFFECTIVITY_ASSIGNMENT\""
      "\"geometry_written\":1")
    require_text("${lot_import_report_text}" "${expected}"
      "${schema} lot-effectivity reimport report")
  endforeach()
  foreach(unrelated_type SI_UNIT UNCERTAINTY_MEASURE_WITH_UNIT
      PLANE_ANGLE_MEASURE_WITH_UNIT)
    reject_text("${lot_import_report_text}"
      "\"type\":\"${unrelated_type}\""
      "${schema} unrelated lot-effectivity measure retention")
  endforeach()
endforeach()

foreach(schema ap203 ap214)
  set(lot_limited_step
    "${OUTPUT_DIR}/g_step_lot_effectivity_${schema}.stp")
  set(lot_limited_report
    "${OUTPUT_DIR}/g_step_lot_effectivity_${schema}_export.json")
  set(lot_limited_strict
    "${OUTPUT_DIR}/g_step_lot_effectivity_${schema}_strict.stp")
  file(REMOVE "${lot_limited_step}" "${lot_limited_report}"
    "${lot_limited_strict}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}"
      --report "${lot_limited_report}" -o "${lot_limited_step}"
      "${lot_input}" lot.r
    RESULT_VARIABLE lot_limited_result
    OUTPUT_VARIABLE lot_limited_output
    ERROR_VARIABLE lot_limited_error
  )
  if(NOT lot_limited_result EQUAL 1 OR NOT EXISTS "${lot_limited_step}")
    message(FATAL_ERROR
      "${schema} lot-effectivity policy failed (${lot_limited_result}):\n"
      "${lot_limited_output}${lot_limited_error}")
  endif()
  file(READ "${lot_limited_report}" lot_limited_report_text)
  if(schema STREQUAL "ap203")
    foreach(expected
        "\"configuration_records_emitted\":3"
        "\"configuration_records_omitted\":2"
        "AP203 edition 1 requires effectivity as a complex"
        "AP203 edition 1 has no APPLIED_EFFECTIVITY_ASSIGNMENT entity")
      require_text("${lot_limited_report_text}" "${expected}"
        "AP203 lot-effectivity report")
    endforeach()
    file(READ "${lot_limited_step}" lot_limited_step_text)
    reject_text("${lot_limited_step_text}" "LOT_EFFECTIVITY("
      "AP203 incomplete complex lot effectivity")
  else()
    foreach(expected
        "\"configuration_records_emitted\":4"
        "\"configuration_records_omitted\":1"
        "AP214 forbids serial, lot, and product-definition effectivities in APPLIED_EFFECTIVITY_ASSIGNMENT")
      require_text("${lot_limited_report_text}" "${expected}"
        "AP214 lot-effectivity report")
    endforeach()
    file(READ "${lot_limited_step}" lot_limited_step_text)
    require_text("${lot_limited_step_text}"
      "LOT_EFFECTIVITY('LOT-E1','BATCH-42',#"
      "AP214 lot effectivity")
    reject_text("${lot_limited_step_text}" "APPLIED_EFFECTIVITY_ASSIGNMENT("
      "AP214 lot-effectivity assignment restriction")
  endif()
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      -o "${lot_limited_strict}" "${lot_input}" lot.r
    RESULT_VARIABLE lot_limited_strict_result
    OUTPUT_VARIABLE lot_limited_strict_output
    ERROR_VARIABLE lot_limited_strict_error
  )
  if(NOT lot_limited_strict_result EQUAL 4 OR
      EXISTS "${lot_limited_strict}")
    message(FATAL_ERROR
      "strict ${schema} lot effectivity was not transactional "
      "(${lot_limited_strict_result}):\n"
      "${lot_limited_strict_output}${lot_limited_strict_error}")
  endif()
endforeach()

# A malformed dimensional dependency prevents every downstream lot node from
# being authored and leaves strict output transactional.
set(lot_bad_step "${OUTPUT_DIR}/g_step_lot_effectivity_bad.stp")
set(lot_bad_report "${OUTPUT_DIR}/g_step_lot_effectivity_bad.json")
set(lot_bad_strict "${OUTPUT_DIR}/g_step_lot_effectivity_bad_strict.stp")
file(REMOVE "${lot_bad_step}" "${lot_bad_report}" "${lot_bad_strict}")
execute_process(
  COMMAND "${MGED}" -c "${lot_input}"
    "attr set _GLOBAL STEP::AP203e2::CONFIGURATION::#900::VALUE {DIMENSIONAL_EXPONENTS(nan,0.,0.,0.,0.,0.,0.)}"
  RESULT_VARIABLE lot_bad_create_result
  OUTPUT_VARIABLE lot_bad_create_output
  ERROR_VARIABLE lot_bad_create_error
)
if(NOT lot_bad_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create malformed lot-effectivity fixture:\n"
    "${lot_bad_create_output}${lot_bad_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --report "${lot_bad_report}"
    -o "${lot_bad_step}" "${lot_input}" lot.r
  RESULT_VARIABLE lot_bad_result
  OUTPUT_VARIABLE lot_bad_output
  ERROR_VARIABLE lot_bad_error
)
if(NOT lot_bad_result EQUAL 1 OR NOT EXISTS "${lot_bad_step}")
  message(FATAL_ERROR
    "malformed lot-effectivity policy failed (${lot_bad_result}):\n"
    "${lot_bad_output}${lot_bad_error}")
endif()
file(READ "${lot_bad_report}" lot_bad_report_text)
foreach(expected
    "\"configuration_records_emitted\":0"
    "\"configuration_records_omitted\":5"
    "DIMENSIONAL_EXPONENTS requires seven finite numbers"
    "the retained unit dimensions were not emitted"
    "the retained measure unit was not emitted"
    "the retained lot-size measure was not emitted"
    "the retained effectivity dependency was not emitted")
  require_text("${lot_bad_report_text}" "${expected}"
    "malformed lot-effectivity report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --strict -o "${lot_bad_strict}"
    "${lot_input}" lot.r
  RESULT_VARIABLE lot_bad_strict_result
  OUTPUT_VARIABLE lot_bad_strict_output
  ERROR_VARIABLE lot_bad_strict_error
)
if(NOT lot_bad_strict_result EQUAL 4 OR EXISTS "${lot_bad_strict}")
  message(FATAL_ERROR
    "strict malformed lot effectivity was not transactional "
    "(${lot_bad_strict_result}):\n"
    "${lot_bad_strict_output}${lot_bad_strict_error}")
endif()

# AP203e1 requires relationship.definition where the modern APs make it
# optional. Prove that the shared source graph does not silently emit an
# AP203-invalid '$', while AP203e2 accepts the same retained semantics.
execute_process(
  COMMAND "${MGED}" -c "${configuration_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#720::VALUE {ALTERNATE_PRODUCT_RELATIONSHIP('',$,#5000,#4000,'first available')}"
  RESULT_VARIABLE optional_definition_result
  OUTPUT_VARIABLE optional_definition_output
  ERROR_VARIABLE optional_definition_error
)
if(NOT optional_definition_result EQUAL 0)
  message(FATAL_ERROR
    "could not create edition-specific configuration fixture:\n"
    "${optional_definition_output}${optional_definition_error}")
endif()
set(optional_ap203_step
  "${OUTPUT_DIR}/g_step_configuration_optional_ap203.stp")
set(optional_ap203_report
  "${OUTPUT_DIR}/g_step_configuration_optional_ap203.json")
set(optional_ap203e2_step
  "${OUTPUT_DIR}/g_step_configuration_optional_ap203e2.stp")
file(REMOVE "${optional_ap203_step}" "${optional_ap203_report}"
  "${optional_ap203e2_step}")
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${optional_ap203_report}"
    -o "${optional_ap203_step}" "${configuration_input}" approved.r
  RESULT_VARIABLE optional_ap203_result
  OUTPUT_VARIABLE optional_ap203_output
  ERROR_VARIABLE optional_ap203_error
)
if(NOT optional_ap203_result EQUAL 1 OR NOT EXISTS "${optional_ap203_step}")
  message(FATAL_ERROR
    "AP203 optional-definition policy failed (${optional_ap203_result}):\n"
    "${optional_ap203_output}${optional_ap203_error}")
endif()
file(READ "${optional_ap203_report}" optional_ap203_report_text)
foreach(expected
    "\"configuration_records_emitted\":5"
    "\"configuration_records_omitted\":9"
    "AP203 edition 1 requires an alternate-product definition"
    "AP203 edition 1 requires effectivity as a complex"
    "AP203 edition 1 has no APPLIED_EFFECTIVITY_ASSIGNMENT entity"
    "an EFFECTIVITY_RELATIONSHIP dependency was not emitted")
  require_text("${optional_ap203_report_text}" "${expected}"
    "AP203 edition-specific configuration report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap203e2 --strict
    -o "${optional_ap203e2_step}"
    "${configuration_input}" approved.r
  RESULT_VARIABLE optional_ap203e2_result
  OUTPUT_VARIABLE optional_ap203e2_output
  ERROR_VARIABLE optional_ap203e2_error
)
if(NOT optional_ap203e2_result EQUAL 0 OR
   NOT EXISTS "${optional_ap203e2_step}")
  message(FATAL_ERROR
    "AP203e2 optional-definition export failed "
    "(${optional_ap203e2_result}):\n"
    "${optional_ap203e2_output}${optional_ap203e2_error}")
endif()

# An assignment item without a surviving emitted identity is an explicit
# permissive omission and a strict transactional failure.
execute_process(
  COMMAND "${MGED}" -c "${configuration_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#720::VALUE {ALTERNATE_PRODUCT_RELATIONSHIP('','',#5000,#4000,'first available')} STEP::AP203::CONFIGURATION::#723::VALUE {CC_DESIGN_APPROVAL(#701,(#9999))} STEP::AP203::CONFIGURATION::#723::REFERENCES {701 9999}"
  RESULT_VARIABLE missing_assignment_result
  OUTPUT_VARIABLE missing_assignment_output
  ERROR_VARIABLE missing_assignment_error
)
if(NOT missing_assignment_result EQUAL 0)
  message(FATAL_ERROR
    "could not create missing approval-assignment fixture:\n"
    "${missing_assignment_output}${missing_assignment_error}")
endif()
set(missing_assignment_step
  "${OUTPUT_DIR}/g_step_configuration_missing_assignment.stp")
set(missing_assignment_report
  "${OUTPUT_DIR}/g_step_configuration_missing_assignment.json")
set(missing_assignment_strict_step
  "${OUTPUT_DIR}/g_step_configuration_missing_assignment_strict.stp")
file(REMOVE "${missing_assignment_step}" "${missing_assignment_report}"
  "${missing_assignment_strict_step}")
execute_process(
  COMMAND "${G_STEP}" --schema ap242
    --report "${missing_assignment_report}" -o "${missing_assignment_step}"
    "${configuration_input}" approved.r
  RESULT_VARIABLE missing_assignment_export_result
  OUTPUT_VARIABLE missing_assignment_export_output
  ERROR_VARIABLE missing_assignment_export_error
)
if(NOT missing_assignment_export_result EQUAL 1 OR
   NOT EXISTS "${missing_assignment_step}")
  message(FATAL_ERROR
    "missing approval-assignment policy failed "
    "(${missing_assignment_export_result}):\n"
    "${missing_assignment_export_output}${missing_assignment_export_error}")
endif()
file(READ "${missing_assignment_report}" missing_assignment_report_text)
foreach(expected
    "\"configuration_records_emitted\":13"
    "\"configuration_records_omitted\":1"
    "an approval-assignment item has no unambiguous emitted entity")
  require_text("${missing_assignment_report_text}" "${expected}"
    "missing approval-assignment report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242 --strict
    -o "${missing_assignment_strict_step}" "${configuration_input}"
    approved.r
  RESULT_VARIABLE missing_assignment_strict_result
  OUTPUT_VARIABLE missing_assignment_strict_output
  ERROR_VARIABLE missing_assignment_strict_error
)
if(NOT missing_assignment_strict_result EQUAL 4 OR
   EXISTS "${missing_assignment_strict_step}")
  message(FATAL_ERROR
    "strict missing approval assignment was not transactional "
    "(${missing_assignment_strict_result}):\n"
    "${missing_assignment_strict_output}${missing_assignment_strict_error}")
endif()

# A malformed serial bound invalidates that effectivity and its dependent
# assignment.  Permissive export retains the independent configuration graph;
# strict export must remain transactional.
execute_process(
  COMMAND "${MGED}" -c "${configuration_input}"
    "attr set _GLOBAL STEP::AP203::CONFIGURATION::#723::VALUE {CC_DESIGN_APPROVAL(#701,(#4001,#4002))} STEP::AP203::CONFIGURATION::#723::REFERENCES {701 4001 4002} STEP::AP203e2::CONFIGURATION::#728::VALUE {SERIAL_NUMBERED_EFFECTIVITY('SERIAL-A','SN-100',17)}"
  RESULT_VARIABLE malformed_effectivity_create_result
  OUTPUT_VARIABLE malformed_effectivity_create_output
  ERROR_VARIABLE malformed_effectivity_create_error
)
if(NOT malformed_effectivity_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create malformed serial-effectivity fixture:\n"
    "${malformed_effectivity_create_output}"
    "${malformed_effectivity_create_error}")
endif()
set(malformed_effectivity_step
  "${OUTPUT_DIR}/g_step_configuration_malformed_effectivity.stp")
set(malformed_effectivity_report
  "${OUTPUT_DIR}/g_step_configuration_malformed_effectivity.json")
set(malformed_effectivity_strict_step
  "${OUTPUT_DIR}/g_step_configuration_malformed_effectivity_strict.stp")
file(REMOVE "${malformed_effectivity_step}" "${malformed_effectivity_report}"
  "${malformed_effectivity_strict_step}")
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4
    --report "${malformed_effectivity_report}"
    -o "${malformed_effectivity_step}" "${configuration_input}" approved.r
  RESULT_VARIABLE malformed_effectivity_result
  OUTPUT_VARIABLE malformed_effectivity_output
  ERROR_VARIABLE malformed_effectivity_error
)
if(NOT malformed_effectivity_result EQUAL 1 OR
   NOT EXISTS "${malformed_effectivity_step}")
  message(FATAL_ERROR
    "malformed serial-effectivity policy failed "
    "(${malformed_effectivity_result}):\n"
    "${malformed_effectivity_output}${malformed_effectivity_error}")
endif()
file(READ "${malformed_effectivity_report}"
  malformed_effectivity_report_text)
foreach(expected
    "\"configuration_records_emitted\":12"
    "\"configuration_records_omitted\":2"
    "SERIAL_NUMBERED_EFFECTIVITY has an invalid retained layout"
    "the retained effectivity dependency was not emitted")
  require_text("${malformed_effectivity_report_text}" "${expected}"
    "malformed serial-effectivity report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242e4 --strict
    -o "${malformed_effectivity_strict_step}" "${configuration_input}"
    approved.r
  RESULT_VARIABLE malformed_effectivity_strict_result
  OUTPUT_VARIABLE malformed_effectivity_strict_output
  ERROR_VARIABLE malformed_effectivity_strict_error
)
if(NOT malformed_effectivity_strict_result EQUAL 4 OR
   EXISTS "${malformed_effectivity_strict_step}")
  message(FATAL_ERROR
    "strict malformed serial effectivity was not transactional "
    "(${malformed_effectivity_strict_result}):\n"
    "${malformed_effectivity_strict_output}"
    "${malformed_effectivity_strict_error}")
endif()

# Malformed presentation attributes are accounted for, permit geometry in the
# default policy, and make strict export transactional.
set(malformed_input "${OUTPUT_DIR}/g_step_metadata_malformed.g")
set(malformed_step "${OUTPUT_DIR}/g_step_metadata_malformed.stp")
set(malformed_report "${OUTPUT_DIR}/g_step_metadata_malformed.json")
set(malformed_strict_step "${OUTPUT_DIR}/g_step_metadata_malformed_strict.stp")
file(REMOVE "${malformed_input}" "${malformed_step}" "${malformed_report}"
  "${malformed_strict_step}")
execute_process(
  COMMAND "${MGED}" -c "${malformed_input}"
    "in bad.s sph 0 0 0 5; r bad.r u bad.s; attr set bad.r step:color_rgb {2 0 0}"
  RESULT_VARIABLE malformed_create_result
  OUTPUT_VARIABLE malformed_create_output
  ERROR_VARIABLE malformed_create_error
)
if(NOT malformed_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create malformed metadata fixture:\n"
    "${malformed_create_output}${malformed_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap242 --report "${malformed_report}"
    -o "${malformed_step}" "${malformed_input}" bad.r
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_output
  ERROR_VARIABLE malformed_error
)
if(NOT malformed_result EQUAL 1 OR NOT EXISTS "${malformed_step}")
  message(FATAL_ERROR
    "permissive malformed metadata policy failed (${malformed_result}):\n"
    "${malformed_output}${malformed_error}")
endif()
file(READ "${malformed_report}" malformed_report_text)
foreach(expected
    "\"outcome\":\"partial\""
    "\"presentation_omitted\":1"
    "malformed presentation metadata")
  require_text("${malformed_report_text}" "${expected}"
    "malformed metadata report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242 --strict
    -o "${malformed_strict_step}" "${malformed_input}" bad.r
  RESULT_VARIABLE malformed_strict_result
  OUTPUT_VARIABLE malformed_strict_output
  ERROR_VARIABLE malformed_strict_error
)
if(NOT malformed_strict_result EQUAL 4 OR EXISTS "${malformed_strict_step}")
  message(FATAL_ERROR
    "strict malformed metadata policy failed (${malformed_strict_result}):\n"
    "${malformed_strict_output}${malformed_strict_error}")
endif()

# The shared modern-mechanical material path retains multiple material
# identities plus typed measure, descriptive, and Cartesian properties.
set(material_input "${OUTPUT_DIR}/g_step_material_input.g")
set(material_step "${OUTPUT_DIR}/g_step_material_ap214.stp")
set(material_export_report "${OUTPUT_DIR}/g_step_material_ap214_export.json")
set(material_roundtrip "${OUTPUT_DIR}/g_step_material_ap214.g")
set(material_import_report "${OUTPUT_DIR}/g_step_material_ap214_import.json")
file(REMOVE "${material_input}" "${material_step}" "${material_export_report}"
  "${material_roundtrip}" "${material_import_report}")
execute_process(
  COMMAND "${MGED}" -c "${material_input}"
    "in part.s rpp 0 10 0 20 0 30; r part.r u part.s; attr set part.r step:material_id AL-6061 step:material_name {Aluminum O'Brien} step:material_description {Test alloy} step:material:2:id STEEL-17-4 step:material:2:name {Stainless Steel} step:material:2:description {Second alloy} step:material:1:property:1:category {material property} step:material:1:property:1:name Mass step:material:1:property:1:description {Sample mass} step:material:1:property:1:value_type mass_measure step:material:1:property:1:values 2.7 step:material:1:property:1:units kg step:material:1:property:1:dimensions {0 1 0 0 0 0 0} step:material:1:property:2:category {material property} step:material:1:property:2:name Temper step:material:1:property:2:value_type descriptive step:material:1:property:2:text T6 step:material:1:property:3:category {material property} step:material:1:property:3:name {Grain direction} step:material:1:property:3:value_type cartesian_point step:material:1:property:3:values {1 0 0} step:material:1:property:4:category {material property} step:material:1:property:4:name Thickness step:material:1:property:4:value_type length_measure step:material:1:property:4:values 12.5 step:material:1:property:4:units mm step:material:1:property:4:dimensions {1 0 0 0 0 0 0} step:material:1:property:5:category {material property} step:material:1:property:5:name Gauge step:material:1:property:5:value_type length_measure step:material:1:property:5:values 0.5 step:material:1:property:5:units in step:material:1:property:5:dimensions {1 0 0 0 0 0 0} step:material:1:property:6:category {material property} step:material:1:property:6:name Density step:material:1:property:6:value_type numeric_measure step:material:1:property:6:values 2.7e-6 step:material:1:property:6:units {kg/mm^3} step:material:1:property:6:dimensions {-3 1 0 0 0 0 0} step:material:1:property:7:category {material property} step:material:1:property:7:name Angle step:material:1:property:7:value_type plane_angle_measure step:material:1:property:7:values 90 step:material:1:property:7:units deg step:material:1:property:7:dimensions {0 0 0 0 0 0 0} step:material:1:property:8:category {material property} step:material:1:property:8:name Area step:material:1:property:8:value_type area_measure step:material:1:property:8:values 4 step:material:1:property:8:units {m^2} step:material:1:property:8:dimensions {2 0 0 0 0 0 0} step:material:1:property:9:category {material property} step:material:1:property:9:name {Nested length} step:material:1:property:9:value_type length_measure step:material:1:property:9:values 3 step:material:1:property:9:units TWIP step:material:1:property:9:dimensions {1 0 0 0 0 0 0} step:material:1:property:9:unit:kind conversion step:material:1:property:9:unit:subtype length_unit step:material:1:property:9:unit:name TWIP step:material:1:property:9:unit:dimensions {1 0 0 0 0 0 0} step:material:1:property:9:unit:conversion_value_type length_measure step:material:1:property:9:unit:conversion_value 2.5 step:material:1:property:9:unit:component:1:kind conversion step:material:1:property:9:unit:component:1:subtype length_unit step:material:1:property:9:unit:component:1:name HALF_MM step:material:1:property:9:unit:component:1:dimensions {1 0 0 0 0 0 0} step:material:1:property:9:unit:component:1:conversion_value_type length_measure step:material:1:property:9:unit:component:1:conversion_value 0.5 step:material:1:property:9:unit:component:1:component:1:kind si step:material:1:property:9:unit:component:1:component:1:subtype length_unit step:material:1:property:9:unit:component:1:component:1:name metre step:material:1:property:9:unit:component:1:component:1:prefix milli step:material:1:property:9:unit:component:1:component:1:dimensions {1 0 0 0 0 0 0} step:material:1:property:10:category {material property} step:material:1:property:10:name {Custom rate} step:material:1:property:10:value_type numeric_measure step:material:1:property:10:values 7 step:material:1:property:10:units {TWIP^2*second^-1} step:material:1:property:10:dimensions {2 0 -1 0 0 0 0} step:material:1:property:10:unit:kind derived step:material:1:property:10:unit:dimensions {2 0 -1 0 0 0 0} step:material:1:property:10:unit:component:1:exponent 2 step:material:1:property:10:unit:component:1:kind conversion step:material:1:property:10:unit:component:1:subtype length_unit step:material:1:property:10:unit:component:1:name TWIP step:material:1:property:10:unit:component:1:dimensions {1 0 0 0 0 0 0} step:material:1:property:10:unit:component:1:conversion_value_type length_measure step:material:1:property:10:unit:component:1:conversion_value 2.5 step:material:1:property:10:unit:component:1:component:1:kind conversion step:material:1:property:10:unit:component:1:component:1:subtype length_unit step:material:1:property:10:unit:component:1:component:1:name HALF_MM step:material:1:property:10:unit:component:1:component:1:dimensions {1 0 0 0 0 0 0} step:material:1:property:10:unit:component:1:component:1:conversion_value_type length_measure step:material:1:property:10:unit:component:1:component:1:conversion_value 0.5 step:material:1:property:10:unit:component:1:component:1:component:1:kind si step:material:1:property:10:unit:component:1:component:1:component:1:subtype length_unit step:material:1:property:10:unit:component:1:component:1:component:1:name metre step:material:1:property:10:unit:component:1:component:1:component:1:prefix milli step:material:1:property:10:unit:component:1:component:1:component:1:dimensions {1 0 0 0 0 0 0} step:material:1:property:10:unit:component:2:exponent -1 step:material:1:property:10:unit:component:2:kind si step:material:1:property:10:unit:component:2:subtype time_unit step:material:1:property:10:unit:component:2:name second step:material:1:property:10:unit:component:2:dimensions {0 0 1 0 0 0 0}"
  RESULT_VARIABLE material_create_result
  OUTPUT_VARIABLE material_create_output
  ERROR_VARIABLE material_create_error
)
if(NOT material_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create material fixture:\n"
    "${material_create_output}${material_create_error}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${material_input}"
    "attr set part.r step:property:1:category {geometric validation property} step:property:1:name {volume check} step:property:1:description {independent product value} step:property:1:value_type volume_measure step:property:1:values 6000 step:property:1:units {mm^3} step:property:1:dimensions {3 0 0 0 0 0 0} step:property:2:category {BRL-CAD product property} step:property:2:name {source note} step:property:2:value_type descriptive step:property:2:text checked"
  RESULT_VARIABLE product_property_create_result
  OUTPUT_VARIABLE product_property_create_output
  ERROR_VARIABLE product_property_create_error
)
if(NOT product_property_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not add product properties to fixture:\n"
    "${product_property_create_output}${product_property_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --strict
    --report "${material_export_report}" -o "${material_step}"
    "${material_input}" part.r
  RESULT_VARIABLE material_export_result
  OUTPUT_VARIABLE material_export_output
  ERROR_VARIABLE material_export_error
)
if(NOT material_export_result EQUAL 0)
  message(FATAL_ERROR
    "AP214 material export failed (${material_export_result}):\n"
    "${material_export_output}${material_export_error}")
endif()
file(READ "${material_step}" material_step_text)
foreach(expected
    "PRODUCT('AL-6061','Aluminum O''Brien','Test alloy'"
    "PRODUCT('STEEL-17-4','Stainless Steel','Second alloy'"
    "MAKE_FROM_USAGE_OPTION('material assignment','make from'"
    "MAKE_FROM_USAGE_OPTION('material assignment 2','make from'"
    "COUNT_MEASURE(1.)"
    "SI_UNIT(.KILO.,.GRAM.)"
    "MEASURE_REPRESENTATION_ITEM('Mass',MASS_MEASURE(2.7)"
    "DESCRIPTIVE_REPRESENTATION_ITEM('Temper','T6')"
    "CARTESIAN_POINT('Grain direction',(1.,0.,0.))"
    "MEASURE_REPRESENTATION_ITEM('Thickness',LENGTH_MEASURE(12.5)"
    "SI_UNIT(.MILLI.,.METRE.)"
    "CONVERSION_BASED_UNIT('INCH'"
    "MEASURE_WITH_UNIT(LENGTH_MEASURE("
    "MEASURE_REPRESENTATION_ITEM('Density',NUMERIC_MEASURE("
    "DERIVED_UNIT("
    "DERIVED_UNIT_ELEMENT("
    "MEASURE_REPRESENTATION_ITEM('Angle',PLANE_ANGLE_MEASURE(90.)"
    "CONVERSION_BASED_UNIT('DEGREE'"
    "MEASURE_REPRESENTATION_ITEM('Area',AREA_MEASURE(4.)"
    "MEASURE_REPRESENTATION_ITEM('Nested length',LENGTH_MEASURE(3.)"
    "CONVERSION_BASED_UNIT('HALF_MM'"
    "CONVERSION_BASED_UNIT('TWIP'"
    "MEASURE_WITH_UNIT(LENGTH_MEASURE(0.5)"
    "MEASURE_WITH_UNIT(LENGTH_MEASURE(2.5)"
    "MEASURE_REPRESENTATION_ITEM('Custom rate',NUMERIC_MEASURE(7.)"
    "MEASURE_REPRESENTATION_ITEM('volume check',VOLUME_MEASURE(6000.)"
    "DESCRIPTIVE_REPRESENTATION_ITEM('source note','checked')")
  require_text("${material_step_text}" "${expected}" "AP214 material graph")
endforeach()
file(READ "${material_export_report}" material_export_report_text)
foreach(expected "\"materials_emitted\":2" "\"materials_omitted\":0"
    "\"material_properties_emitted\":10"
    "\"material_properties_omitted\":0"
    "\"product_properties_emitted\":2"
    "\"product_properties_omitted\":0")
  require_text("${material_export_report_text}" "${expected}"
    "AP214 material export report")
endforeach()
execute_process(
  COMMAND "${STEP_G}" --strict -O "${material_roundtrip}"
    --report "${material_import_report}" "${material_step}"
  RESULT_VARIABLE material_import_result
  OUTPUT_VARIABLE material_import_output
  ERROR_VARIABLE material_import_error
)
if(NOT material_import_result EQUAL 0)
  message(FATAL_ERROR
    "AP214 material reimport failed (${material_import_result}):\n"
    "${material_import_output}${material_import_error}")
endif()
file(READ "${material_import_report}" material_import_report_text)
require_text("${material_import_report_text}" "\"materials_extracted\":2"
  "AP214 material import report")
require_text("${material_import_report_text}" "\"properties_extracted\":12"
  "AP214 material property import report")
execute_process(
  COMMAND "${MGED}" -c "${material_roundtrip}" "attr show part_r"
  RESULT_VARIABLE material_inspect_result
  OUTPUT_VARIABLE material_inspect_output
  ERROR_VARIABLE material_inspect_error
)
string(APPEND material_inspect_output "${material_inspect_error}")
if(NOT material_inspect_result EQUAL 0)
  message(FATAL_ERROR
    "could not inspect AP214 material round trip:\n${material_inspect_output}")
endif()
foreach(expected AL-6061 "Aluminum O'Brien" "Test alloy" STEEL-17-4
    "Stainless Steel" "Second alloy" "material property" Mass "Sample mass"
    mass_measure kilogram "0 1 0 0 0 0 0" Temper T6 descriptive
    "Grain direction" cartesian_point "1 0 0" Thickness length_measure
    millimetre "1 0 0 0 0 0 0" Gauge INCH Density numeric_measure
    "kilogram^1*millimetre^-3" "-3 1 0 0 0 0 0" Angle
    plane_angle_measure DEGREE Area area_measure "metre^2" "2 0 0 0 0 0 0"
    "Nested length" TWIP HALF_MM "Custom rate" "TWIP^2*second^-1"
    "step:material:1:property:9:unit:conversion_value" "2.5"
    "step:material:1:property:9:unit:component:1:conversion_value" "0.5"
    "step:material:1:property:10:unit:component:1:exponent" "second"
    "step:property:1:category" "geometric validation property"
    "step:property:1:name" "volume check" "step:property:1:value_type"
    volume_measure "step:property:1:values" 6000 "step:property:1:units"
    "millimetre^3" "step:property:2:name" "source note"
    "step:property:2:value_type" descriptive "step:property:2:text" checked)
  require_text("${material_inspect_output}" "${expected}"
    "AP214 material attributes")
endforeach()

foreach(schema ap203e2 ap242e1 ap242e2 ap242e3 ap242e4)
  set(shared_material_step "${OUTPUT_DIR}/g_step_material_${schema}.stp")
  set(shared_material_export_report
    "${OUTPUT_DIR}/g_step_material_${schema}_export.json")
  set(shared_material_roundtrip "${OUTPUT_DIR}/g_step_material_${schema}.g")
  set(shared_material_import_report
    "${OUTPUT_DIR}/g_step_material_${schema}_import.json")
  file(REMOVE "${shared_material_step}" "${shared_material_export_report}"
    "${shared_material_roundtrip}" "${shared_material_import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${shared_material_export_report}" -o "${shared_material_step}"
      "${material_input}" part.r
    RESULT_VARIABLE shared_material_export_result
    OUTPUT_VARIABLE shared_material_export_output
    ERROR_VARIABLE shared_material_export_error
  )
  if(NOT shared_material_export_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} shared material export failed "
      "(${shared_material_export_result}):\n"
      "${shared_material_export_output}${shared_material_export_error}")
  endif()
  file(READ "${shared_material_step}" shared_material_step_text)
  foreach(expected
      "PRODUCT('AL-6061','Aluminum O''Brien','Test alloy'"
      "PRODUCT('STEEL-17-4','Stainless Steel','Second alloy'"
      "MAKE_FROM_USAGE_OPTION('material assignment','make from'"
      "MAKE_FROM_USAGE_OPTION('material assignment 2','make from'"
      "MEASURE_REPRESENTATION_ITEM('Mass',MASS_MEASURE(2.7)"
      "DESCRIPTIVE_REPRESENTATION_ITEM('Temper','T6')"
      "CARTESIAN_POINT('Grain direction',(1.,0.,0.))"
      "MEASURE_REPRESENTATION_ITEM('Thickness',LENGTH_MEASURE(12.5)"
      "SI_UNIT(.MILLI.,.METRE.)"
      "CONVERSION_BASED_UNIT('INCH'"
      "DERIVED_UNIT("
      "MEASURE_REPRESENTATION_ITEM('Angle',PLANE_ANGLE_MEASURE(90.)"
      "CONVERSION_BASED_UNIT('DEGREE'"
      "MEASURE_REPRESENTATION_ITEM('Area',AREA_MEASURE(4.)"
      "MEASURE_REPRESENTATION_ITEM('Nested length',LENGTH_MEASURE(3.)"
      "CONVERSION_BASED_UNIT('HALF_MM'"
      "CONVERSION_BASED_UNIT('TWIP'"
      "MEASURE_REPRESENTATION_ITEM('Custom rate',NUMERIC_MEASURE(7.)"
      "MEASURE_REPRESENTATION_ITEM('volume check',VOLUME_MEASURE(6000.)"
      "DESCRIPTIVE_REPRESENTATION_ITEM('source note','checked')")
    require_text("${shared_material_step_text}" "${expected}"
      "${schema} shared material graph")
  endforeach()
  file(READ "${shared_material_export_report}"
    shared_material_export_report_text)
  require_text("${shared_material_export_report_text}"
    "\"materials_emitted\":2" "${schema} material export report")
  require_text("${shared_material_export_report_text}"
    "\"material_properties_emitted\":10"
    "${schema} material property export report")
  require_text("${shared_material_export_report_text}"
    "\"product_properties_emitted\":2"
    "${schema} product property export report")
  execute_process(
    COMMAND "${STEP_G}" --schema "${schema}" --strict
      -O "${shared_material_roundtrip}"
      --report "${shared_material_import_report}" "${shared_material_step}"
    RESULT_VARIABLE shared_material_import_result
    OUTPUT_VARIABLE shared_material_import_output
    ERROR_VARIABLE shared_material_import_error
  )
  if(NOT shared_material_import_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} shared material reimport failed "
      "(${shared_material_import_result}):\n"
      "${shared_material_import_output}${shared_material_import_error}")
  endif()
  file(READ "${shared_material_import_report}"
    shared_material_import_report_text)
  require_text("${shared_material_import_report_text}"
    "\"materials_extracted\":2" "${schema} material import report")
  require_text("${shared_material_import_report_text}"
    "\"properties_extracted\":12" "${schema} property import report")
  execute_process(
    COMMAND "${MGED}" -c "${shared_material_roundtrip}" "attr show part_r"
    RESULT_VARIABLE shared_material_inspect_result
    OUTPUT_VARIABLE shared_material_inspect_output
    ERROR_VARIABLE shared_material_inspect_error
  )
  string(APPEND shared_material_inspect_output
    "${shared_material_inspect_error}")
  if(NOT shared_material_inspect_result EQUAL 0)
    message(FATAL_ERROR
      "could not inspect ${schema} material round trip:\n"
      "${shared_material_inspect_output}")
  endif()
  foreach(expected AL-6061 "Aluminum O'Brien" "Test alloy" STEEL-17-4
      "Stainless Steel" "Second alloy" "material property" Mass
      "Sample mass" mass_measure kilogram "0 1 0 0 0 0 0" Temper T6 descriptive
      "Grain direction" cartesian_point "1 0 0" Thickness length_measure
      millimetre Gauge INCH Density numeric_measure
      "kilogram^1*millimetre^-3" "-3 1 0 0 0 0 0" Angle
      plane_angle_measure DEGREE Area area_measure "metre^2"
      "2 0 0 0 0 0 0" "Nested length" TWIP HALF_MM "Custom rate"
      "TWIP^2*second^-1"
      "step:material:1:property:9:unit:conversion_value" "2.5"
      "step:material:1:property:10:unit:component:2:exponent" "second"
      "step:property:1:category" "geometric validation property"
      "step:property:1:name" "volume check" "step:property:1:value_type"
      volume_measure "step:property:1:values" 6000 "step:property:1:units"
      "millimetre^3" "step:property:2:name" "source note"
      "step:property:2:value_type" descriptive "step:property:2:text" checked)
    require_text("${shared_material_inspect_output}" "${expected}"
      "${schema} material attributes")
  endforeach()
endforeach()

# Malformed structured material properties are omitted explicitly in the
# permissive policy and make strict publication transactional.  The valid
# material identity and geometry remain usable in permissive output.
set(material_bad_input "${OUTPUT_DIR}/g_step_material_bad_input.g")
set(material_bad_step "${OUTPUT_DIR}/g_step_material_bad.stp")
set(material_bad_report "${OUTPUT_DIR}/g_step_material_bad.json")
set(material_bad_strict "${OUTPUT_DIR}/g_step_material_bad_strict.stp")
file(REMOVE "${material_bad_input}" "${material_bad_step}"
  "${material_bad_report}" "${material_bad_strict}")
execute_process(
  COMMAND "${MGED}" -c "${material_bad_input}"
    "in badmat.s sph 0 0 0 5; r badmat.r u badmat.s; attr set badmat.r step:material_id MAT-BAD step:material_name {Malformed material} step:material:1:property:1:category {material property} step:material:1:property:1:name Length step:material:1:property:1:value_type length_measure step:material:1:property:1:values nope step:material:1:property:1:units mm step:material:1:property:2:category {material property} step:material:1:property:2:name {Wrong unit} step:material:1:property:2:value_type length_measure step:material:1:property:2:values 1 step:material:1:property:2:units kg step:material:1:property:2:dimensions {1 0 0 0 0 0 0} step:material:1:property:3:category {material property} step:material:1:property:3:name {Broken conversion} step:material:1:property:3:value_type length_measure step:material:1:property:3:values 1 step:material:1:property:3:units BROKEN step:material:1:property:3:dimensions {1 0 0 0 0 0 0} step:material:1:property:3:unit:kind conversion step:material:1:property:3:unit:subtype length_unit step:material:1:property:3:unit:name BROKEN step:material:1:property:3:unit:dimensions {1 0 0 0 0 0 0} step:material:1:property:3:unit:conversion_value_type length_measure step:material:1:property:3:unit:conversion_value 1"
  RESULT_VARIABLE material_bad_create_result
  OUTPUT_VARIABLE material_bad_create_output
  ERROR_VARIABLE material_bad_create_error
)
if(NOT material_bad_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create malformed material fixture:\n"
    "${material_bad_create_output}${material_bad_create_error}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${material_bad_input}"
    "attr set badmat.r step:property:1:category {geometric validation property} step:property:1:name {bad volume} step:property:1:value_type volume_measure step:property:1:values nope step:property:1:units {mm^3} step:property:1:dimensions {3 0 0 0 0 0 0}"
  RESULT_VARIABLE product_property_bad_create_result
  OUTPUT_VARIABLE product_property_bad_create_output
  ERROR_VARIABLE product_property_bad_create_error
)
if(NOT product_property_bad_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not add malformed product property:\n"
    "${product_property_bad_create_output}${product_property_bad_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --report "${material_bad_report}"
    -o "${material_bad_step}" "${material_bad_input}" badmat.r
  RESULT_VARIABLE material_bad_result
  OUTPUT_VARIABLE material_bad_output
  ERROR_VARIABLE material_bad_error
)
if(NOT material_bad_result EQUAL 1 OR NOT EXISTS "${material_bad_step}")
  message(FATAL_ERROR
    "permissive malformed material-property policy failed "
    "(${material_bad_result}):\n${material_bad_output}${material_bad_error}")
endif()
file(READ "${material_bad_report}" material_bad_report_text)
foreach(expected "\"outcome\":\"partial\"" "\"materials_emitted\":1"
    "\"material_properties_omitted\":3"
    "\"product_properties_omitted\":1"
    "requires exactly one finite value"
    "standardized property unit kind does not match value_type"
    "conversion unit needs a name, typed value, and one factor unit")
  require_text("${material_bad_report_text}" "${expected}"
    "malformed material-property report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --strict -o "${material_bad_strict}"
    "${material_bad_input}" badmat.r
  RESULT_VARIABLE material_bad_strict_result
  OUTPUT_VARIABLE material_bad_strict_output
  ERROR_VARIABLE material_bad_strict_error
)
if(NOT material_bad_strict_result EQUAL 4 OR EXISTS "${material_bad_strict}")
  message(FATAL_ERROR
    "strict malformed material-property policy failed "
    "(${material_bad_strict_result}):\n"
    "${material_bad_strict_output}${material_bad_strict_error}")
endif()

set(material_unsupported_step "${OUTPUT_DIR}/g_step_material_ap203.stp")
set(material_unsupported_report "${OUTPUT_DIR}/g_step_material_ap203.json")
set(material_unsupported_strict "${OUTPUT_DIR}/g_step_material_ap203_strict.stp")
file(REMOVE "${material_unsupported_step}" "${material_unsupported_report}"
  "${material_unsupported_strict}")
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --report "${material_unsupported_report}"
    -o "${material_unsupported_step}" "${material_input}" part.r
  RESULT_VARIABLE material_unsupported_result
  OUTPUT_VARIABLE material_unsupported_output
  ERROR_VARIABLE material_unsupported_error
)
if(NOT material_unsupported_result EQUAL 1 OR
   NOT EXISTS "${material_unsupported_step}")
  message(FATAL_ERROR
    "permissive unsupported-material policy failed "
    "(${material_unsupported_result}):\n"
    "${material_unsupported_output}${material_unsupported_error}")
endif()
file(READ "${material_unsupported_report}" material_unsupported_report_text)
foreach(expected "\"outcome\":\"partial\"" "\"materials_omitted\":2"
    "\"product_properties_omitted\":2"
    "no enabled material mapping" "no enabled product-property mapping")
  require_text("${material_unsupported_report_text}" "${expected}"
    "unsupported material report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap203 --strict
    -o "${material_unsupported_strict}" "${material_input}" part.r
  RESULT_VARIABLE material_unsupported_strict_result
  OUTPUT_VARIABLE material_unsupported_strict_output
  ERROR_VARIABLE material_unsupported_strict_error
)
if(NOT material_unsupported_strict_result EQUAL 4 OR
   EXISTS "${material_unsupported_strict}")
  message(FATAL_ERROR
    "strict unsupported-material policy failed "
    "(${material_unsupported_strict_result}):\n"
    "${material_unsupported_strict_output}${material_unsupported_strict_error}")
endif()

# Occurrence identity is retained on the parent combination with a guarded
# child-name key.  The guard prevents stale attributes from migrating to a
# different member after the BRL-CAD tree is edited.
set(occurrence_input "${OUTPUT_DIR}/g_step_occurrence_input.g")
file(REMOVE "${occurrence_input}")
execute_process(
  COMMAND "${MGED}" -c "${occurrence_input}"
    "in left.s rpp 0 10 0 10 0 10; r left.r u left.s; in right.s rpp 20 30 0 10 0 10; r right.r u right.s; comb asm.c u left.r u right.r; attr set asm.c step:occurrence:1:child left.r step:occurrence:1:id OCC-A step:occurrence:1:name {Left use} step:occurrence:1:description {Left description} step:occurrence:1:reference_designator L-1 step:occurrence:2:child right.r step:occurrence:2:id OCC-B step:occurrence:2:name {Right use} step:occurrence:2:description {Right description} step:occurrence:2:reference_designator R-2"
  RESULT_VARIABLE occurrence_create_result
  OUTPUT_VARIABLE occurrence_create_output
  ERROR_VARIABLE occurrence_create_error
)
if(NOT occurrence_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create occurrence fixture:\n"
    "${occurrence_create_output}${occurrence_create_error}")
endif()
foreach(schema ap203 ap203e2 ap214 ap242)
  set(occurrence_step "${OUTPUT_DIR}/g_step_occurrence_${schema}.stp")
  set(occurrence_export_report
    "${OUTPUT_DIR}/g_step_occurrence_${schema}_export.json")
  set(occurrence_roundtrip "${OUTPUT_DIR}/g_step_occurrence_${schema}.g")
  file(REMOVE "${occurrence_step}" "${occurrence_export_report}"
    "${occurrence_roundtrip}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${occurrence_export_report}" -o "${occurrence_step}"
      "${occurrence_input}" asm.c
    RESULT_VARIABLE occurrence_export_result
    OUTPUT_VARIABLE occurrence_export_output
    ERROR_VARIABLE occurrence_export_error
  )
  if(NOT occurrence_export_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} occurrence export failed (${occurrence_export_result}):\n"
      "${occurrence_export_output}${occurrence_export_error}")
  endif()
  file(READ "${occurrence_step}" occurrence_step_text)
  foreach(expected
      "NEXT_ASSEMBLY_USAGE_OCCURRENCE('OCC-A','Left use','Left description'"
      "'L-1')"
      "NEXT_ASSEMBLY_USAGE_OCCURRENCE('OCC-B','Right use','Right description'"
      "'R-2')")
    require_text("${occurrence_step_text}" "${expected}"
      "${schema} occurrence graph")
  endforeach()
  file(READ "${occurrence_export_report}" occurrence_export_report_text)
  foreach(expected "\"occurrences_updated\":2" "\"occurrences_omitted\":0")
    require_text("${occurrence_export_report_text}" "${expected}"
      "${schema} occurrence report")
  endforeach()
  execute_process(
    COMMAND "${STEP_G}" --strict -O "${occurrence_roundtrip}"
      "${occurrence_step}"
    RESULT_VARIABLE occurrence_import_result
    OUTPUT_VARIABLE occurrence_import_output
    ERROR_VARIABLE occurrence_import_error
  )
  if(NOT occurrence_import_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} occurrence reimport failed (${occurrence_import_result}):\n"
      "${occurrence_import_output}${occurrence_import_error}")
  endif()
  execute_process(
    COMMAND "${MGED}" -c "${occurrence_roundtrip}" "attr show asm_c"
    RESULT_VARIABLE occurrence_inspect_result
    OUTPUT_VARIABLE occurrence_inspect_output
    ERROR_VARIABLE occurrence_inspect_error
  )
  string(APPEND occurrence_inspect_output "${occurrence_inspect_error}")
  if(NOT occurrence_inspect_result EQUAL 0)
    message(FATAL_ERROR
      "could not inspect ${schema} occurrence round trip:\n"
      "${occurrence_inspect_output}")
  endif()
  foreach(expected OCC-A "Left use" "Left description" L-1
      OCC-B "Right use" "Right description" R-2)
    require_text("${occurrence_inspect_output}" "${expected}"
      "${schema} occurrence attributes")
  endforeach()
endforeach()

set(stale_occurrence_input "${OUTPUT_DIR}/g_step_occurrence_stale.g")
set(stale_occurrence_step "${OUTPUT_DIR}/g_step_occurrence_stale.stp")
set(stale_occurrence_report "${OUTPUT_DIR}/g_step_occurrence_stale.json")
set(stale_occurrence_strict "${OUTPUT_DIR}/g_step_occurrence_stale_strict.stp")
file(REMOVE "${stale_occurrence_input}" "${stale_occurrence_step}"
  "${stale_occurrence_report}" "${stale_occurrence_strict}")
execute_process(
  COMMAND "${MGED}" -c "${stale_occurrence_input}"
    "in leaf.s sph 0 0 0 5; r leaf.r u leaf.s; comb parent.c u leaf.r; attr set parent.c step:occurrence:1:child replaced.r step:occurrence:1:id STALE"
  RESULT_VARIABLE stale_create_result
  OUTPUT_VARIABLE stale_create_output
  ERROR_VARIABLE stale_create_error
)
if(NOT stale_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create stale occurrence fixture:\n"
    "${stale_create_output}${stale_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap242 --report "${stale_occurrence_report}"
    -o "${stale_occurrence_step}" "${stale_occurrence_input}" parent.c
  RESULT_VARIABLE stale_occurrence_result
  OUTPUT_VARIABLE stale_occurrence_output
  ERROR_VARIABLE stale_occurrence_error
)
if(NOT stale_occurrence_result EQUAL 1 OR NOT EXISTS "${stale_occurrence_step}")
  message(FATAL_ERROR
    "permissive stale-occurrence policy failed (${stale_occurrence_result}):\n"
    "${stale_occurrence_output}${stale_occurrence_error}")
endif()
file(READ "${stale_occurrence_report}" stale_occurrence_report_text)
foreach(expected "\"occurrences_omitted\":1" "stored child" "no longer matches")
  require_text("${stale_occurrence_report_text}" "${expected}"
    "stale occurrence report")
endforeach()
execute_process(
  COMMAND "${G_STEP}" --schema ap242 --strict -o "${stale_occurrence_strict}"
    "${stale_occurrence_input}" parent.c
  RESULT_VARIABLE stale_occurrence_strict_result
  OUTPUT_VARIABLE stale_occurrence_strict_output
  ERROR_VARIABLE stale_occurrence_strict_error
)
if(NOT stale_occurrence_strict_result EQUAL 4 OR
   EXISTS "${stale_occurrence_strict}")
  message(FATAL_ERROR
    "strict stale-occurrence policy failed "
    "(${stale_occurrence_strict_result}):\n"
    "${stale_occurrence_strict_output}${stale_occurrence_strict_error}")
endif()
