if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED TEMPLATE OR
   NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED REPORT OR
   NOT DEFINED SCHEMA OR NOT DEFINED SCHEMA_LABEL OR NOT DEFINED FILE_NAME OR
   NOT DEFINED SOLID_NAME OR NOT DEFINED TOLERANCE OR NOT DEFINED APPLICATION OR
   NOT DEFINED PRODUCT_ID OR NOT DEFINED PRODUCT_NAME OR NOT DEFINED ASSOCIATION OR
   NOT DEFINED TOP)
  message(FATAL_ERROR "all surface-model fixture parameters are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

# Derive an open-plus-closed surface-model case from the exact common tetra
# topology.  This retains one source of truth for all edge and plane geometry
# while giving every schema its own actual Part 21 association construct.
set(base_input "${INPUT}.base")
configure_file("${TEMPLATE}" "${base_input}" @ONLY)
file(READ "${base_input}" surface_text)
string(REPLACE
  "#90=CLOSED_SHELL('',(#48,#59,#68,#79));\n#91=MANIFOLD_SOLID_BREP('${SOLID_NAME}',#90);"
  "#80=OPEN_SHELL('open triangular shell',(#48));\n#81=SHELL_BASED_SURFACE_MODEL('open boundary model',(#80));\n#90=CLOSED_SHELL('closed tetrahedral shell',(#48,#59,#68,#79));\n#91=SHELL_BASED_SURFACE_MODEL('closed boundary model',(#90));"
  surface_text "${surface_text}")
string(REPLACE
  "#128=ADVANCED_BREP_SHAPE_REPRESENTATION('',(#91),#115);"
  "#128=MANIFOLD_SURFACE_SHAPE_REPRESENTATION('',(#81,#91),#115);"
  surface_text "${surface_text}")
file(WRITE "${INPUT}" "${surface_text}")
file(REMOVE "${OUTPUT}" "${REPORT}")

execute_process(
  COMMAND "${STEP_G}" --strict -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "strict ${SCHEMA_LABEL} surface-model import returned ${import_result} or omitted output:\n"
    "${import_output}${import_error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"MANIFOLD_SURFACE_SHAPE_REPRESENTATION\":1"
    "\"SHELL_BASED_SURFACE_MODEL\":2"
    "\"OPEN_SHELL\":1"
    "\"CLOSED_SHELL\":1"
    "\"${ASSOCIATION}\":1"
    "\"products\":1,\"occurrences\":0,\"geometry_attempted\":2,\"geometry_written\":2,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"invalid_breps\":0"
    "\"tolerance_mm\":${TOLERANCE}"
    "\"skipped_items\":[]")
  require_text("${report_text}" "${expected}" "${SCHEMA_LABEL} surface-model report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep "${TOP}_item.s" info
  RESULT_VARIABLE open_result
  OUTPUT_VARIABLE open_output
  ERROR_VARIABLE open_error
)
set(open_text "${open_output}${open_error}")
if(NOT open_result EQUAL 0 OR NOT open_text MATCHES "Valid: YES, Solid: NO" OR
   NOT open_text MATCHES "faces:[ ]+1" OR NOT open_text MATCHES "edges:[ ]+3")
  message(FATAL_ERROR "${SCHEMA_LABEL} open surface shell is wrong:\n${open_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep "${TOP}_item_step91.s" info
  RESULT_VARIABLE closed_result
  OUTPUT_VARIABLE closed_output
  ERROR_VARIABLE closed_error
)
set(closed_text "${closed_output}${closed_error}")
if(NOT closed_result EQUAL 0 OR NOT closed_text MATCHES "Valid: YES, Solid: YES" OR
   NOT closed_text MATCHES "faces:[ ]+4" OR NOT closed_text MATCHES "edges:[ ]+6")
  message(FATAL_ERROR "${SCHEMA_LABEL} closed surface shell is wrong:\n${closed_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" tree "${TOP}"
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
set(tree_text "${tree_output}${tree_error}")
require_text("${tree_text}" "${TOP}_item.s" "${SCHEMA_LABEL} open surface hierarchy")
require_text("${tree_text}" "${TOP}_item_step91.s" "${SCHEMA_LABEL} closed surface hierarchy")

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" get "${TOP}_item"
  OUTPUT_VARIABLE comb_output
  ERROR_VARIABLE comb_error
)
require_text("${comb_output}${comb_error}" "comb region no"
  "${SCHEMA_LABEL} open surface wrapper")

# The policy filter must remove only the authored open boundary.  It is an
# intentional selection decision, so it remains successful under --strict and
# is reported as filtered rather than as unsupported or failed geometry.
set(filtered_output "${OUTPUT}.skip_open_shells.g")
set(filtered_report "${REPORT}.skip_open_shells.json")
file(REMOVE "${filtered_output}" "${filtered_report}")
execute_process(
  COMMAND "${STEP_G}" --strict --skip-open-shells -o "${filtered_output}"
    --report "${filtered_report}" "${INPUT}"
  RESULT_VARIABLE filtered_result
  OUTPUT_VARIABLE filtered_output_text
  ERROR_VARIABLE filtered_error
)
if(NOT filtered_result EQUAL 0 OR NOT EXISTS "${filtered_output}")
  message(FATAL_ERROR
    "strict filtered ${SCHEMA_LABEL} surface-model import returned "
    "${filtered_result} or omitted output:\n"
    "${filtered_output_text}${filtered_error}")
endif()
file(READ "${filtered_report}" filtered_report_text)
foreach(expected
    "\"skip_open_shells\":true"
    "\"products\":1,\"occurrences\":0,\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0,\"outcome\":\"complete\""
    "\"status\":\"filtered\""
    "\"filtered_items\":1"
    "\"geometry_filtered\":1"
    "OPEN_SHELL boundary excluded by --skip-open-shells"
    "\"skipped_items\":[]")
  require_text("${filtered_report_text}" "${expected}"
    "${SCHEMA_LABEL} filtered surface-model report")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${filtered_output}" brep "${TOP}_item_step91.s" info
  RESULT_VARIABLE filtered_closed_result
  OUTPUT_VARIABLE filtered_closed_output
  ERROR_VARIABLE filtered_closed_error
)
set(filtered_closed_text "${filtered_closed_output}${filtered_closed_error}")
if(NOT filtered_closed_result EQUAL 0 OR
   NOT filtered_closed_text MATCHES "Valid: YES, Solid: YES" OR
   NOT filtered_closed_text MATCHES "faces:[ ]+4" OR
   NOT filtered_closed_text MATCHES "edges:[ ]+6")
  message(FATAL_ERROR
    "${SCHEMA_LABEL} filtered closed surface shell is wrong:\n"
    "${filtered_closed_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${filtered_output}" tree "${TOP}"
  OUTPUT_VARIABLE filtered_tree_output
  ERROR_VARIABLE filtered_tree_error
)
set(filtered_tree_text "${filtered_tree_output}${filtered_tree_error}")
require_text("${filtered_tree_text}" "${TOP}_item_step91.s"
  "${SCHEMA_LABEL} filtered closed surface hierarchy")
string(FIND "${filtered_tree_text}" "${TOP}_item.s" filtered_open_found)
if(NOT filtered_open_found EQUAL -1)
  message(FATAL_ERROR
    "${SCHEMA_LABEL} filtered hierarchy still contains open shell:\n"
    "${filtered_tree_text}")
endif()
