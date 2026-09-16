if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED FACETED_INPUT OR
   NOT DEFINED MAPPED_INPUT OR NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR "STEP_G, MGED, both inputs, and OUTPUT_DIR are required")
endif()
set(faceted_output "${OUTPUT_DIR}/ap203_faceted_tetra.g")
set(faceted_report "${OUTPUT_DIR}/ap203_faceted_tetra.json")
set(mapped_output "${OUTPUT_DIR}/ap203_mapped_assembly.g")
set(mapped_report "${OUTPUT_DIR}/ap203_mapped_assembly.json")
file(REMOVE "${faceted_output}" "${faceted_report}" "${mapped_output}" "${mapped_report}")

execute_process(COMMAND "${STEP_G}" -O "${faceted_output}" --report "${faceted_report}" "${FACETED_INPUT}"
  RESULT_VARIABLE faceted_result OUTPUT_VARIABLE faceted_log ERROR_VARIABLE faceted_error)
if(NOT faceted_result EQUAL 0)
  message(FATAL_ERROR "AP203 faceted import returned ${faceted_result}:\n${faceted_log}${faceted_error}")
endif()
file(READ "${faceted_report}" faceted_text)
foreach(expected
    "\"FACETED_BREP\":1"
    "\"POLY_LOOP\":4"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0")
  string(FIND "${faceted_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "faceted report omits ${expected}:\n${faceted_text}")
  endif()
endforeach()
execute_process(COMMAND "${MGED}" -c "${faceted_output}" "get tetra_item.s"
  OUTPUT_VARIABLE bot_text ERROR_VARIABLE bot_error)
execute_process(COMMAND "${MGED}" -c "${faceted_output}" "bot check solid tetra_item.s"
  OUTPUT_VARIABLE solid_text ERROR_VARIABLE solid_error)
execute_process(COMMAND "${MGED}" -c "${faceted_output}" "bot check manifold tetra_item.s"
  OUTPUT_VARIABLE manifold_text ERROR_VARIABLE manifold_error)
string(STRIP "${solid_text}${solid_error}" solid_result)
string(STRIP "${manifold_text}${manifold_error}" manifold_result)
set(bot_result "${bot_text}${bot_error}")
if(NOT bot_result MATCHES "bot mode volume" OR
   NOT solid_result STREQUAL "1" OR NOT manifold_result STREQUAL "1")
  message(FATAL_ERROR "faceted tetra BoT validation failed (solid='${solid_result}', manifold='${manifold_result}'):\n${bot_text}${bot_error}${solid_text}${solid_error}${manifold_text}${manifold_error}")
endif()

execute_process(COMMAND "${STEP_G}" -O "${mapped_output}" --report "${mapped_report}" "${MAPPED_INPUT}"
  RESULT_VARIABLE mapped_result OUTPUT_VARIABLE mapped_log ERROR_VARIABLE mapped_error)
if(NOT mapped_result EQUAL 0)
  message(FATAL_ERROR "AP203 mapped import returned ${mapped_result}:\n${mapped_log}${mapped_error}")
endif()
file(READ "${mapped_report}" mapped_text)
if(NOT mapped_text MATCHES "\"products\":2" OR
   NOT mapped_text MATCHES "\"occurrences\":3" OR
   NOT mapped_text MATCHES "\"shape_method\":\"mapped\"" OR
   NOT mapped_text MATCHES "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0" OR
   NOT mapped_text MATCHES "\"invalid_breps\":0")
  message(FATAL_ERROR "mapped report is wrong:\n${mapped_text}")
endif()
execute_process(COMMAND "${MGED}" -c "${mapped_output}" "bb -q Mapped_Assembly"
  OUTPUT_VARIABLE bbox_text ERROR_VARIABLE bbox_error)
foreach(expected "X Length: 40 mm" "Y Length: 50 mm" "Z Length: 10 mm")
  string(FIND "${bbox_text}${bbox_error}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "mapped assembly bbox omits ${expected}:\n${bbox_text}${bbox_error}")
  endif()
endforeach()
