# Verify the native MGED launcher schema without starting a graphical session.
# --help intentionally exits with status 1, matching the historical launcher.

execute_process(
  COMMAND "${MGED_EXECUTABLE}"
    --rt-debug=ff --bu-debug 10 --bg 0,0,64 --geo-color 1 2 3
    --set use_air=1 --rset "g snap 1" --command-owned-option --help
  RESULT_VARIABLE valid_result
  OUTPUT_VARIABLE valid_output
  ERROR_VARIABLE valid_error
)

if(NOT valid_result EQUAL 1)
  message(FATAL_ERROR "native MGED option parse exited with ${valid_result}\nstdout:\n${valid_output}\nstderr:\n${valid_error}")
endif()

set(valid_combined "${valid_output}\n${valid_error}")
foreach(expected
    "--rt-debug hex"
    "--geo-color R G B"
    "--set VAR=VALUE"
)
  string(FIND "${valid_combined}" "${expected}" expected_pos)
  if(expected_pos EQUAL -1)
    message(FATAL_ERROR "native MGED help is missing [${expected}]\n${valid_combined}")
  endif()
endforeach()

string(FIND "${valid_combined}" "unknown option" unknown_pos)
if(NOT unknown_pos EQUAL -1)
  message(FATAL_ERROR "command-owned dash option was rejected by MGED launcher\n${valid_combined}")
endif()

execute_process(
  COMMAND "${MGED_EXECUTABLE}" --rt-debug=not-hex --help
  RESULT_VARIABLE bad_debug_result
  OUTPUT_VARIABLE bad_debug_output
  ERROR_VARIABLE bad_debug_error
)
if(bad_debug_result EQUAL 0)
  message(FATAL_ERROR "invalid native hexadecimal debug mask unexpectedly succeeded")
endif()
set(bad_debug_combined "${bad_debug_output}\n${bad_debug_error}")
string(FIND "${bad_debug_combined}" "invalid hexadecimal integer" bad_debug_pos)
if(bad_debug_pos EQUAL -1)
  message(FATAL_ERROR "invalid debug mask lacked native diagnostic\n${bad_debug_combined}")
endif()

execute_process(
  COMMAND "${MGED_EXECUTABLE}" --set missing_equals --help
  RESULT_VARIABLE bad_set_result
  OUTPUT_VARIABLE bad_set_output
  ERROR_VARIABLE bad_set_error
)
if(bad_set_result EQUAL 0)
  message(FATAL_ERROR "malformed --set unexpectedly succeeded")
endif()
set(bad_set_combined "${bad_set_output}\n${bad_set_error}")
string(FIND "${bad_set_combined}" "requires VAR=VALUE" bad_set_pos)
if(bad_set_pos EQUAL -1)
  message(FATAL_ERROR "malformed --set lacked native diagnostic\n${bad_set_combined}")
endif()
