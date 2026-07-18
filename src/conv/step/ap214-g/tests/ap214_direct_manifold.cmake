if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR "ap214-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"products\":2"
    "\"styles_extracted\":1"
    "\"styles_applied\":1"
    "\"layers_extracted\":1"
    "\"materials_extracted\":2"
    "\"properties_extracted\":5"
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
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
