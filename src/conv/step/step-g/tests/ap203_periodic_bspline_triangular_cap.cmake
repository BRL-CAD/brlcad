if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -o "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "periodic B-spline triangular cap import failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

# This fixture is the five-face vertex neighborhood around the three-edge
# periodic cap from a large AP203 solid.  It retains the source model's
# 0.009906 mm uncertainty: without the neighboring shared-edge uses and that
# tolerance, the one-period out-of-domain pullback regression is not exposed.
# A rejected speculative candidate must neither leak a legacy unqualified
# error nor affect the validated safe candidate.
if(import_error MATCHES "Error:")
  message(FATAL_ERROR
    "speculative periodic-cap failure leaked to the user:\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "inserted an exact singular trim at a surface pole")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep SC_loop_vertex_star_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+5" OR
   NOT brep_text MATCHES "edges:[ ]+13" OR
   NOT brep_text MATCHES "trims:[ ]+22")
  message(FATAL_ERROR
    "periodic B-spline triangular cap validation failed:\n${brep_text}")
endif()
