if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED SOURCE OR
   NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED REPORT OR
   NOT DEFINED SCHEMA OR NOT DEFINED SCHEMA_LABEL OR NOT DEFINED FILE_NAME OR
   NOT DEFINED APPLICATION OR NOT DEFINED PRODUCT_ID OR NOT DEFINED PRODUCT_NAME OR
   NOT DEFINED ASSOCIATION OR NOT DEFINED TOP)
  message(FATAL_ERROR "all BREP-with-voids fixture parameters are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

# The topology is intentionally shared with the independent AP214 void
# fixture so identical oriented-shell semantics are compared across actual
# schema bindings.  Only schema-level product graph constructs are rewritten.
file(READ "${SOURCE}" input_text)
string(REGEX REPLACE
  "FILE_DESCRIPTION\\(\\([^\n]*\\)\\);"
  "FILE_DESCRIPTION(('BRL-CAD ${SCHEMA_LABEL} BREP with voids fixture'),'2;1');"
  input_text "${input_text}")
string(REGEX REPLACE
  "FILE_NAME\\('[^']*'"
  "FILE_NAME('${FILE_NAME}'"
  input_text "${input_text}")
string(REGEX REPLACE
  "FILE_SCHEMA\\(\\([^\n]*\\)\\);"
  "FILE_SCHEMA(('${SCHEMA}'));"
  input_text "${input_text}")
string(REPLACE
  "#320=APPLICATION_CONTEXT('automotive design');"
  "#320=APPLICATION_CONTEXT('${APPLICATION}');"
  input_text "${input_text}")
string(REGEX REPLACE
  "#321=APPLICATION_PROTOCOL_DEFINITION\\([^\n]*\n"
  ""
  input_text "${input_text}")
string(REPLACE
  "#324=PRODUCT('void_tetra','Void Tetra','BREP with oriented cavity',(#322));"
  "#324=PRODUCT('${PRODUCT_ID}','${PRODUCT_NAME}','BREP with oriented cavity',(#322));"
  input_text "${input_text}")
string(REPLACE
  "#329=SHAPE_DEFINITION_REPRESENTATION(#327,#328);"
  "#329=${ASSOCIATION}(#327,#328);"
  input_text "${input_text}")
file(WRITE "${INPUT}" "${input_text}")
file(REMOVE "${OUTPUT}" "${REPORT}")

execute_process(
  COMMAND "${STEP_G}" --strict -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "strict ${SCHEMA_LABEL} BREP-with-voids import returned ${import_result} or omitted output:\n"
    "${import_output}${import_error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"ADVANCED_BREP_SHAPE_REPRESENTATION\":1"
    "\"BREP_WITH_VOIDS\":1"
    "\"MANIFOLD_SOLID_BREP\":1"
    "\"ORIENTED_CLOSED_SHELL\":1"
    "\"STYLED_ITEM\":1"
    "\"PRESENTATION_LAYER_ASSIGNMENT\":1"
    "\"${ASSOCIATION}\":1"
    "\"products\":1,\"occurrences\":0,\"geometry_attempted\":2,\"geometry_written\":2,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"invalid_breps\":0"
    "\"skipped_items\":[]")
  require_text("${report_text}" "${expected}" "${SCHEMA_LABEL} BREP-with-voids report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep "${TOP}_item.s" info
  RESULT_VARIABLE void_result
  OUTPUT_VARIABLE void_output
  ERROR_VARIABLE void_error
)
set(void_text "${void_output}${void_error}")
if(NOT void_result EQUAL 0 OR NOT void_text MATCHES "Valid: YES, Solid: YES" OR
   NOT void_text MATCHES "faces:[ ]+8" OR NOT void_text MATCHES "vertices:[ ]+8")
  message(FATAL_ERROR "${SCHEMA_LABEL} BREP with void is wrong:\n${void_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep "${TOP}_item_step295.s" info
  RESULT_VARIABLE solid_result
  OUTPUT_VARIABLE solid_output
  ERROR_VARIABLE solid_error
)
set(solid_text "${solid_output}${solid_error}")
if(NOT solid_result EQUAL 0 OR NOT solid_text MATCHES "Valid: YES, Solid: YES" OR
   NOT solid_text MATCHES "faces:[ ]+4" OR NOT solid_text MATCHES "vertices:[ ]+4")
  message(FATAL_ERROR "${SCHEMA_LABEL} companion manifold solid is wrong:\n${solid_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" tree "${TOP}"
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
set(tree_text "${tree_output}${tree_error}")
require_text("${tree_text}" "${TOP}_item.s" "${SCHEMA_LABEL} void solid hierarchy")
require_text("${tree_text}" "${TOP}_item_step295.s" "${SCHEMA_LABEL} manifold solid hierarchy")

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "attr show ${TOP}_item"
  RESULT_VARIABLE attr_result
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}${attr_error}")
if(NOT attr_result EQUAL 0)
  message(FATAL_ERROR "unable to inspect ${TOP}_item attributes:\n${attr_text}")
endif()
foreach(expected
    "step:style_name"
    "void solid style"
    "step:color_rgb"
    "0.15 0.65 0.25"
    "step:layers"
    "void geometry"
    "step:style_source_ids")
  require_text("${attr_text}" "${expected}" "${SCHEMA_LABEL} presentation metadata")
endforeach()
