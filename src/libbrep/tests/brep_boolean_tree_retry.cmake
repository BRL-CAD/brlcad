# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1.

if(NOT DEFINED MGED OR NOT DEFINED BREP_CORPUS OR NOT DEFINED SOURCE_DB OR
   NOT DEFINED TEST_DB)
  message(FATAL_ERROR
    "MGED, BREP_CORPUS, SOURCE_DB, and TEST_DB must be defined")
endif()

get_filename_component(TEST_DIR "${TEST_DB}" DIRECTORY)
file(MAKE_DIRECTORY "${TEST_DIR}")
file(COPY_FILE "${SOURCE_DB}" "${TEST_DB}")

function(execute_brep_command COMMAND_TEXT)
  execute_process(
    COMMAND "${MGED}" -c "${TEST_DB}" "${COMMAND_TEXT}"
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE STDOUT
    ERROR_VARIABLE STDERR)
  if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "${COMMAND_TEXT} failed with ${RESULT}: ${STDOUT}${STDERR}")
  endif()
  set(BREP_COMMAND_OUTPUT "${STDOUT}${STDERR}" PARENT_SCOPE)
endfunction()

function(require_output COMMAND_TEXT EXPECTED_OUTPUT)
  execute_brep_command("${COMMAND_TEXT}")
  string(STRIP "${BREP_COMMAND_OUTPUT}" OUTPUT)
  if(NOT OUTPUT MATCHES "${EXPECTED_OUTPUT}")
    message(FATAL_ERROR "${COMMAND_TEXT} produced unexpected output: ${OUTPUT}")
  endif()
endfunction()

function(check_retry_solid COMBINATION OPERATION)
  set(RESULT "${COMBINATION}.brep")
  execute_brep_command("brep ${COMBINATION} brep")
  if(NOT BREP_COMMAND_OUTPUT MATCHES "Retrying Boolean operation ${OPERATION} with regularized primitive operands")
    message(FATAL_ERROR "${COMBINATION} did not exercise operation ${OPERATION} retry: ${BREP_COMMAND_OUTPUT}")
  endif()
  if(NOT BREP_COMMAND_OUTPUT MATCHES "${RESULT} is made")
    message(FATAL_ERROR "${COMBINATION} retry produced no B-Rep: ${BREP_COMMAND_OUTPUT}")
  endif()
  require_output("brep ${RESULT} valid" "brep is valid")
  require_output("brep ${RESULT} solid" "brep is solid")
endfunction()

# This pair still requires regularization after the exact Boolean path.  Keep
# the fixture tied to that behavior: older m35/r614 geometry now succeeds
# exactly and therefore no longer exercises the retry machinery.
check_retry_solid(3_r.wipe1 2)

set(TRANSLATION_MATRIX "1 0 0 100 0 1 0 0 0 0 1 0 0 0 0 1")
execute_brep_command(
  "put retry_transformed comb region yes tree {u {l 3_s.wipe1 {${TRANSLATION_MATRIX}}} {l 3_s.wipe2 {${TRANSLATION_MATRIX}}}}")
check_retry_solid(retry_transformed 2)

execute_brep_command(
  "put retry_difference comb region yes tree {- {l 3_s.wipe1} {l 3_s.wipe2}}")
check_retry_solid(retry_difference 4)

execute_brep_command(
  "put retry_intersection comb region yes tree {+ {l 3_s.wipe1} {l 3_s.wipe2}}")
execute_brep_command("brep retry_intersection brep")
if(NOT BREP_COMMAND_OUTPUT MATCHES "Retrying Boolean operation 3 with regularized primitive operands" OR
   NOT BREP_COMMAND_OUTPUT MATCHES "Boolean result is empty")
  message(FATAL_ERROR "intersection retry was not empty after retry: ${BREP_COMMAND_OUTPUT}")
endif()
require_output("exists -e retry_intersection.brep" "^0$")

execute_brep_command(
  "put retry_xor comb region yes tree {^ {l 3_s.wipe1} {l 3_s.wipe2}}")
require_output("get retry_xor tree" "^\\^")
check_retry_solid(retry_xor 5)

# A transformed XOR exercises transformed retry leaves together with both
# inner and outer regularization bounds used to construct the two differences.
execute_brep_command(
  "put retry_xor_transformed comb region yes tree {^ {l 3_s.wipe1 {${TRANSLATION_MATRIX}}} {l 3_s.wipe2 {${TRANSLATION_MATRIX}}}}")
require_output("get retry_xor_transformed tree" "^\\^")
check_retry_solid(retry_xor_transformed 5)

# A transform on a combination leaf is applied after that combination's local
# Boolean has been evaluated.  Its regularization must account for the pending
# scale: the local perturbation may be smaller, but its final model-space
# displacement must remain below the modeling tolerance.
set(SCALE_MATRIX "1.5 0 0 0 0 1.5 0 0 0 0 1.5 0 0 0 0 1")
execute_brep_command(
  "put retry_scaled_nested comb region yes tree {l 3_r.wipe1 {${SCALE_MATRIX}}}")
check_retry_solid(retry_scaled_nested 2)

execute_process(
  COMMAND "${BREP_CORPUS}"
    --database "${TEST_DB}"
    --component retry_scaled_nested
    --output "${TEST_DIR}/retry_scaled_nested.csv"
    --rays 16384
    --timeout-ms 30000
    --require-semantic-pass
    --require-fidelity approximate
  RESULT_VARIABLE SCALED_RESULT
  OUTPUT_VARIABLE SCALED_STDOUT
  ERROR_VARIABLE SCALED_STDERR)
if(NOT SCALED_RESULT EQUAL 0)
  message(FATAL_ERROR
    "model-space-bounded transformed retry failed with ${SCALED_RESULT}: "
    "${SCALED_STDOUT}${SCALED_STDERR}")
endif()
