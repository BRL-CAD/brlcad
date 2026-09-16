if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()
file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "topology-wireframe import returned ${result}:\n${log}${error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"EDGE_BASED_WIREFRAME_SHAPE_REPRESENTATION\":1"
    "\"SHELL_BASED_WIREFRAME_SHAPE_REPRESENTATION\":1"
    "\"EDGE_BASED_WIREFRAME_MODEL\":1"
    "\"SHELL_BASED_WIREFRAME_MODEL\":1"
    "\"geometry_attempted\":2,\"geometry_written\":2,\"geometry_skipped\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "topology-wireframe report omits ${expected}:\n${report_text}")
  endif()
endforeach()
foreach(object Edge_Wire_wire.s Shell_Wire_wire.s)
  execute_process(COMMAND "${MGED}" -c "${OUTPUT}" "brep ${object} info"
    OUTPUT_VARIABLE brep_output ERROR_VARIABLE brep_error)
  set(brep_text "${brep_output}${brep_error}")
  if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
     NOT brep_text MATCHES "edges:[ ]+2" OR
     NOT brep_text MATCHES "3d curve:[ ]+2")
    message(FATAL_ERROR "${object} validation failed:\n${brep_text}")
  endif()
endforeach()
