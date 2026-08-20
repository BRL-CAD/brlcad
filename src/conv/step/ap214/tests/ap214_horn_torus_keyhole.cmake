if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -j 1 -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)

# This owned synthetic fixture isolates the periodic branch choice seen in the
# the focused horn-torus horn-torus face.  Separating its exact opposite-use keyhole
# bridge exposes two boundaries with opposite full-period windings which meet
# at the torus's collapsed pole.  The importer must represent the native sides
# with singular trims, not a zero-length edge or a preserved-invalid BREP.
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "horn torus keyhole returned ${import_result}, expected valid status 0\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0,\"invalid_breps_written\":0,\"invalid_breps_rejected\":0"
    "\"message\":\"inserted exact singular trims at a collapsed periodic-band pole\",\"count\":1"
    "\"message\":\"removed an exact opposite-use STEP keyhole bridge after validated topology separation\",\"count\":1"
    "\"message\":\"unwrapped an exact full-period boundary from its 3-D STEP edge chain\",\"count\":2")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "horn torus report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Horn_Torus_Keyhole_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
   NOT brep_text MATCHES "Valid: YES, Solid: NO, Plate mode: YES" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+7" OR
   NOT brep_text MATCHES "vertices:[ ]+6" OR
   NOT brep_text MATCHES "loops:[ ]+1" OR
   NOT brep_text MATCHES "trims:[ ]+9")
  message(FATAL_ERROR "horn torus keyhole validation failed:\n${brep_text}")
endif()
