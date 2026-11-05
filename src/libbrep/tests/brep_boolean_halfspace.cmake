if(NOT DEFINED MGED OR NOT DEFINED SOURCE_DB OR NOT DEFINED TEST_DB)
  message(FATAL_ERROR "MGED, SOURCE_DB, and TEST_DB must be defined")
endif()

get_filename_component(TEST_DIR "${TEST_DB}" DIRECTORY)
file(MAKE_DIRECTORY "${TEST_DIR}")
file(COPY_FILE "${SOURCE_DB}" "${TEST_DB}")

set(BOX_MINIMUM -10)
set(BOX_MAXIMUM 10)
set(BELOW_BOX_MAXIMUM -1)
set(HALFSPACE_TRANSLATION 5)
set(OBLIQUE_HALFSPACE_NORMAL 0.5773502691896258)
set(TRANSLATED_HALFSPACE_MATRIX
  "1 0 0 0 0 1 0 0 0 0 1 ${HALFSPACE_TRANSLATION} 0 0 0 1")

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

function(run_brep_command COMMAND_TEXT EXPECTED_OUTPUT)
  execute_brep_command("${COMMAND_TEXT}")

  string(STRIP "${BREP_COMMAND_OUTPUT}" OUTPUT)
  if(NOT OUTPUT MATCHES "${EXPECTED_OUTPUT}")
    message(FATAL_ERROR "${COMMAND_TEXT} produced unexpected output: ${OUTPUT}")
  endif()
endfunction()

function(check_brep COMBINATION)
  set(RESULT "${COMBINATION}.brep")
  run_brep_command("brep ${COMBINATION} brep" "is made")
  run_brep_command("brep ${RESULT} valid" "brep is valid")
  run_brep_command("brep ${RESULT} solid" "brep is solid")
endfunction()

execute_brep_command(
  "in bounded_halfspace_box rpp ${BOX_MINIMUM} ${BOX_MAXIMUM} ${BOX_MINIMUM} ${BOX_MAXIMUM} ${BOX_MINIMUM} ${BOX_MAXIMUM}")
execute_brep_command(
  "in bounded_halfspace_below rpp ${BOX_MINIMUM} ${BOX_MAXIMUM} ${BOX_MINIMUM} ${BOX_MAXIMUM} ${BOX_MINIMUM} ${BELOW_BOX_MAXIMUM}")
execute_brep_command("in bounded_halfspace_axis half 0 0 1 0")
execute_brep_command(
  "in bounded_halfspace_oblique half ${OBLIQUE_HALFSPACE_NORMAL} ${OBLIQUE_HALFSPACE_NORMAL} ${OBLIQUE_HALFSPACE_NORMAL} 0")

# A halfspace has no finite closed B-Rep by itself.  Direct conversion must
# report that limitation without leaving a misleading result object behind.
execute_brep_command(
  "put bounded_halfspace_direct comb region yes tree {l bounded_halfspace_axis}")
execute_brep_command("brep bounded_halfspace_direct brep")
if(NOT BREP_COMMAND_OUTPUT MATCHES "unsupported Boolean input or operation")
  message(FATAL_ERROR
    "direct halfspace did not report unsupported conversion: ${BREP_COMMAND_OUTPUT}")
endif()
run_brep_command("exists -e bounded_halfspace_direct.brep" "^0$")

execute_brep_command(
  "r bounded_halfspace_difference u bounded_halfspace_box - bounded_halfspace_axis")
check_brep(bounded_halfspace_difference)

execute_brep_command(
  "r bounded_halfspace_intersection u bounded_halfspace_box + bounded_halfspace_axis")
check_brep(bounded_halfspace_intersection)

# Intersection is commutative, so a leading halfspace must defer until the
# finite operand has supplied proxy bounds.
execute_brep_command(
  "put bounded_halfspace_reversed comb region yes tree {+ {l bounded_halfspace_axis} {l bounded_halfspace_box}}")
check_brep(bounded_halfspace_reversed)

# Propagate the finite context through a left-nested chain of halfspaces.  This
# cannot be handled by merely swapping the immediate leaf operands.
execute_brep_command(
  "put bounded_halfspace_nested comb region yes tree {+ {+ {l bounded_halfspace_axis} {l bounded_halfspace_oblique}} {l bounded_halfspace_box}}")
check_brep(bounded_halfspace_nested)

execute_brep_command(
  "r bounded_halfspace_empty u bounded_halfspace_below - bounded_halfspace_axis")
run_brep_command("brep bounded_halfspace_empty brep" "Boolean result is empty")
run_brep_command("exists -e bounded_halfspace_empty.brep" "^0$")

execute_brep_command(
  "r bounded_halfspace_oblique_difference u bounded_halfspace_box - bounded_halfspace_oblique")
check_brep(bounded_halfspace_oblique_difference)

execute_brep_command(
  "put bounded_halfspace_transformed comb region yes tree {- {l bounded_halfspace_box} {l bounded_halfspace_axis {${TRANSLATED_HALFSPACE_MATRIX}}}}")
check_brep(bounded_halfspace_transformed)

execute_brep_command(
  "put bounded_halfspace_reversed_transformed comb region yes tree {+ {l bounded_halfspace_axis {${TRANSLATED_HALFSPACE_MATRIX}}} {l bounded_halfspace_box}}")
check_brep(bounded_halfspace_reversed_transformed)
