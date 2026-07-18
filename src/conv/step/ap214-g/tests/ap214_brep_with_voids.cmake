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
    "\"BREP_WITH_VOIDS\":1"
    "\"MANIFOLD_SOLID_BREP\":1"
    "\"ORIENTED_CLOSED_SHELL\":1"
    "\"products\":1"
    "\"styles_applied\":1"
    "\"layers_extracted\":1"
    "\"geometry_attempted\":2"
    "\"geometry_written\":2"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Void_Tetra_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+8" OR
   NOT brep_text MATCHES "edges:[ ]+12" OR
   NOT brep_text MATCHES "vertices:[ ]+8")
  message(FATAL_ERROR "BREP-with-voids validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Void_Tetra_item_step295.s info
  OUTPUT_VARIABLE shared_output
  ERROR_VARIABLE shared_error
)
set(shared_text "${shared_output}\n${shared_error}")
if(NOT shared_text MATCHES "Valid: YES, Solid: YES" OR
   NOT shared_text MATCHES "faces:[ ]+4" OR
   NOT shared_text MATCHES "edges:[ ]+6" OR
   NOT shared_text MATCHES "vertices:[ ]+4")
  message(FATAL_ERROR "separately emitted shared-shell BREP validation failed\n${shared_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Void_Tetra_item
  OUTPUT_VARIABLE item_attr_output
  ERROR_VARIABLE item_attr_error
)
set(item_attr_text "${item_attr_output}\n${item_attr_error}")
if(NOT item_attr_text MATCHES "step:source_id[ ]+292" OR
   NOT item_attr_text MATCHES "step:color_rgb[ ]+0.15 0.65 0.25" OR
   NOT item_attr_text MATCHES "step:layers[ ]+void geometry")
  message(FATAL_ERROR "BREP-with-voids item metadata validation failed\n${item_attr_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Void_Tetra
  OUTPUT_VARIABLE product_attr_output
  ERROR_VARIABLE product_attr_error
)
set(product_attr_text "${product_attr_output}\n${product_attr_error}")
if(NOT product_attr_text MATCHES "step:product_id[ ]+void_tetra" OR
   NOT product_attr_text MATCHES "step:revision[ ]+A" OR
   NOT product_attr_text MATCHES "step:definition_description[ ]+void-bearing product")
  message(FATAL_ERROR "BREP-with-voids product metadata validation failed\n${product_attr_text}")
endif()
