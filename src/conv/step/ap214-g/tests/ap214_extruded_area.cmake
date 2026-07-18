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
    "\"styles_extracted\":1"
    "\"styles_applied\":1"
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"EXTRUDED_AREA_SOLID\":1")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Extruded_Block_swept_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+10" OR
   NOT brep_text MATCHES "edges:[ ]+24")
  message(FATAL_ERROR "extruded-area BREP validation failed\n${brep_output}\n${brep_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Extruded_Block_swept_item
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+71" OR
   NOT attr_text MATCHES "step:color_rgb[ ]+0.1 0.5 0.9")
  message(FATAL_ERROR "extruded-area metadata validation failed\n${attr_output}\n${attr_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" bb -q -e Extruded_Block_swept_item.s
  OUTPUT_VARIABLE bbox_output
  ERROR_VARIABLE bbox_error
)
set(bbox_text "${bbox_output}\n${bbox_error}")
# BRL-CAD's ray-tracing bbox intentionally pads BREP extents by its working
# tolerance.  Verify the expected millimetre-scale coordinates including that
# small symmetric padding.
if(NOT bbox_text MATCHES "min \\{-0\\.00[0-9]+ -0\\.00[0-9]+ -0\\.00[0-9]+\\} max \\{10\\.00[0-9]+ 8\\.00[0-9]+ 5\\.00[0-9]+\\}")
  message(FATAL_ERROR "extruded-area bounding box validation failed\n${bbox_output}\n${bbox_error}")
endif()
