if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

function(check_import report output exact)
  file(REMOVE "${report}" "${output}")
  if(exact)
    set(mode --exact)
  else()
    set(mode)
  endif()
  execute_process(
    COMMAND "${STEP_G}" ${mode} --report "${report}" "${INPUT}" "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 60
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
      "torus seam relocation import failed (${result}, exact=${exact})\n${stdout}\n${stderr}")
  endif()

  file(READ "${report}" report_text)
  foreach(expected
      "\"geometry_attempted\":1"
      "\"geometry_written\":1"
      "\"geometry_skipped\":0"
      "\"invalid_breps\":0")
    string(FIND "${report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
      "report does not contain ${expected} (exact=${exact}):\n${report_text}")
    endif()
  endforeach()

  # The horn-torus constructor now chooses its identity-proven axis-collapse
  # point as the profile seam.  That exact canonical parameterization can make
  # a later explicit seam-relocation repair unnecessary, so guard the resulting
  # geometry and source topology instead of an implementation-stage message.
  execute_process(
    COMMAND "${MGED}" -c "${output}"
      brep AP203_Torus_Seam_Relocation_item.s info
    RESULT_VARIABLE brep_result
    OUTPUT_VARIABLE brep_output
    ERROR_VARIABLE brep_error
  )
  set(brep_text "${brep_output}\n${brep_error}")
  if(NOT brep_result EQUAL 0 OR
      NOT brep_text MATCHES "Valid: YES, Solid: NO, Plate mode: YES" OR
      NOT brep_text MATCHES "faces:[ ]+1" OR
      NOT brep_text MATCHES "edges:[ ]+4" OR
      NOT brep_text MATCHES "vertices:[ ]+4" OR
      NOT brep_text MATCHES "loops:[ ]+1" OR
      NOT brep_text MATCHES "trims:[ ]+4")
    message(FATAL_ERROR
      "torus seam geometry validation failed (exact=${exact})\n${brep_text}")
  endif()
endfunction()

check_import("${REPORT}" "${OUTPUT}" FALSE)
check_import("${REPORT}.exact" "${OUTPUT}.exact.g" TRUE)
