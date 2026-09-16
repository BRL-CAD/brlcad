if(NOT DEFINED STEP_G OR NOT DEFINED G_STEP OR NOT DEFINED MGED OR
   NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED REPORT OR
   NOT DEFINED EXPORT_OUTPUT OR NOT DEFINED EXPORT_REPORT)
  message(FATAL_ERROR
    "STEP_G, G_STEP, MGED, INPUT, OUTPUT, REPORT, EXPORT_OUTPUT, and EXPORT_REPORT are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

file(REMOVE "${OUTPUT}" "${REPORT}" "${EXPORT_OUTPUT}" "${EXPORT_REPORT}")
execute_process(
  COMMAND "${STEP_G}" -f --schema ap214 --strict --report "${REPORT}"
    "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "hand-authored AP214 unit import failed (${import_result}):\n${import_output}${import_error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"materials_extracted\":1"
    "\"properties_extracted\":2"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0,\"outcome\":\"complete\"")
  require_text("${report_text}" "${expected}" "AP214 structured-unit report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "attr show Unit_Product"
  RESULT_VARIABLE inspect_result
  OUTPUT_VARIABLE inspect_output
  ERROR_VARIABLE inspect_error
)
string(APPEND inspect_output "${inspect_error}")
if(NOT inspect_result EQUAL 0)
  message(FATAL_ERROR "could not inspect retained unit graph:\n${inspect_output}")
endif()
foreach(expected
    "step:material:1:property:1:dimensions                                          1 0 0 0 0 0 0"
    "step:material:1:property:1:unit:conversion_value                               2.5"
    "step:material:1:property:1:unit:component:1:conversion_value                   0.5"
    "step:material:1:property:2:dimensions                                          2 0 -1 0 0 0 0"
    "step:material:1:property:2:unit:component:1:exponent                           2"
    "step:material:1:property:2:unit:component:2:exponent                           -1")
  require_text("${inspect_output}" "${expected}" "retained AP214 unit graph")
endforeach()

execute_process(
  COMMAND "${G_STEP}" --schema ap214 --native-csg --strict
    --report "${EXPORT_REPORT}" -o "${EXPORT_OUTPUT}" "${OUTPUT}" Unit_Product
  RESULT_VARIABLE export_result
  OUTPUT_VARIABLE export_stdout
  ERROR_VARIABLE export_stderr
)
if(NOT export_result EQUAL 0 OR NOT EXISTS "${EXPORT_OUTPUT}")
  message(FATAL_ERROR
    "retained AP214 unit export failed (${export_result}):\n${export_stdout}${export_stderr}")
endif()
file(READ "${EXPORT_OUTPUT}" step_text)
foreach(expected
    "CONVERSION_BASED_UNIT('HALF_MM'"
    "CONVERSION_BASED_UNIT('TWIP'"
    "MEASURE_REPRESENTATION_ITEM('Nested length',LENGTH_MEASURE(3.)"
    "DERIVED_UNIT_ELEMENT("
    "MEASURE_REPRESENTATION_ITEM('Custom rate',NUMERIC_MEASURE(7.)")
  require_text("${step_text}" "${expected}" "re-authored AP214 unit graph")
endforeach()
file(READ "${EXPORT_REPORT}" export_report_text)
require_text("${export_report_text}" "\"outcome\":\"complete\""
  "structured-unit export report")
