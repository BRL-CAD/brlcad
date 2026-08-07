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
    "AP214 periodic endpoint-branch fixture failed "
    "(${import_result})\n${import_output}\n${import_error}")
endif()

# AP214 root #3639 has two full-period boundaries on cylindrical face
# #154314.  Relocating the surface seam to outer STEP edge #91030 places the
# final numerical sample of inner edge #91029 infinitesimally beyond the
# candidate native domain even though its immutable 3-D endpoint and STEP
# topology vertex both lie on the requested seam.  Only that proven endpoint
# may be normalized; accepting an interior sample would conceal a real seam
# crossing and corrupt the boundary.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "inserted an exact OpenNURBS seam for an implicit periodic STEP face band"
    "regenerated an adjacent split boundary as an exact full-period surface isocurve")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

string(FIND "${report_text}" "OpenNURBS structural validation failed"
  structural_failure)
if(NOT structural_failure EQUAL -1)
  message(FATAL_ERROR
    "periodic endpoint-branch fixture remained structurally invalid:\n"
    "${report_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep periodic_endpoint_branch_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
    NOT brep_text MATCHES "faces:[ ]+3" OR
    NOT brep_text MATCHES "edges:[ ]+10" OR
    NOT brep_text MATCHES "loops:[ ]+3" OR
    NOT brep_text MATCHES "trims:[ ]+17")
  message(FATAL_ERROR
    "AP214 periodic endpoint-branch BREP validation failed:\n"
    "${brep_text}")
endif()
