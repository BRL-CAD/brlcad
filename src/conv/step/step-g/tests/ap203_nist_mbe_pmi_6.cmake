if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED RT OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, RT, INPUT, REPORT, and OUTPUT are required")
endif()

set(ray_output_file "${OUTPUT}.nist6-cap.pix")
file(REMOVE "${REPORT}" "${OUTPUT}" "${ray_output_file}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "NIST MBE PMI 6 import failed (${import_result})\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"message\":\"selected the exact singular-pole branch matching the STEP face bound\",\"count\":3")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Document_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
    NOT brep_text MATCHES "faces:[ ]+152" OR
    NOT brep_text MATCHES "edges:[ ]+456" OR
    NOT brep_text MATCHES "vertices:[ ]+324")
  message(FATAL_ERROR "NIST MBE PMI 6 BREP validation failed\n${brep_text}")
endif()

# This deterministic ray enters the lower spherical cap which used to select
# the complementary half of a pole-bounded periodic surface.  The BREP could
# still report structurally valid and solid in that state, but rt had to flip
# the back-facing entry normal and the cap shaded as disconnected.
execute_process(
  COMMAND "${RT}" -P 1 -B -s 300 -b "161 66" -a 35 -e 25
    -o "${ray_output_file}" "${OUTPUT}" Document
  RESULT_VARIABLE ray_result
  OUTPUT_VARIABLE ray_output
  ERROR_VARIABLE ray_error
  TIMEOUT 30
)
set(ray_text "${ray_output}\n${ray_error}")
if(NOT ray_result EQUAL 0 OR
    NOT ray_text MATCHES "1 solid/ray intersections: 1 hits \\+ 0 miss" OR
    ray_text MATCHES "flip N")
  message(FATAL_ERROR
    "NIST MBE PMI 6 spherical-cap ray validation failed\n${ray_text}")
endif()
file(REMOVE "${ray_output_file}")
