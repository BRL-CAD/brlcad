# lcov.cmake
# `ctest -S lcov.cmake`

if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  message(FATAL_ERROR "LCOV is Linux-only")
endif(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

set(CTEST_SOURCE_DIRECTORY .)
set(CTEST_BINARY_DIRECTORY build_lcov)
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_MEMORYCHECK_COMMAND /usr/bin/valgrind)
set(CTEST_INITIAL_CACHE "
SITE:STRING=${CTEST_SITE}
BUILDNAME:STRING=${CTEST_BUILD_NAME}
SC_ENABLE_TESTING:BOOL=ON
SC_ENABLE_COVERAGE:BOOL=ON
SC_BUILD_SCHEMAS:STRING=ALL
SC_BUILD_TYPE:STRING=Debug
")

set(LCOV_OUT "${CTEST_BINARY_DIRECTORY}/lcov_html")

ctest_start(lcov)
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
message("configuring...")
ctest_configure(BUILD "${CTEST_BINARY_DIRECTORY}" OPTIONS "-DSC_BUILD_SCHEMAS=ALL;-DSC_ENABLE_COVERAGE=ON;-SC_PYTHON_GENERATOR=OFF")
message("lcov: resetting counters...")
execute_process(COMMAND lcov -z -d .
  WORKING_DIRECTORY ${CTEST_BINARY_DIRECTORY} OUTPUT_QUIET)

# copy files so lcov won't complain
execute_process(COMMAND ${CMAKE_COMMAND} -E copy src/express/expparse.y ${CTEST_BINARY_DIRECTORY}/src/express/CMakeFiles/express.dir)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy src/express/expscan.l ${CTEST_BINARY_DIRECTORY}/src/express/CMakeFiles/express.dir)

message("building...")
ctest_build(BUILD "${CTEST_BINARY_DIRECTORY}")

message("running tests...")
ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}" PARALLEL_LEVEL 1)

message("analyzing profiling data using lcov...")
execute_process(COMMAND lcov -c -d . -o stepcode.lcov
  WORKING_DIRECTORY ${CTEST_BINARY_DIRECTORY} OUTPUT_QUIET ERROR_QUIET)

message("removing system headers...")
execute_process(COMMAND lcov -r stepcode.lcov "/usr/include/*" -o stepcode_no_usr.lcov
  WORKING_DIRECTORY ${CTEST_BINARY_DIRECTORY} OUTPUT_QUIET ERROR_QUIET)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${LCOV_OUT})

message("creating html files...")
execute_process(COMMAND genhtml ../stepcode_no_usr.lcov
  WORKING_DIRECTORY ${LCOV_OUT} OUTPUT_QUIET ERROR_QUIET)

message("html files are located in ${LCOV_OUT}")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar czf ${LCOV_OUT}.tgz ${LCOV_OUT} WORKING_DIRECTORY ${CTEST_SOURCE_DIRECTORY})

message("tarball at ${LCOV_OUT}.tgz")

message("================================================ Success! ================================================")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

