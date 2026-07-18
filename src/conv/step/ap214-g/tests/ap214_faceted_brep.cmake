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
    "\"FACETED_BREP\":1"
    "\"FACETED_BREP_SHAPE_REPRESENTATION\":1"
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
  COMMAND "${MGED}" -c "${OUTPUT}" get Faceted_Tetra_item.s
  OUTPUT_VARIABLE bot_output
  ERROR_VARIABLE bot_error
)
set(bot_text "${bot_output}\n${bot_error}")
if(NOT bot_text MATCHES "bot mode volume orient rh" OR
   NOT bot_text MATCHES "V [{]" OR
   NOT bot_text MATCHES "F [{]")
  message(FATAL_ERROR "faceted BREP was not emitted as the expected tetrahedral solid BoT\n${bot_text}")
endif()

foreach(check solid manifold)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" bot check "${check}" Faceted_Tetra_item.s
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
  )
  set(check_text "${check_output}\n${check_error}")
  if(NOT check_text MATCHES "(^|[\r\n])1([\r\n]|$)")
    message(FATAL_ERROR "faceted BoT failed ${check} check\n${check_text}")
  endif()
endforeach()

foreach(check open_edges flipped_edges)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" bot check "${check}" Faceted_Tetra_item.s
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
  )
  set(check_text "${check_output}\n${check_error}")
  if(NOT check_text MATCHES "(^|[\r\n])0([\r\n]|$)")
    message(FATAL_ERROR "faceted BoT has ${check}\n${check_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show Faceted_Tetra_item
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
if(NOT attr_text MATCHES "step:source_id[ ]+91" OR
   NOT attr_text MATCHES "step:representation[ ]+FACETED_BREP" OR
   NOT attr_text MATCHES "step:color_rgb[ ]+0.9 0.4 0.1" OR
   NOT attr_text MATCHES "step:layers[ ]+faceted geometry")
  message(FATAL_ERROR "faceted BREP metadata validation failed\n${attr_text}")
endif()
