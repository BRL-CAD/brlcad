if(NOT DEFINED AP214_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "AP214_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${AP214_G}" -j 4 -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 1)
  message(FATAL_ERROR
    "partial parallel import returned ${import_result}, expected 1\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"requested_jobs\":4,\"effective_jobs\":4"
    "\"products\":2"
    "\"occurrences\":1"
    "\"geometry_attempted\":2"
    "\"geometry_written\":1"
    "\"geometry_skipped\":1"
    "\"performance\":{\"stages\":{"
    "\"brep_construction_pullback\":{\"calls\":2"
    "\"face_construction_pullback\":{\"calls\":7"
    "\"pullback\":{\"closest_point_queries\":0"
    "closed STEP BREP did not validate as a solid")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "parallel failure report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Parallel_Failure_item.s info
  OUTPUT_VARIABLE valid_output
  ERROR_VARIABLE valid_error
)
set(valid_text "${valid_output}\n${valid_error}")
if(NOT valid_text MATCHES "Valid: YES, Solid: YES")
  message(FATAL_ERROR "valid worker result was not retained\n${valid_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" db get Invalid_Child_item.s
  OUTPUT_VARIABLE invalid_output
  ERROR_VARIABLE invalid_error
)
set(invalid_text "${invalid_output}\n${invalid_error}")
if(NOT invalid_text MATCHES "not found")
  message(FATAL_ERROR "invalid worker result leaked into output\n${invalid_text}")
endif()

# An occurrence whose only geometry item is skipped must still resolve to a
# concrete, empty product combination.  Otherwise mk_comb accepts the forward
# reference but MGED reports db_lookup failures when the completed hierarchy is
# drawn.
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" draw Parallel_Failure
  OUTPUT_VARIABLE draw_output
  ERROR_VARIABLE draw_error
)
set(draw_text "${draw_output}\n${draw_error}")
if(draw_text MATCHES "db_lookup\\(" OR draw_text MATCHES "not found")
  message(FATAL_ERROR "partial assembly contains a dangling member\n${draw_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Invalid_Child
  OUTPUT_VARIABLE child_output
  ERROR_VARIABLE child_error
)
set(child_text "${child_output}\n${child_error}")
if(NOT child_text MATCHES "step:source_id[ ]+140" OR
   NOT child_text MATCHES "step:import_status[ ]+partial" OR
   NOT child_text MATCHES "step:skipped_geometry_count[ ]+1")
  message(FATAL_ERROR
    "skipped child product was not retained and annotated\n${child_text}")
endif()
