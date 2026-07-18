if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED NIRT OR NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, NIRT, INPUT, REPORT, and OUTPUT are required")
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
    "\"REVOLVED_AREA_SOLID\":1")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Revolved_Ring_swept_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: YES")
  message(FATAL_ERROR "revolved-area BREP validation failed\n${brep_output}\n${brep_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Revolved_Ring_swept_item
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+36" OR
   NOT attr_text MATCHES "step:color_rgb[ ]+0.2 0.7 0.3")
  message(FATAL_ERROR "revolved-area metadata validation failed\n${attr_output}\n${attr_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" bb -q -e Revolved_Ring_swept_item.s
  OUTPUT_VARIABLE bbox_output
  ERROR_VARIABLE bbox_error
)
set(bbox_text "${bbox_output}\n${bbox_error}")
if(NOT bbox_text MATCHES "min \\{-7\\.00[0-9]+ -2\\.00[0-9]+ -7\\.00[0-9]+\\} max \\{7\\.00[0-9]+ 2\\.00[0-9]+ 7\\.00[0-9]+\\}")
  message(FATAL_ERROR "revolved-area bounding box validation failed\n${bbox_output}\n${bbox_error}")
endif()

execute_process(
  COMMAND "${NIRT}" -H 0 -b -f csv -e "xyz -10 0 0; dir 1 0 0; s; q" "${OUTPUT}" Revolved_Ring_swept_item
  RESULT_VARIABLE ray_result
  OUTPUT_VARIABLE ray_output
  ERROR_VARIABLE ray_error
)
set(ray_text "${ray_output}\n${ray_error}")
if(NOT ray_result EQUAL 0 OR
   NOT ray_text MATCHES ",-7\\.000000,0\\.000000,0\\.000000,7\\.000000,-5\\.000000" OR
   NOT ray_text MATCHES ",5\\.000000,0\\.000000,0\\.000000,-5\\.000000,7\\.000000")
  message(FATAL_ERROR "revolved-area ray smoke test failed\n${ray_output}\n${ray_error}")
endif()

# Exercise exact partial-angle capping from the same owned profile.  Keeping the
# profile identical makes this a focused regression for angular end topology.
get_filename_component(test_directory "${OUTPUT}" DIRECTORY)
set(partial_input "${test_directory}/ap214_revolved_area_partial.stp")
set(partial_report "${test_directory}/ap214_revolved_area_partial.json")
set(partial_output "${test_directory}/ap214_revolved_area_partial.g")
file(READ "${INPUT}" partial_text)
string(REPLACE
  "#36=REVOLVED_AREA_SOLID('rectangular ring',#35,#7,6.283185307179586);"
  "#36=REVOLVED_AREA_SOLID('rectangular ring',#35,#7,1.5707963267948966);"
  partial_text "${partial_text}")
file(WRITE "${partial_input}" "${partial_text}")
file(REMOVE "${partial_report}" "${partial_output}")
execute_process(
  COMMAND "${AP214_G}" -O "${partial_output}" --report "${partial_report}" "${partial_input}"
  RESULT_VARIABLE partial_result
  OUTPUT_VARIABLE partial_import_output
  ERROR_VARIABLE partial_import_error
)
if(NOT partial_result EQUAL 0)
  message(FATAL_ERROR "partial ap214-g returned ${partial_result}\n${partial_import_output}\n${partial_import_error}")
endif()

file(READ "${partial_report}" partial_report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"REVOLVED_AREA_SOLID\":1")
  string(FIND "${partial_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "partial report does not contain ${expected}:\n${partial_report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${partial_output}" brep Revolved_Ring_swept_item.s info
  OUTPUT_VARIABLE partial_brep_output
  ERROR_VARIABLE partial_brep_error
)
set(partial_brep_text "${partial_brep_output}\n${partial_brep_error}")
if(NOT partial_brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT partial_brep_text MATCHES "faces:[ ]+3" OR
   NOT partial_brep_text MATCHES "edges:[ ]+3")
  message(FATAL_ERROR "partial revolved-area BREP validation failed\n${partial_brep_output}\n${partial_brep_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${partial_output}" bb -q -e Revolved_Ring_swept_item.s
  OUTPUT_VARIABLE partial_bbox_output
  ERROR_VARIABLE partial_bbox_error
)
set(partial_bbox_text "${partial_bbox_output}\n${partial_bbox_error}")
if(NOT partial_bbox_text MATCHES "min \\{-0\\.00[0-9]+ -2\\.00[0-9]+ -7\\.00[0-9]+\\} max \\{7\\.00[0-9]+ 2\\.00[0-9]+ 0\\.00[0-9]+\\}")
  message(FATAL_ERROR "partial revolved-area bounding box validation failed\n${partial_bbox_output}\n${partial_bbox_error}")
endif()
