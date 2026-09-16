if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --schema ap203e2 --strict -O "${OUTPUT}"
    --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "strict AP203e2 swept-solid import returned ${import_result}:\n"
    "${import_output}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"strict\":true"
    "\"geometry_attempted\":3"
    "\"geometry_written\":3"
    "\"geometry_skipped\":0"
    "\"REVOLVED_AREA_SOLID\":1"
    "\"SWEPT_DISK_SOLID\":2"
    "\"outcome\":\"complete\"")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "AP203e2 report omits ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep AP203e2_Sweeps_swept_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
   NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+2" OR
   NOT brep_text MATCHES "edges:[ ]+4")
  message(FATAL_ERROR "AP203e2 revolved-area BREP is invalid:\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get AP203e2_Sweeps_swept_item_step45
  RESULT_VARIABLE disk_result
  OUTPUT_VARIABLE disk_output
  ERROR_VARIABLE disk_error
)
set(disk_text "${disk_output}\n${disk_error}")
if(NOT disk_result EQUAL 0 OR
   NOT disk_text MATCHES "AP203e2_Sweeps_swept_item_step45_outer.s" OR
   NOT disk_text MATCHES "AP203e2_Sweeps_swept_item_step45_inner.s")
  message(FATAL_ERROR "AP203e2 swept-disk Boolean is invalid:\n${disk_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep AP203e2_Sweeps_swept_item_step108.s info
  RESULT_VARIABLE circular_result
  OUTPUT_VARIABLE circular_output
  ERROR_VARIABLE circular_error
)
set(circular_text "${circular_output}\n${circular_error}")
if(NOT circular_result EQUAL 0 OR
   NOT circular_text MATCHES "Valid: YES, Solid: YES" OR
   NOT circular_text MATCHES "faces:[ ]+2" OR
   NOT circular_text MATCHES "edges:[ ]+4")
  message(FATAL_ERROR
    "AP203e2 circular swept-disk BREP is invalid:\n${circular_text}")
endif()
