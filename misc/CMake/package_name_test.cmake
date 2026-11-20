foreach(_required IN ITEMS TEST_ROOT MODULE_FILE OPTIONS_FILE PROJECT_SOURCE_DIR GIT_EXECUTABLE)
  if(NOT DEFINED ${_required})
    message(FATAL_ERROR "Missing test argument: ${_required}")
  endif()
endforeach()

function(run)
  execute_process(
    COMMAND ${ARGV}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Test command failed: ${ARGV}\n${_output}\n${_error}")
  endif()
endfunction()

function(assert_suffix expected source_dir description)
  brlcad_binary_package_suffix(_actual "${source_dir}")
  if(NOT _actual STREQUAL expected)
    message(FATAL_ERROR "${description}: expected '${expected}', got '${_actual}'")
  endif()
endfunction()

include("${MODULE_FILE}")
file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/archive" "${TEST_ROOT}/checkout")

assert_suffix("" "${TEST_ROOT}/archive" "non-Git source tree")

run("${GIT_EXECUTABLE}" init "${TEST_ROOT}/checkout")
file(WRITE "${TEST_ROOT}/checkout/tracked.txt" "package name test\n")
run("${GIT_EXECUTABLE}" -C "${TEST_ROOT}/checkout" add tracked.txt)
run(
  "${CMAKE_COMMAND}" -E env
  GIT_AUTHOR_NAME=BRL-CAD
  GIT_AUTHOR_EMAIL=brlcad@example.invalid
  GIT_COMMITTER_NAME=BRL-CAD
  GIT_COMMITTER_EMAIL=brlcad@example.invalid
  "${GIT_EXECUTABLE}" -C "${TEST_ROOT}/checkout"
  -c commit.gpgSign=false commit -m initial
)
execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${TEST_ROOT}/checkout" rev-parse --short HEAD
  RESULT_VARIABLE _revision_result
  OUTPUT_VARIABLE _revision
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _revision_result EQUAL 0)
  message(FATAL_ERROR "Unable to determine test repository revision")
endif()

assert_suffix("_${_revision}" "${TEST_ROOT}/checkout" "untagged checkout")
run("${GIT_EXECUTABLE}" -C "${TEST_ROOT}/checkout" tag test-tag)
assert_suffix("_${_revision}" "${TEST_ROOT}/checkout" "non-release tag")
run("${GIT_EXECUTABLE}" -C "${TEST_ROOT}/checkout" tag rel-test)
assert_suffix("" "${TEST_ROOT}/checkout" "release tag")
assert_suffix("" "${TEST_ROOT}/checkout/.git" "non-root Git directory")

set(_working_git_executable "${GIT_EXECUTABLE}")
set(GIT_EXECUTABLE "${CMAKE_COMMAND}")
assert_suffix("" "${TEST_ROOT}/checkout" "failed Git query")
set(GIT_EXECUTABLE "${_working_git_executable}")

brlcad_binary_package_suffix(_project_suffix "${PROJECT_SOURCE_DIR}")
set(CPACK_PACKAGE_FILE_NAME test-binary)
unset(CPACK_INSTALLED_DIRECTORIES)
include("${OPTIONS_FILE}")
if(NOT CPACK_PACKAGE_FILE_NAME STREQUAL "test-binary${_project_suffix}")
  message(FATAL_ERROR "CPack binary name did not receive the checkout suffix")
endif()

set(CPACK_PACKAGE_FILE_NAME test-source)
set(CPACK_INSTALLED_DIRECTORIES "${TEST_ROOT}/archive;/")
include("${OPTIONS_FILE}")
if(NOT CPACK_PACKAGE_FILE_NAME STREQUAL "test-source")
  message(FATAL_ERROR "CPack source name received a checkout suffix")
endif()
