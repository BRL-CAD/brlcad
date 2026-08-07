if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED MALFORMED_INPUT OR NOT DEFINED MULTI_FACE_INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR
    "STEP_G, MGED, INPUT, malformed inputs, REPORT, and OUTPUT are required")
endif()

set(exact_report "${REPORT}.exact.json")
set(none_report "${REPORT}.none.json")
set(multi_report "${REPORT}.multiple.json")
file(REMOVE "${MALFORMED_INPUT}" "${MULTI_FACE_INPUT}" "${REPORT}"
  "${exact_report}" "${none_report}" "${multi_report}" "${OUTPUT}")

# Omit one face from a tetrahedral MANIFOLD_SOLID_BREP while retaining all of
# its authored face, edge, and vertex instances.  The shell's three one-use
# edges form the uniquely determined planar boundary of the missing face.
file(READ "${INPUT}" source_text)
string(REPLACE
  "#90=CLOSED_SHELL('',(#48,#58,#68,#79));"
  "#90=CLOSED_SHELL('',(#58,#68,#79));"
  malformed_text "${source_text}")
file(WRITE "${MALFORMED_INPUT}" "${malformed_text}")

execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}"
    "${MALFORMED_INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "permissive missing-cap import returned ${import_result}:\n"
    "${import_output}${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"inferred_curves\":3"
    "missing_planar_triangular_cap"
    "synthesized one missing planar triangular cap")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive missing-cap report lacks ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Direct_Tetra_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+4")
  message(FATAL_ERROR
    "missing-cap inference did not produce a valid tetrahedron:\n${brep_text}")
endif()

foreach(object Direct_Tetra_item.s Direct_Tetra_item)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" attr show "${object}"
    OUTPUT_VARIABLE attr_output
    ERROR_VARIABLE attr_error
  )
  set(attr_text "${attr_output}\n${attr_error}")
  foreach(expected
      "step:geometry_status[ ]+inferred"
      "step:inferred_curve_ids[ ]+30 31 33"
      "30=missing_planar_triangular_cap"
      "31=missing_planar_triangular_cap"
      "33=missing_planar_triangular_cap"
      "complete one-use STEP edge cycle")
    if(NOT attr_text MATCHES "${expected}")
      message(FATAL_ERROR
        "${object} cap-inference provenance lacks ${expected}:\n${attr_text}")
    endif()
  endforeach()
endforeach()

function(expect_invalid_preserved policy_name expected_status)
  set(policy_report "${REPORT}.${policy_name}.json")
  file(REMOVE "${policy_report}")
  execute_process(
    COMMAND "${STEP_G}" -D ${ARGN} --report "${policy_report}"
      "${MALFORMED_INPUT}"
    RESULT_VARIABLE policy_result
    OUTPUT_VARIABLE policy_output
    ERROR_VARIABLE policy_error
  )
  if(NOT policy_result EQUAL expected_status)
    message(FATAL_ERROR
      "${policy_name} returned ${policy_result}, expected ${expected_status}:\n"
      "${policy_output}${policy_error}")
  endif()
  file(READ "${policy_report}" policy_text)
  foreach(expected
      "\"geometry_written\":1"
      "\"geometry_skipped\":0"
      "\"invalid_breps\":1"
      "\"invalid_breps_written\":1"
      "\"inferred_curves\":0")
    string(FIND "${policy_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "${policy_name} report lacks ${expected}:\n${policy_text}")
    endif()
  endforeach()
endfunction()

expect_invalid_preserved(exact 1 --exact)
expect_invalid_preserved(none 1 --repair none)

# Omitting two tetrahedron faces leaves a four-edge opening.  It is not a
# uniquely determined triangular cap, so permissive mode must preserve the
# invalid source without applying this inference.
string(REPLACE
  "#90=CLOSED_SHELL('',(#48,#58,#68,#79));"
  "#90=CLOSED_SHELL('',(#68,#79));"
  multi_face_text "${source_text}")
file(WRITE "${MULTI_FACE_INPUT}" "${multi_face_text}")
execute_process(
  COMMAND "${STEP_G}" -D --report "${multi_report}" "${MULTI_FACE_INPUT}"
  RESULT_VARIABLE multi_result
  OUTPUT_VARIABLE multi_output
  ERROR_VARIABLE multi_error
)
if(NOT multi_result EQUAL 1)
  message(FATAL_ERROR
    "multi-face opening returned ${multi_result}, expected preserved invalid:\n"
    "${multi_output}${multi_error}")
endif()
file(READ "${multi_report}" multi_text)
if(NOT multi_text MATCHES "\"invalid_breps\":1" OR
   NOT multi_text MATCHES "\"inferred_curves\":0" OR
   multi_text MATCHES "missing_planar_triangular_cap")
  message(FATAL_ERROR
    "non-triangular opening was incorrectly inferred:\n${multi_text}")
endif()
