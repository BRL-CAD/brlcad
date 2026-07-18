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
    "\"ADVANCED_BREP_SHAPE_REPRESENTATION\":1"
    "\"MAPPED_ITEM\":2"
    "\"REPRESENTATION_MAP\":1"
    "\"products\":1"
    "\"styles_applied\":1"
    "\"layers_extracted\":1"
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" get Mapped_Tetra
  OUTPUT_VARIABLE tree_output
  ERROR_VARIABLE tree_error
)
set(tree_text "${tree_output}\n${tree_error}")
string(REGEX MATCHALL "Mapped_Tetra_mapped_item" mapped_members "${tree_text}")
list(LENGTH mapped_members mapped_member_count)
if(NOT mapped_member_count EQUAL 2 OR
   NOT tree_text MATCHES "1 0 0 25  0 1 0 5  0 0 1 0" OR
   NOT tree_text MATCHES "1 0 0 -5  0 1 0 20  0 0 1 0")
  message(FATAL_ERROR "mapped representation was not reused with both transforms\n${tree_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Mapped_Tetra_mapped_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+4" OR
   NOT brep_text MATCHES "edges:[ ]+6")
  message(FATAL_ERROR "mapped source BREP validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Mapped_Tetra_mapped_item
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+91" OR
   NOT attr_text MATCHES "step:representation_map_id[ ]+130" OR
   NOT attr_text MATCHES "step:color_rgb[ ]+0.5 0.2 0.8" OR
   NOT attr_text MATCHES "step:layers[ ]+mapped instances")
  message(FATAL_ERROR "mapped representation metadata validation failed\n${attr_text}")
endif()
