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

set(workspace "${TEST_DIR}/mged-command-options-workspace")
set(history_script "${TEST_DIR}/mged-command-options-history.tcl")
file(REMOVE_RECURSE "${workspace}")
check_mged("mode workspace" -c -a nu --no-rc "${database}" db history workspace "${workspace}")
file(WRITE "${history_script}" "db make history.s sph\n.inmem get history.s\n")
check_mged("ell V" -c -a nu --no-rc "${workspace}" source "${history_script}")
file(WRITE "${history_script}" "db undo\n.inmem get history.s\n")
check_mged("history.s: not found" -c -a nu --no-rc "${workspace}" source "${history_script}")
file(WRITE "${history_script}" "db redo\n.inmem get history.s\n")
check_mged("ell V" -c -a nu --no-rc "${workspace}" source "${history_script}")
file(WRITE "${history_script}" "draw history.s\ndb title {Drawn history model}\ndb undo\nwho\n")
check_mged("history.s" -c -a nu --no-rc "${workspace}" source "${history_script}")
file(WRITE "${history_script}" [=[
proc exercise_history_edit {choose object destination operation} {
    set original [db get {*}$object]
    set revision [db history status]
    {*}$choose
    translate {*}$destination
    reject
    if {[db history status] ne $revision || [db get {*}$object] ne $original} {
        error "Reject changed the working revision or model"
    }

    {*}$choose
    translate {*}$destination
    accept
    set changed [db get {*}$object]
    if {$changed eq $original || ![string match "*$operation*" [db history status]]} {
        error "Accept did not record $operation"
    }
    db undo
    if {[db get {*}$object] ne $original} {
        error "Undo did not recover the original model"
    }
    db redo
    if {[db get {*}$object] ne $changed} {
        error "Redo did not recover the accepted model"
    }
}
draw history.s
exercise_history_edit {sed history.s} {history.s V} {1 2 3} {Edit history.s}
db g history.c history.s
draw history.c
exercise_history_edit {oed history.c/history.s} {history.c} {4 5 6} {Transform history.s}
puts MGED_HISTORY_EDIT_OK
]=])
check_mged("MGED_HISTORY_EDIT_OK" -c -a nu --no-rc "${workspace}" source "${history_script}")
check_mged("access read-only" -c -a nu -r --no-rc "${workspace}" db history status)
check_mged("open read-only" -c -a nu -r --no-rc "${workspace}" db undo)
file(REMOVE_RECURSE "${workspace}")

file(REMOVE "${database}" "${dash_database}" "${history_script}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
