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
    "\"products\":1"
    "\"SURFACE_REPLICA\":1"
    "\"styles_extracted\":2"
    "\"styles_applied\":2"
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
  COMMAND "${MGED}" -c "${OUTPUT}" brep Surface_Models_item.s info
  OUTPUT_VARIABLE open_output
  ERROR_VARIABLE open_error
)
set(open_text "${open_output}\n${open_error}")
if(NOT open_text MATCHES "Valid: YES, Solid: NO" OR
   NOT open_text MATCHES "faces:[ ]+1" OR
   NOT open_text MATCHES "edges:[ ]+3")
  message(FATAL_ERROR "open surface-shell validation failed\n${open_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Surface_Models_item_step93.s info
  OUTPUT_VARIABLE closed_output
  ERROR_VARIABLE closed_error
)
set(closed_text "${closed_output}\n${closed_error}")
if(NOT closed_text MATCHES "Valid: YES, Solid: YES" OR
   NOT closed_text MATCHES "faces:[ ]+4" OR
   NOT closed_text MATCHES "edges:[ ]+6")
  message(FATAL_ERROR "closed surface-shell validation failed\n${closed_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" tree Surface_Models
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
set(tree_text "${tree_output}\n${tree_error}")
if(NOT tree_text MATCHES "Surface_Models_item" OR
   NOT tree_text MATCHES "Surface_Models_item_step93")
  message(FATAL_ERROR "surface-shell product hierarchy is incomplete\n${tree_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" get Surface_Models_item
  OUTPUT_VARIABLE comb_output
  ERROR_VARIABLE comb_error
)
set(comb_text "${comb_output}\n${comb_error}")
if(NOT comb_text MATCHES "comb region no")
  message(FATAL_ERROR "open surface shell was incorrectly promoted to a region\n${comb_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Surface_Models_item
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+91" OR
   NOT attr_text MATCHES "step:color_rgb[ ]+0.1 0.7 0.3" OR
   NOT attr_text MATCHES "step:layers[ ]+surface layer")
  message(FATAL_ERROR "surface-shell metadata validation failed\n${attr_text}")
endif()
