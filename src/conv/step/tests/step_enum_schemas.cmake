if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED WORK_DIRECTORY)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, and WORK_DIRECTORY are required")
endif()

file(READ "${INPUT}" ap203_text)
set(schema_cases
  "ap203|CONFIG_CONTROL_DESIGN"
  "ap203_interim|CONFIG_CONTROL_DESIGN','GEOMETRIC_VALIDATION_PROPERTIES_MIM','SHAPE_APPEARANCE_LAYER_MIM"
  "ap203e2|AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF { 1 0 10303 403 1 1 4 }"
  "ap214|AUTOMOTIVE_DESIGN { 1 0 10303 214 3 1 1 1 }"
  "ap242|AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF { 1 0 10303 442 1 1 4 }"
)

foreach(schema_case IN LISTS schema_cases)
  string(FIND "${schema_case}" "|" separator)
  string(SUBSTRING "${schema_case}" 0 ${separator} schema)
  math(EXPR identifier_start "${separator} + 1")
  string(SUBSTRING "${schema_case}" ${identifier_start} -1 identifier)
  string(REPLACE "CONFIG_CONTROL_DESIGN" "${identifier}" step_text "${ap203_text}")

  set(step_file "${WORK_DIRECTORY}/step_enum_${schema}.stp")
  set(output "${WORK_DIRECTORY}/step_enum_${schema}.g")
  set(report "${WORK_DIRECTORY}/step_enum_${schema}.json")
  file(WRITE "${step_file}" "${step_text}")
  file(REMOVE "${output}" "${report}")
  execute_process(
    COMMAND "${STEP_G}" -f -j 1 -o "${output}" --report "${report}" "${step_file}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_output
    ERROR_VARIABLE import_error
    TIMEOUT 60
  )
  if(NOT import_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} symbolic-enum import failed (${import_result})\n${import_output}\n${import_error}")
  endif()

  foreach(expected_summary
      "Loaded "
      "B_Spline_Curve_With_Knots"
      "Named_Unit"
      "Used ")
    string(FIND "${import_output}" "${expected_summary}" summary_found)
    if(summary_found EQUAL -1)
      message(FATAL_ERROR
        "${schema} pre-conversion entity census omits '${expected_summary}':\n${import_output}")
    endif()
  endforeach()
  if(schema STREQUAL "ap203_interim")
    foreach(expected_profile_text
        "accepting legacy/interim FILE_SCHEMA identifier or profile as ap203e2")
      string(FIND "${import_output}${import_error}" "${expected_profile_text}" profile_found)
      if(profile_found EQUAL -1)
        message(FATAL_ERROR
          "interim AP203 profile was not routed through AP203e2: missing "
          "'${expected_profile_text}':\n${import_output}${import_error}")
      endif()
    endforeach()
  endif()

  file(READ "${report}" report_text)
  if(schema STREQUAL "ap203_interim")
    string(FIND "${report_text}" "STEP::AP203e2::FILE_SCHEMA" profile_schema_found)
    if(profile_schema_found EQUAL -1)
      message(FATAL_ERROR
        "interim AP203 profile report does not identify the AP203e2 binding:\n"
        "${report_text}")
    endif()
  endif()
  foreach(expected
      "\"B_SPLINE_CURVE_WITH_KNOTS\":1"
      "\"NAMED_UNIT\":3"
      "\"repair\":\"safe\""
      "\"requested_invalid_brep_policy\":\"preserve\""
      "\"effective_invalid_brep_policy\":\"preserve\""
      "\"geometry_attempted\":1"
      "\"geometry_written\":1"
      "\"geometry_skipped\":0"
      "\"invalid_breps\":0")
    string(FIND "${report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "${schema} report does not contain ${expected}:\n${report_text}")
    endif()
  endforeach()

  execute_process(
    COMMAND "${MGED}" -c "${output}"
      brep AP203_Unclamped_Closed_Curve_Knots_item.s info
    RESULT_VARIABLE brep_result
    OUTPUT_VARIABLE brep_output
    ERROR_VARIABLE brep_error
  )
  set(brep_text "${brep_output}\n${brep_error}")
  if(NOT brep_result EQUAL 0 OR
     NOT brep_text MATCHES "Valid: YES, Solid: NO, Plate mode: YES" OR
     NOT brep_text MATCHES "faces:[ ]+1" OR
     NOT brep_text MATCHES "edges:[ ]+1" OR
     NOT brep_text MATCHES "vertices:[ ]+1")
    message(FATAL_ERROR "${schema} symbolic-enum geometry is invalid:\n${brep_text}")
  endif()
endforeach()
