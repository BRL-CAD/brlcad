string(RANDOM LENGTH 16 directory_suffix)
set(test_dir "${CMAKE_CURRENT_BINARY_DIR}/gversion-${directory_suffix}")
file(MAKE_DIRECTORY "${test_dir}")

function(run_version commands)
  file(WRITE "${test_dir}/commands" "${commands}")
  execute_process(
    COMMAND "${GVERSION}" ${ARGN}
    INPUT_FILE "${test_dir}/commands"
    WORKING_DIRECTORY "${test_dir}"
    OUTPUT_VARIABLE output
    RESULT_VARIABLE status
  )
  set(result "${output}" PARENT_SCOPE)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "gversion ${ARGN} failed in ${test_dir} with status ${status}")
  endif()
endfunction()

function(compare_files left right)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${left}" "${right}"
    WORKING_DIRECTORY "${test_dir}"
    COMMAND_ERROR_IS_FATAL ANY
  )
endfunction()

run_version("make extrusion.s extrude\ng assembly.c skt_0 extrusion.s\n" --session - base.g)
run_version("" --session base.g canonical.g)
run_version("mvall skt_0 outline.s\nhistory\n" base.g versioned.g)
if(NOT result MATCHES "\\* 1 0 4 mvall skt_0 outline.s")
  message(FATAL_ERROR "Expected one rename revision spanning four changes.\n${result}")
endif()

run_version("undo\n" --session versioned.g restored.g)
compare_files(canonical.g restored.g)
run_version("" --session versioned.g renamed.g)
run_version("undo\nredo\n" --session versioned.g redone.g)
compare_files(renamed.g redone.g)

run_version("undo\ntitle \"Second branch\"\nhistory\n" versioned.g branched.g)
if(NOT result MATCHES "\\* 2 0 1 title")
  message(FATAL_ERROR "Expected an independent branch containing a title edit.\n${result}")
endif()
run_version("checkout 1\n" --session branched.g previous-branch.g)
compare_files(renamed.g previous-branch.g)
run_version("checkout 0\n" --session branched.g original.g)
compare_files(canonical.g original.g)

file(WRITE "${test_dir}/commands" "title temporary\nmvall outline.s assembly.c\n")
execute_process(
  COMMAND "${GVERSION}" versioned.g failed.g
  INPUT_FILE "${test_dir}/commands"
  WORKING_DIRECTORY "${test_dir}"
  ERROR_VARIABLE error
  RESULT_VARIABLE status
)
if(status EQUAL 0 OR EXISTS "${test_dir}/failed.g" OR NOT error MATCHES "already exists")
  message(FATAL_ERROR "A failed edit published a result or did not report its conflict.\n${error}")
endif()

run_version("mvall skt_0 outline.s\n" --lmdb working base.g -)
run_version("" --session --lmdb working - lmdb-renamed.g)
compare_files(renamed.g lmdb-renamed.g)
run_version("undo\n" --session --lmdb working - lmdb-restored.g)
compare_files(canonical.g lmdb-restored.g)
run_version("redo\n" --lmdb working - lmdb-exported.g)
compare_files(versioned.g lmdb-exported.g)
run_version("" --lmdb imported branched.g -)
run_version("checkout 0\n" --session --lmdb imported - lmdb-imported.g)
compare_files(canonical.g lmdb-imported.g)

file(WRITE "${test_dir}/commands" "undo\nmvall skt_0 assembly.c\n")
execute_process(
  COMMAND "${GVERSION}" --lmdb working - -
  INPUT_FILE "${test_dir}/commands"
  WORKING_DIRECTORY "${test_dir}"
  ERROR_VARIABLE error
  RESULT_VARIABLE status
)
if(status EQUAL 0 OR NOT error MATCHES "already exists")
  message(FATAL_ERROR "The failed LMDB editing command did not report its conflict.\n${error}")
endif()
run_version("" --session --lmdb working - lmdb-after-failure.g)
compare_files(canonical.g lmdb-after-failure.g)

file(REMOVE_RECURSE "${test_dir}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
