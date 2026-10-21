if(NOT DEFINED MGED OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "MGED and TEST_DIR are required")
endif()

function(check_mged expected)
  execute_process(
    COMMAND "${MGED}" ${ARGN}
    WORKING_DIRECTORY "${TEST_DIR}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result
    TIMEOUT 30
  )
  set(log "${output}\n${error}")
  if(NOT result EQUAL 0 OR NOT log MATCHES "${expected}" OR log MATCHES "developer option")
    message(FATAL_ERROR "MGED command option check failed (${ARGN}):\n${log}")
  endif()
endfunction()

set(database "${TEST_DIR}/mged-command-options.g")
set(dash_database "${TEST_DIR}/-mged-command-options.g")
file(REMOVE "${database}" "${dash_database}")

# Reproduce blastsectionD without depending on the example database build.
set(fixture "put blastsectionD arb8 V1 {-318 224 0} V2 {-248 298 0} V3 {-248 298 180} V4 {-318 224 180} V5 {-318 298 0} V6 {-248 298 0} V7 {-248 298 180} V8 {-318 298 180}")
check_mged("V1" -c -a nu --no-rc "${database}" "${fixture}")
check_mged("success" -c -a nu --no-rc "${database}" repair -o arbfixed.s blastsectionD)
check_mged("arb8 V1" -c -a nu --no-rc "${database}" get arbfixed.s)
check_mged("Usage: repair" -c -a nu --no-rc "${database}" repair -h)
check_mged("-c -a --help --perspective bad" -c -a nu --no-rc "${database}" list -c -a --help --perspective bad)
check_mged("--help -o" --classic --attach=nu --no-rc --bg 10 20 30 --perspective -1 "${database}" list --help -o)

configure_file("${database}" "${dash_database}" COPYONLY)
check_mged("arb8 V1" -c -anu --no-rc -- -mged-command-options.g get arbfixed.s)
file(REMOVE "${database}" "${dash_database}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
