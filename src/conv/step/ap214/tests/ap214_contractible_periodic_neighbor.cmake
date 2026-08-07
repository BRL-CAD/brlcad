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
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "AP214 contractible periodic-neighbor fixture failed "
    "(${import_result})\n${import_output}\n${import_error}")
endif()

# AP214 root #3624 has two full-period boundaries and a contractible
# one-edge hole on cylindrical face #150013.  The outer edge #81818 is shared
# by neighboring toroidal face #150014 and its STEP vertex is antipodal to the
# cylinder's private OpenNURBS seam.  A topology-driven surface seam move must
# preserve the contractible hole as an exact sub-period curve; treating every
# closed singleton as a full-period boundary rolls the transaction back and
# leaves the outer loop structurally open.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "inserted an exact OpenNURBS seam for an implicit periodic STEP face band"
    "regenerated a native-seam closed boundary as an exact full-period surface isocurve")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

string(FIND "${report_text}" "OpenNURBS structural validation failed"
  structural_failure)
if(NOT structural_failure EQUAL -1)
  message(FATAL_ERROR
    "periodic-neighbor fixture remained structurally invalid:\n${report_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep contractible_periodic_neighbor_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
    NOT brep_text MATCHES "faces:[ ]+2" OR
    NOT brep_text MATCHES "edges:[ ]+7" OR
    NOT brep_text MATCHES "loops:[ ]+3" OR
    NOT brep_text MATCHES "trims:[ ]+11")
  message(FATAL_ERROR
    "AP214 contractible periodic-neighbor BREP validation failed:\n"
    "${brep_text}")
endif()
