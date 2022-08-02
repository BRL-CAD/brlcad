# run_ctest.cmake
# `ctest -S run_ctest.cmake`

set(CTEST_SOURCE_DIRECTORY .)
set(CTEST_BINARY_DIRECTORY build_matrix)
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_MEMORYCHECK_COMMAND /usr/bin/valgrind)
set(CTEST_INITIAL_CACHE "
SITE:STRING=${CTEST_SITE}
BUILDNAME:STRING=${CTEST_BUILD_NAME}
SC_ENABLE_TESTING:BOOL=ON
SC_BUILD_TYPE:STRING=Debug
")


ctest_start(matrix)
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
ctest_configure(BUILD "${CTEST_BINARY_DIRECTORY}" OPTIONS -DSC_ENABLE_TESTING=ON)
ctest_build(BUILD "${CTEST_BINARY_DIRECTORY}")
message("running tests")
ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}" INCLUDE_LABEL "cpp_schema_....*")

message("running python script")
execute_process(COMMAND python ../misc/wiki-scripts/update-matrix.py
                 WORKING_DIRECTORY ${CTEST_BINARY_DIRECTORY})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

