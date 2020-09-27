CMAKE_MINIMUM_REQUIRED(VERSION 3.12)
#c++ tests for clstepcore

INCLUDE_DIRECTORIES( ${SC_SOURCE_DIR}/src/cldai ${SC_SOURCE_DIR}/src/cleditor ${SC_SOURCE_DIR}/src/clutils
         ${SC_SOURCE_DIR}/src/clstepcore ${SC_SOURCE_DIR}/src/base )

function(add_stepcore_test name libs)
    SC_ADDEXEC( tst_${name} test_${name}.cc "${libs}" "TESTABLE" )
    add_test( NAME build_cpp_${name}
              WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
              COMMAND ${CMAKE_COMMAND} --build .
                                       --target tst_${name}
                                       --config $<CONFIGURATION> )
    add_test( NAME test_${name}
              WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
              COMMAND $<TARGET_FILE:tst_${name}>  )
    set_tests_properties( build_cpp_${name} PROPERTIES LABELS cpp_unit_stepcore DEPENDS stepcore )
    set_tests_properties( test_${name} PROPERTIES LABELS cpp_unit_stepcore DEPENDS build_cpp_${name} )
endfunction(add_stepcore_test name libs)

add_stepcore_test( "SupertypesIterator" "stepcore;steputils;stepeditor;stepdai;base" )
