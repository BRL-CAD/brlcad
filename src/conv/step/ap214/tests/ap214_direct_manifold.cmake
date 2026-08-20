if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "step-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"products\":2"
    "\"styles_extracted\":1"
    "\"styles_applied\":1"
    "\"layers_extracted\":1"
    "\"materials_extracted\":2"
    "\"properties_extracted\":5"
    "\"product_metadata\":["
    "\"identifier\":\"AL-TEST\""
    "\"name\":\"Fixture Aluminum\""
    "\"name\":\"Anodized\""
    "\"name\":\"volume measure\""
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

# A required FACE_SURFACE geometry reference may be absent in malformed
# producer output.  Reject that face during adapter loading; it must never
# survive as a null surface and crash the later geometry worker.
file(READ "${INPUT}" malformed_face_source)
string(REPLACE
  "#48=ADVANCED_FACE('',(#44),#47,.T.);"
  "#48=ADVANCED_FACE('',(#44),$,.T.);"
  malformed_face_source "${malformed_face_source}")
set(malformed_face_input "${OUTPUT}.missing-face-geometry.step")
set(malformed_face_output "${OUTPUT}.missing-face-geometry.g")
set(malformed_face_report "${REPORT}.missing-face-geometry.json")
file(WRITE "${malformed_face_input}" "${malformed_face_source}")
file(REMOVE "${malformed_face_output}" "${malformed_face_report}")
execute_process(
  COMMAND "${STEP_G}" -O "${malformed_face_output}"
    --report "${malformed_face_report}" "${malformed_face_input}"
  RESULT_VARIABLE malformed_face_result
  OUTPUT_VARIABLE malformed_face_output_text
  ERROR_VARIABLE malformed_face_error
)
if(NOT malformed_face_result EQUAL 3 OR EXISTS "${malformed_face_output}")
  message(FATAL_ERROR
    "missing face geometry was not rejected safely (status ${malformed_face_result})\n${malformed_face_output_text}\n${malformed_face_error}")
endif()
file(READ "${malformed_face_report}" malformed_face_report_text)
foreach(expected
    "\"outcome\":\"failed\""
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "missing required face geometry")
  string(FIND "${malformed_face_report_text}" "${expected}" malformed_face_found)
  if(malformed_face_found EQUAL -1)
    message(FATAL_ERROR
      "missing-face report does not contain ${expected}:\n${malformed_face_report_text}")
  endif()
endforeach()
string(FIND "${malformed_face_report_text}"
  "\"brep_construction_pullback\"" malformed_face_pullback)
if(NOT malformed_face_pullback EQUAL -1)
  message(FATAL_ERROR
    "missing required face geometry triggered an inapplicable pullback retry:\n"
    "${malformed_face_report_text}")
endif()
set(malformed_face_strict_report
  "${REPORT}.missing-face-geometry.strict.json")
file(REMOVE "${malformed_face_strict_report}")
execute_process(
  COMMAND "${STEP_G}" --strict -D
    --report "${malformed_face_strict_report}" "${malformed_face_input}"
  RESULT_VARIABLE malformed_face_strict_result
  OUTPUT_VARIABLE malformed_face_strict_output
  ERROR_VARIABLE malformed_face_strict_error
)
if(NOT malformed_face_strict_result EQUAL 3)
  message(FATAL_ERROR
    "strict missing-face import returned ${malformed_face_strict_result}\n${malformed_face_strict_output}\n${malformed_face_strict_error}")
endif()

# The schema's valid_units rule requires POSITIVE_RATIO_MEASURE to be
# dimensionless.  In the narrowly identified density context, permissive mode
# normalizes this common producer defect to NUMERIC_MEASURE while preserving
# the positive value and exact mass/volume unit.
file(READ "${INPUT}" invalid_source)
string(REPLACE
  "NUMERIC_MEASURE(0.0000027)"
  "POSITIVE_RATIO_MEASURE(0.0000027)"
  invalid_source "${invalid_source}")
set(invalid_input "${OUTPUT}.invalid.step")
set(invalid_output "${OUTPUT}.invalid.g")
set(invalid_report "${REPORT}.invalid.json")
file(WRITE "${invalid_input}" "${invalid_source}")
file(REMOVE "${invalid_output}" "${invalid_report}")
execute_process(
  COMMAND "${STEP_G}" -O "${invalid_output}" --report "${invalid_report}"
    "${invalid_input}"
  RESULT_VARIABLE invalid_result
  OUTPUT_VARIABLE invalid_output_text
  ERROR_VARIABLE invalid_error
)
if(NOT invalid_result EQUAL 0)
  message(FATAL_ERROR
    "invalid property import returned ${invalid_result}\n${invalid_output_text}\n${invalid_error}")
endif()
file(READ "${invalid_report}" invalid_report_text)
foreach(expected
    "\"outcome\":\"complete\""
    "\"properties_extracted\":5"
    "\"properties_invalid\":0"
    "\"name\":\"density measure\""
    "\"value_type\":\"numeric_measure\""
    "\"valid\":true"
    "normalized an explicit dimensioned density from a ratio measure")
  string(FIND "${invalid_report_text}" "${expected}" invalid_found)
  if(invalid_found EQUAL -1)
    message(FATAL_ERROR
      "invalid-property report does not contain ${expected}:\n${invalid_report_text}")
  endif()
endforeach()
execute_process(
  COMMAND "${MGED}" -c "${invalid_output}" attr show Direct_Tetra
  OUTPUT_VARIABLE invalid_attr_output
  ERROR_VARIABLE invalid_attr_error
)
set(invalid_attr_text "${invalid_attr_output}\n${invalid_attr_error}")
if(NOT invalid_attr_text MATCHES "step:density[ ]+2.7e-06" OR
   NOT invalid_attr_text MATCHES "step:density_units[ ]+kilogram.1.millimetre.-3")
  message(FATAL_ERROR
    "repaired density was not retained as exportable metadata\n${invalid_attr_text}")
endif()

# Some files from the same producer family additionally multiply by the
# length cubed unit instead of dividing by it.  The explicit density role,
# positive value, and exact mass^1*length^3 graph make that exponent inversion
# unambiguous; do not invert the numeric value.
string(REPLACE
  "DERIVED_UNIT_ELEMENT(#111,-3.)"
  "DERIVED_UNIT_ELEMENT(#111,3.)"
  reversed_source "${invalid_source}")
set(reversed_input "${OUTPUT}.reversed-density.step")
set(reversed_output "${OUTPUT}.reversed-density.g")
set(reversed_report "${REPORT}.reversed-density.json")
file(WRITE "${reversed_input}" "${reversed_source}")
file(REMOVE "${reversed_output}" "${reversed_report}")
execute_process(
  COMMAND "${STEP_G}" -O "${reversed_output}" --report "${reversed_report}"
    "${reversed_input}"
  RESULT_VARIABLE reversed_result
  OUTPUT_VARIABLE reversed_output_text
  ERROR_VARIABLE reversed_error
)
if(NOT reversed_result EQUAL 0)
  message(FATAL_ERROR
    "reversed-density repair returned ${reversed_result}\n${reversed_output_text}\n${reversed_error}")
endif()
file(READ "${reversed_report}" reversed_report_text)
foreach(expected
    "\"outcome\":\"complete\""
    "\"properties_invalid\":0"
    "\"value_type\":\"numeric_measure\""
    "\"valid\":true"
    "corrected an explicit density unit from mass*length^3 to mass*length^-3"
    "normalized an explicit dimensioned density from a ratio measure")
  string(FIND "${reversed_report_text}" "${expected}" reversed_found)
  if(reversed_found EQUAL -1)
    message(FATAL_ERROR
      "reversed-density report does not contain ${expected}:\n${reversed_report_text}")
  endif()
endforeach()
execute_process(
  COMMAND "${MGED}" -c "${reversed_output}" attr show Direct_Tetra
  OUTPUT_VARIABLE reversed_attr_output
  ERROR_VARIABLE reversed_attr_error
)
set(reversed_attr_text "${reversed_attr_output}\n${reversed_attr_error}")
if(NOT reversed_attr_text MATCHES "step:density[ ]+2.7e-06" OR
   NOT reversed_attr_text MATCHES "step:density_units[ ]+kilogram.1.millimetre.-3")
  message(FATAL_ERROR
    "reversed density was not normalized for export\n${reversed_attr_text}")
endif()

set(strict_invalid_output "${OUTPUT}.invalid.strict.g")
set(strict_invalid_report "${REPORT}.invalid.strict.json")
file(REMOVE "${strict_invalid_output}" "${strict_invalid_report}")
execute_process(
  COMMAND "${STEP_G}" --strict -O "${strict_invalid_output}"
    --report "${strict_invalid_report}" "${reversed_input}"
  RESULT_VARIABLE strict_invalid_result
  OUTPUT_VARIABLE strict_invalid_output_text
  ERROR_VARIABLE strict_invalid_error
)
if(NOT strict_invalid_result EQUAL 3 OR EXISTS "${strict_invalid_output}")
  message(FATAL_ERROR
    "strict invalid property import was not transactional (status ${strict_invalid_result})\n${strict_invalid_output_text}\n${strict_invalid_error}")
endif()
file(READ "${strict_invalid_report}" strict_invalid_report_text)
foreach(expected
    "\"outcome\":\"failed\""
    "\"properties_invalid\":1"
    "\"valid\":false")
  string(FIND "${strict_invalid_report_text}" "${expected}" strict_invalid_found)
  if(strict_invalid_found EQUAL -1)
    message(FATAL_ERROR
      "strict invalid-property report does not contain ${expected}:\n${strict_invalid_report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Direct_Tetra_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
# MGED reports a harmless missing interactive callback in command-line mode;
# validate the command's actual BREP output instead of its aggregate status.
if(NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+4" OR
   NOT brep_text MATCHES "edges:[ ]+6")
  message(FATAL_ERROR "direct manifold BREP validation failed\n${brep_output}\n${brep_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Direct_Tetra_item
  RESULT_VARIABLE attr_result
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+91" OR
   NOT attr_text MATCHES "step:color_rgb[ ]+0.2 0.4 0.6" OR
   NOT attr_text MATCHES "step:layers[ ]+geometry layer")
  message(FATAL_ERROR "direct manifold metadata validation failed\n${attr_output}\n${attr_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Direct_Tetra
  OUTPUT_VARIABLE product_attr_output
  ERROR_VARIABLE product_attr_error
)
set(product_attr_text "${product_attr_output}\n${product_attr_error}")
if(NOT product_attr_text MATCHES "step:product_id[ ]+direct_tetra" OR
   NOT product_attr_text MATCHES "step:description[ ]+exact solid fixture" OR
   NOT product_attr_text MATCHES "step:revision[ ]+1" OR
   NOT product_attr_text MATCHES "step:revision_description[ ]+initial revision" OR
   NOT product_attr_text MATCHES "step:definition_id[ ]+design" OR
   NOT product_attr_text MATCHES "step:definition_description[ ]+fixture product definition" OR
   NOT product_attr_text MATCHES "step:material_id[ ]+AL-TEST" OR
   NOT product_attr_text MATCHES "step:material_name[ ]+Fixture Aluminum" OR
   NOT product_attr_text MATCHES "step:material:2:name[ ]+Anodized" OR
   NOT product_attr_text MATCHES "step:material:2:finish_name[ ]+Anodized" OR
   NOT product_attr_text MATCHES "step:material:2:coating_density[ ]+1.1999999999999999e-06" OR
   NOT product_attr_text MATCHES "step:material:2:coating_density_units[ ]+kilogram.1.millimetre.-3" OR
   NOT product_attr_text MATCHES "step:density[ ]+2.7e-06" OR
   NOT product_attr_text MATCHES "step:density_units[ ]+kilogram.1.millimetre.-3" OR
   NOT product_attr_text MATCHES "step:validation:volume_measure[ ]+166.666" OR
   NOT product_attr_text MATCHES "step:validation:centre_point[ ]+2.5 2.5 2.5")
  message(FATAL_ERROR "product metadata validation failed\n${product_attr_output}\n${product_attr_error}")
endif()
foreach(expected
    "step:property:1:category[ ]+geometric validation property"
    "step:property:1:name[ ]+volume measure"
    "step:property:1:value_type[ ]+volume_measure"
    "step:property:1:units[ ]+millimetre.3"
    "step:property:2:name[ ]+centre point"
    "step:property:2:value_type[ ]+cartesian_point"
    "step:property:2:values[ ]+2.5 2.5 2.5")
  if(NOT product_attr_text MATCHES "${expected}")
    message(FATAL_ERROR
      "structured product property is missing ${expected}\n${product_attr_text}")
  endif()
endforeach()
