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
    "AP214 doubly-periodic fixture failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

# AP214 root #3682 contains this toroidal inner boundary.  Four supplied
# edges lift to one exact topology chain, but its final pcurve join is one major
# period apart.  Final orientation/seam work must not undo the coherent branch
# selected from the detached STEP edges.
file(READ "${REPORT}" report_text)
foreach(expected
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
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep ap214_doubly_periodic_chain_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO")
  message(FATAL_ERROR
    "AP214 doubly-periodic BREP validation failed:\n${brep_text}")
endif()

# Both periodic endpoint images miss the asserted STEP vertex by 0.0000161 mm
# while the file declares 0.000001 mm uncertainty.  Safe mode may retain the
# densely proven exact locus with a local OpenNURBS tolerance warning.  Exact
# mode must preserve and tag that source contradiction rather than applying
# the measured tolerance adjustment.  Omission is reserved for --strict or
# --reject-invalid-objs.
execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
  TIMEOUT 60
)
if(NOT exact_result EQUAL 1)
  message(FATAL_ERROR
    "AP214 exact import returned ${exact_result}, expected 1\n"
    "${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":1"
    "\"invalid_breps_written\":1")
  string(FIND "${exact_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "exact report does not contain ${expected}:\n${exact_text}")
  endif()
endforeach()
foreach(forbidden
    "adjusted one periodic boundary tolerance to measured source geometry"
    "source periodic edge/surface endpoint exceeded the declared tolerance")
  string(FIND "${exact_text}" "${forbidden}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR
      "exact import applied a safe measured-tolerance repair:\n${exact_text}")
  endif()
endforeach()
