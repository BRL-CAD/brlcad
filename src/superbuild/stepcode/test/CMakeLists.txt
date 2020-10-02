#
# Adding unittest for unitary schemas
#
FILE(GLOB UNITARY_SCHEMAS "${CMAKE_CURRENT_SOURCE_DIR}/unitary_schemas/*.exp")
FOREACH(UNITARY_SCHEMA ${UNITARY_SCHEMAS})
  GET_FILENAME_COMPONENT(SCHEMA_NAME ${UNITARY_SCHEMA} NAME_WE)
  SET(TEST_NAME test_unit_${SCHEMA_NAME})
  ADD_TEST(${TEST_NAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/check-express ${UNITARY_SCHEMA})
  set_tests_properties( ${TEST_NAME} PROPERTIES LABELS unitary_schemas )
  #if the file name begins with fail_, then testing it should result in an error.
  if( UNITARY_SCHEMA MATCHES "fail_.*" )
    set_tests_properties( ${TEST_NAME} PROPERTIES WILL_FAIL true )
  endif( UNITARY_SCHEMA MATCHES "fail_.*" )
ENDFOREACH(UNITARY_SCHEMA ${UNITARY_SCHEMAS})

add_subdirectory(p21)
add_subdirectory(cpp)
