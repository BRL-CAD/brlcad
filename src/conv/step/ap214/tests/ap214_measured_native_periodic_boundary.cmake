if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED EXACT_REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR
    "STEP_G, MGED, INPUT, REPORT, EXACT_REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "AP214 measured native-periodic fixture failed "
    "(${import_result})\n${import_output}\n${import_error}")
endif()

# AP214 root #3638 contains toroidal face #153817.  Its full-period
# circle and supporting torus disagree by about 1.6e-6 mm although the file
# declares 1e-6 mm uncertainty.  Safe mode may retain both exact source
# geometries with a densely measured local OpenNURBS tolerance; it must not
# move the edge or surface, bridge a gap, or apply the allowance in --exact.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "adjusted one native periodic boundary tolerance to measured source geometry"
    "source native periodic boundary exceeded the declared tolerance")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep measured_native_periodic_boundary_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO")
  message(FATAL_ERROR
    "AP214 measured native-periodic BREP validation failed:\n"
    "${brep_text}")
endif()

execute_process(
  COMMAND "${STEP_G}" -D --exact --reject-invalid-objs
    --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
  TIMEOUT 60
)
if(NOT exact_result EQUAL 3)
  message(FATAL_ERROR
    "AP214 measured native-periodic exact import returned "
    "${exact_result}, expected 3\n${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "OpenNURBS structural validation failed")
  string(FIND "${exact_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "exact report does not contain ${expected}:\n${exact_text}")
  endif()
endforeach()
foreach(forbidden
    "adjusted one native periodic boundary tolerance to measured source geometry"
    "source native periodic boundary exceeded the declared tolerance")
  string(FIND "${exact_text}" "${forbidden}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR
      "exact import applied the safe measured-tolerance repair:\n${exact_text}")
  endif()
endforeach()
