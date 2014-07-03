# run_ctest.cmake
# `ctest -S run_ctest.cmake`

set( CTEST_SOURCE_DIRECTORY . )
set( CTEST_BINARY_DIRECTORY build_ctest )
set( CTEST_CMAKE_GENERATOR "Unix Makefiles" )
set( CTEST_MEMORYCHECK_COMMAND /usr/bin/valgrind )
set( CTEST_INITIAL_CACHE "
SITE:STRING=${CTEST_SITE}
BUILDNAME:STRING=${CTEST_BUILD_NAME}
SC_ENABLE_TESTING:BOOL=ON
SC_BUILD_TYPE:STRING=Debug
")



if( EXISTS "../.SC_CTEST_PREFS.cmake" )
    include( "../.SC_CTEST_PREFS.cmake" )
else()
    message( WARNING "Did not find ../.SC_CTEST_PREFS.cmake, containing config variables - result submission disabled" )
    set( SKIP_SUBMISSION TRUE )
endif()

######################################################
######################################################
# use config variables such as these in
#   ../.SC_CTEST_PREFS.cmake
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

file(WRITE "${CTEST_BINARY_DIRECTORY}/CTestCustom.cmake" "
set( CTEST_CUSTOM_ERROR_EXCEPTION \"{standard input}:[0-9][0-9]*: WARNING: \")
")

ctest_configure( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND OPTIONS -DSC_ENABLE_TESTING=ON )
SUBMIT_TEST( Configure )
ctest_build( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND )
SUBMIT_TEST( Build )
# ctest_memcheck( BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res )

if(NOT SKIP_TEST_UNITARY_SCHEMAS )
    ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                INCLUDE_LABEL "unitary_schemas" )
    SUBMIT_TEST( Test )
endif()

if(NOT SKIP_CPP_TEST_SCHEMA_GEN )
    ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                INCLUDE_LABEL "cpp_schema_gen" )
    SUBMIT_TEST( Test )
    if(NOT SKIP_CPP_TEST_SCHEMA_BUILD )
        ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                    INCLUDE_LABEL "cpp_schema_build" )
        SUBMIT_TEST( Test )
        if(NOT SKIP_CPP_TEST_SCHEMA_RW )
            ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                        INCLUDE_LABEL "cpp_schema_rw" )
            SUBMIT_TEST( Test )
        endif()
    endif()
endif()

if(NOT SKIP_TEST_EXCHANGE_FILE )
    if( SKIP_CPP_TEST_SCHEMA_BUILD )
        ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                INCLUDE "build_cpp_sdai_AP214E3_2010" )
        SUBMIT_TEST( Test )
    endif()
    ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                INCLUDE_LABEL "exchange_file" )
    SUBMIT_TEST( Test )
endif()

if(NOT SKIP_TEST_CPP_SCHEMA_SPECIFIC )
    ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                INCLUDE_LABEL "cpp_schema_specific" )
    SUBMIT_TEST( Test )
endif()

if(NOT SKIP_TEST_CPP_UNIT )
    ctest_test( BUILD "${CTEST_BINARY_DIRECTORY}" APPEND
                INCLUDE_LABEL "cpp_unit_....*" )
    SUBMIT_TEST( Test )
endif()

# ctest_coverage( )
