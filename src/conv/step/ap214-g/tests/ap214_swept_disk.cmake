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
    "\"SWEPT_DISK_SOLID\":1")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get Hollow_Sweep_swept_item_outer.s
  OUTPUT_VARIABLE outer_output
  ERROR_VARIABLE outer_error
)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get Hollow_Sweep_swept_item_inner.s
  OUTPUT_VARIABLE inner_output
  ERROR_VARIABLE inner_error
)
set(primitive_text "${outer_output}\n${outer_error}\n${inner_output}\n${inner_error}")
string(REGEX MATCHALL "tgc V" cylinder_matches "${primitive_text}")
list(LENGTH cylinder_matches cylinder_count)
if(NOT cylinder_count EQUAL 2)
  message(FATAL_ERROR "swept-disk cylinders are not native exact primitives\n${primitive_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get Hollow_Sweep_swept_item
  OUTPUT_VARIABLE region_output
  ERROR_VARIABLE region_error
)
set(region_text "${region_output}\n${region_error}")
if(NOT region_text MATCHES "comb region yes" OR
   NOT region_text MATCHES "Hollow_Sweep_swept_item_outer.s" OR
   NOT region_text MATCHES "Hollow_Sweep_swept_item_inner.s" OR
   NOT region_text MATCHES "rgb \\{230 179 26\\}")
  message(FATAL_ERROR "swept-disk Boolean region validation failed\n${region_output}\n${region_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Hollow_Sweep_swept_item
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+6" OR
   NOT attr_text MATCHES "step:color_rgb[ ]+0.9 0.7 0.1")
  message(FATAL_ERROR "swept-disk metadata validation failed\n${attr_output}\n${attr_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" bb -q -e Hollow_Sweep_swept_item
  OUTPUT_VARIABLE bbox_output
  ERROR_VARIABLE bbox_error
)
set(bbox_text "${bbox_output}\n${bbox_error}")
if(NOT bbox_text MATCHES "min \\{-3\\.0+ -3\\.0+ 0\\.0+\\} max \\{3\\.0+ 3\\.0+ 20\\.0+\\}")
  message(FATAL_ERROR "swept-disk bounding box validation failed\n${bbox_output}\n${bbox_error}")
endif()

execute_process(
  COMMAND "${NIRT}" -H 0 -b -f csv -e "xyz -5 0 10; dir 1 0 0; s; q" "${OUTPUT}" Hollow_Sweep_swept_item
  RESULT_VARIABLE ray_result
  OUTPUT_VARIABLE ray_output
  ERROR_VARIABLE ray_error
)
set(ray_text "${ray_output}\n${ray_error}")
if(NOT ray_result EQUAL 0 OR
   NOT ray_text MATCHES ",-3\\.000000,0\\.000000,10\\.000000,3\\.000000,-1\\.000000" OR
   NOT ray_text MATCHES ",1\\.000000,0\\.000000,10\\.000000,-1\\.000000,3\\.000000")
  message(FATAL_ERROR "swept-disk ray smoke test failed\n${ray_output}\n${ray_error}")
endif()
