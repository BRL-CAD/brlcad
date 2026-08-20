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

set(input "${OUTPUT_DIR}/g_step_units_input.g")
file(REMOVE "${input}")
execute_process(
  COMMAND "${MGED}" -c "${input}"
    "in box.s rpp 0 25.4 0 50.8 0 76.2; r box.r u box.s; put all.g comb region no tree {u {l box.r} {l box.r {1 0 0 25.4  0 1 0 0  0 0 1 0  0 0 0 1}}}; in cone.s trc 0 0 0 0 0 25.4 12.7 6.35; r cone.r u cone.s"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create the unit-policy fixture:\n${create_output}${create_error}")
endif()

# Every enabled schema uses the same conversion-based inch, radian, explicit
# uncertainty, BRep-coordinate, and rigid-placement policy.
foreach(schema ap203 ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  set(step "${OUTPUT_DIR}/g_step_units_${schema}.stp")
  set(roundtrip "${OUTPUT_DIR}/g_step_units_${schema}.g")
  set(export_report "${OUTPUT_DIR}/g_step_units_${schema}_export.json")
  set(import_report "${OUTPUT_DIR}/g_step_units_${schema}_import.json")
  file(REMOVE "${step}" "${roundtrip}" "${export_report}" "${import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --output-units in
      --angle-units radian --uncertainty 0.002 --strict
      --report "${export_report}" -o "${step}" "${input}" all.g
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result EQUAL 0 OR NOT EXISTS "${step}")
    message(FATAL_ERROR
      "${schema} inch/radian export failed (${export_result}):\n"
      "${export_output}${export_error}")
  endif()
  file(READ "${step}" step_text)
  foreach(expected
      "UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(0.002)"
      "LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(0.0254)"
      "CONVERSION_BASED_UNIT('IN'"
      "SI_UNIT($,.RADIAN.)"
      "CARTESIAN_POINT('',(1.,2.,3.))")
    require_text("${step_text}" "${expected}" "${schema} unit context")
  endforeach()
  if(step_text MATCHES "CONVERSION_BASED_UNIT\\('DEGREES'")
    message(FATAL_ERROR "${schema} radian context also emitted degrees")
  endif()
  file(READ "${export_report}" export_report_text)
  foreach(expected
      "\"output_units\":\"in\""
      "\"length_unit_mm\":25.4"
      "\"angle_units\":\"radian\""
      "\"uncertainty\":0.002"
      "\"outcome\":\"complete\"")
    require_text("${export_report_text}" "${expected}"
      "${schema} unit export report")
  endforeach()

  execute_process(
    COMMAND "${STEP_G}" --schema "${schema}" --strict -O "${roundtrip}"
      --report "${import_report}" "${step}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_output
    ERROR_VARIABLE import_error
  )
  if(NOT import_result EQUAL 0 OR NOT EXISTS "${roundtrip}")
    message(FATAL_ERROR
      "${schema} inch/radian reimport failed (${import_result}):\n"
      "${import_output}${import_error}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "XDG_CACHE_HOME=${OUTPUT_DIR}/g_step_units_cache"
      "${MGED}" -c "${roundtrip}" bb -q -e box_r_item.s
    RESULT_VARIABLE bounds_result
    OUTPUT_VARIABLE bounds_output
    ERROR_VARIABLE bounds_error
  )
  set(bounds_text "${bounds_output}${bounds_error}")
  if(NOT bounds_result EQUAL 0)
    message(FATAL_ERROR "could not inspect ${schema} box bounds:\n${bounds_text}")
  endif()
  require_text("${bounds_text}" "max {25.405500 50.805500 76.205500}"
    "${schema} physical box dimensions")
  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}" get all_g
    RESULT_VARIABLE tree_result
    OUTPUT_VARIABLE tree_output
    ERROR_VARIABLE tree_error
  )
  set(tree_text "${tree_output}${tree_error}")
  if(NOT tree_result EQUAL 0)
    message(FATAL_ERROR "could not inspect ${schema} assembly:\n${tree_text}")
  endif()
  require_text("${tree_text}" "1 0 0 25.4"
    "${schema} physical assembly translation")
  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}" attr show box_r
    RESULT_VARIABLE attribute_result
    OUTPUT_VARIABLE attribute_output
    ERROR_VARIABLE attribute_error
  )
  set(attribute_text "${attribute_output}${attribute_error}")
  if(NOT attribute_result EQUAL 0)
    message(FATAL_ERROR
      "could not inspect ${schema} source-unit provenance:\n${attribute_text}")
  endif()
  foreach(expected
      "step:source_context:1:length_unit                 in"
      "step:source_context:1:length_unit_mm              25.399"
      "step:source_context:1:plane_angle_unit            radian"
      "step:source_context:1:plane_angle_unit_radians    1"
      "step:source_context:1:uncertainty_mm              0.050799")
    require_text("${attribute_text}" "${expected}"
      "${schema} source-unit provenance")
  endforeach()
endforeach()

# The schemas with native CSG must apply the same length and plane-angle
# policy to analytic parameters, not merely to BRep control points.
foreach(schema ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
  set(step "${OUTPUT_DIR}/g_step_units_csg_${schema}.stp")
  set(roundtrip "${OUTPUT_DIR}/g_step_units_csg_${schema}.g")
  file(REMOVE "${step}" "${roundtrip}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --native-csg
      --output-units in --angle-units radian --strict
      -o "${step}" "${input}" cone.r
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result EQUAL 0 OR NOT EXISTS "${step}")
    message(FATAL_ERROR
      "${schema} native unit export failed (${export_result}):\n"
      "${export_output}${export_error}")
  endif()
  file(READ "${step}" step_text)
  require_text("${step_text}"
    "RIGHT_CIRCULAR_CONE('cone.s'," "${schema} native cone")
  require_text("${step_text}" ",1.,0.5,-0.244978663126864)"
    "${schema} native cone inch/radian parameters")

  execute_process(
    COMMAND "${STEP_G}" --schema "${schema}" --strict
      -O "${roundtrip}" "${step}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_output
    ERROR_VARIABLE import_error
  )
  if(NOT import_result EQUAL 0 OR NOT EXISTS "${roundtrip}")
    message(FATAL_ERROR
      "${schema} native unit reimport failed (${import_result}):\n"
      "${import_output}${import_error}")
  endif()
  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}" db get cone_r_csg_primitive.s
    RESULT_VARIABLE cone_result
    OUTPUT_VARIABLE cone_output
    ERROR_VARIABLE cone_error
  )
  set(cone_text "${cone_output}${cone_error}")
  if(NOT cone_result EQUAL 0)
    message(FATAL_ERROR "could not inspect ${schema} native cone:\n${cone_text}")
  endif()
  foreach(expected "H {0 0 25.399" "12.699" "6.350")
    require_text("${cone_text}" "${expected}"
      "${schema} reconstructed native cone")
  endforeach()
endforeach()
