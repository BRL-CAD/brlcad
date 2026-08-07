if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED EXACT_REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR
    "STEP_G, MGED, INPUT, REPORT, EXACT_REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${EXACT_REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --report "${REPORT}" "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "conical multi-edge periodic cap import failed (${import_result})\n"
    "${import_output}\n${import_error}")
endif()

# This is the one-face dependency closure around a multi-edge conical cap.
# Its two half-circle pcurves meet on equivalent cone images with a 0.00099 mm
# measured source residual under the file's asserted 0.01 mm uncertainty.
# Safe repair must prove the shared STEP vertex and materialize the cone-apex
# pole cut.  Exact mode may perform the same lossless pcurve/seam
# reparameterization.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "aligned a cap surface seam with its proven periodic boundary vertex"
    "inserted an exact pole cut for a multi-edge full-period boundary")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "safe report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep Conical_multi_edge_periodic_cap_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: NO, Plate mode: YES" OR
    NOT brep_text MATCHES "faces:[ ]+1" OR
    NOT brep_text MATCHES "edges:[ ]+4" OR
    NOT brep_text MATCHES "trims:[ ]+5")
  message(FATAL_ERROR
    "conical multi-edge periodic cap validation failed:\n${brep_text}")
endif()

execute_process(
  COMMAND "${STEP_G}" -D --exact --report "${EXACT_REPORT}" "${INPUT}"
  RESULT_VARIABLE exact_result
  OUTPUT_VARIABLE exact_output
  ERROR_VARIABLE exact_error
  TIMEOUT 60
)
if(NOT exact_result EQUAL 0)
  message(FATAL_ERROR
    "conical periodic cap exact import returned ${exact_result}, expected 0\n"
    "${exact_output}\n${exact_error}")
endif()
file(READ "${EXACT_REPORT}" exact_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0")
  string(FIND "${exact_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "exact report does not contain ${expected}:\n${exact_text}")
  endif()
endforeach()
# The aggregate repair counter is not the strictness contract: exact conversion
# may normalize lift-equivalent pcurves and materialize the required OpenNURBS
# pole topology.
