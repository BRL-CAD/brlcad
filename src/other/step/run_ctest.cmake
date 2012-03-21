# run_ctest.cmake
# `ctest -S run_ctest.cmake`

set( CTEST_SOURCE_DIRECTORY . )
set( CTEST_BINARY_DIRECTORY build_ctest )
set( CTEST_CMAKE_GENERATOR "Unix Makefiles" )
set( CTEST_MEMORYCHECK_COMMAND /usr/bin/valgrind )
set( CTEST_INITIAL_CACHE "
SITE:STRING=${CTEST_SITE}
BUILDNAME:STRING=${CTEST_BUILD_NAME}
ENABLE_TESTING:BOOL=ON
CMAKE_BUILD_TYPE:STRING=Debug
")



if( EXISTS "../.SCL_CTEST_PREFS.cmake" )
    include( "../.SCL_CTEST_PREFS.cmake" )
else()
    message( WARNING "Did not find ../.SCL_CTEST_PREFS.cmake, containing config variables - result submission disabled" )
    set( SKIP_SUBMISSION TRUE )
endif()

######################################################
######################################################
# use config variables such as these in
#   ../.SCL_CTEST_PREFS.cmake
#
#set( CTEST_SITE "username")
#set( CTEST_BUILD_NAME "build type, os, arch")
#
#
# setting any of these to true causes that set of tests
# to be skipped (unless another test depends on that test)
# SKIP_CPP_TEST_SCHEMA_GEN
# SKIP_CPP_TEST_SCHEMA_BUILD
# SKIP_CPP_TEST_SCHEMA_RW
# SKIP_TEST_UNITARY_SCHEMAS
# SKIP_TEST_EXCHANGE_FILE
#
# setting this one disables result submission to my.cdash.org
# SKIP_SUBMISSION
######################################################
######################################################


function( SUBMIT_TEST part )
    if( NOT SKIP_SUBMISSION )
        ctest_submit( PARTS ${part} )
    endif()
endfunction( SUBMIT_TEST part )

# find number of processors, for faster builds
# from http://www.kitware.com/blog/home/post/63
if(NOT DEFINED PROCESSOR_COUNT)
  # Unknown:
  set(PROCESSOR_COUNT 0)

  # Linux:
  set(cpuinfo_file "/proc/cpuinfo")
  if(EXISTS "${cpuinfo_file}")
    file(STRINGS "${cpuinfo_file}" procs REGEX "^processor.: [0-9]+$")
    list(LENGTH procs PROCESSOR_COUNT)
  endif()

  # Mac:
  if(APPLE)
    find_program(cmd_sys_pro "system_profiler")
    if(cmd_sys_pro)
      execute_process(COMMAND ${cmd_sys_pro} OUTPUT_VARIABLE info)
      string(REGEX REPLACE "^.*Total Number Of Cores: ([0-9]+).*$" "\\1"
        PROCESSOR_COUNT "${info}")
    endif()
  endif()

  # Windows:
  if(WIN32)
    set(PROCESSOR_COUNT "$ENV{NUMBER_OF_PROCESSORS}")
  endif()
endif()

set(CTEST_BUILD_FLAGS "-j${PROCESSOR_COUNT}")


######################################################
##### To disable reporting of a set of tests, comment
##### out the SUBMIT_TEST line immediately following
##### the set you wish to disable.
#####
##### To do this for all tests:
##### set( SKIP_SUBMISSION TRUE )
######################################################

ctest_start(Experimental)
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})

ctest_configure( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND OPTIONS -DENABLE_TESTING=ON )
SUBMIT_TEST( Configure )
ctest_build( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND )
SUBMIT_TEST( Build )
# ctest_memcheck( BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res PARALLEL_LEVEL ${PROCESSOR_COUNT} )

if(NOT SKIP_TEST_UNITARY_SCHEMAS )
    ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                PARALLEL_LEVEL ${PROCESSOR_COUNT} INCLUDE_LABEL "unitary_schemas" )
    SUBMIT_TEST( Test )
endif()

if(NOT SKIP_CPP_TEST_SCHEMA_GEN )
    ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                PARALLEL_LEVEL ${PROCESSOR_COUNT} INCLUDE_LABEL "cpp_schema_gen" )
    SUBMIT_TEST( Test )
    if(NOT SKIP_CPP_TEST_SCHEMA_BUILD )
        ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                    PARALLEL_LEVEL ${PROCESSOR_COUNT} INCLUDE_LABEL "cpp_schema_build" )
        SUBMIT_TEST( Test )
        if(NOT SKIP_CPP_TEST_SCHEMA_RW )
            ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                        PARALLEL_LEVEL ${PROCESSOR_COUNT} INCLUDE_LABEL "cpp_schema_rw" )
            SUBMIT_TEST( Test )
        endif()
    endif()
endif()

if(NOT SKIP_TEST_EXCHANGE_FILE )
    if( SKIP_CPP_TEST_SCHEMA_BUILD )
        ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                PARALLEL_LEVEL ${PROCESSOR_COUNT} INCLUDE "build_cpp_sdai_AP214E3_2010" )
        SUBMIT_TEST( Test )
    endif()
    ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                PARALLEL_LEVEL ${PROCESSOR_COUNT} INCLUDE_LABEL "exchange_file" )
    SUBMIT_TEST( Test )
endif()

# ctest_coverage( )
