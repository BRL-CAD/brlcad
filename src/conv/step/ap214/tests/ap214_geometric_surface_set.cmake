if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
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
  message(FATAL_ERROR
    "step-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION\":1"
    "\"GEOMETRIC_SET\":1"
    "\"CURVE_BOUNDED_SURFACE\":1"
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "bridged a composite boundary join within the declared model tolerance")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Surface_Set_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "loops:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+1")
  message(FATAL_ERROR "bounded surface BREP validation failed\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Surface_Set_item.s
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+70")
  message(FATAL_ERROR "bounded surface metadata validation failed\n${attr_text}")
endif()

# The authored 0.5 micrometre join is inside the declared model uncertainty,
# but still requires safe repair.  The no-repair policy must reject the set
# transactionally and report each failed item only once even though document
# traversal can encounter the same representation through multiple paths.
set(no_repair_report "${REPORT}.no-repair")
file(REMOVE "${no_repair_report}")
execute_process(
  COMMAND "${STEP_G}" -D --repair none --report "${no_repair_report}" "${INPUT}"
  RESULT_VARIABLE no_repair_result
  OUTPUT_VARIABLE no_repair_output
  ERROR_VARIABLE no_repair_error
)
if(no_repair_result EQUAL 0)
  message(FATAL_ERROR
    "no-repair conversion unexpectedly succeeded\n"
    "${no_repair_output}\n${no_repair_error}")
endif()
file(READ "${no_repair_report}" no_repair_text)
foreach(expected
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "\"invalid_breps\":0"
    "a boundary segment join required safe tolerance repair"
    "\"entity_id\":70,\"entity_type\":\"GEOMETRIC_SET\"")
  string(FIND "${no_repair_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "no-repair report does not contain ${expected}:\n${no_repair_text}")
  endif()
endforeach()
string(REGEX MATCHALL
  "a boundary segment join required safe tolerance repair"
  duplicate_reasons "${no_repair_text}")
list(LENGTH duplicate_reasons duplicate_reason_count)
if(NOT duplicate_reason_count EQUAL 1)
  message(FATAL_ERROR
    "no-repair item was reported ${duplicate_reason_count} times")
endif()
