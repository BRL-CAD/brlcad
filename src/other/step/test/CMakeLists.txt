
#
# Adding unittest for unitary schemas
#
FILE(GLOB UNITARY_SCHEMAS "${CMAKE_CURRENT_SOURCE_DIR}/unitary_schemas/*.exp")
FOREACH(UNITARY_SCHEMA ${UNITARY_SCHEMAS})
  GET_FILENAME_COMPONENT(SCHEMA_NAME ${UNITARY_SCHEMA} NAME_WE)
  # setting test_name
  SET(TEST_NAME test${SCHEMA_NAME})
  # add test
  ADD_TEST(${TEST_NAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/check-express ${UNITARY_SCHEMA})
  set_tests_properties( ${TEST_NAME} PROPERTIES LABELS unitary_schemas )
ENDFOREACH(UNITARY_SCHEMA ${UNITARY_SCHEMAS})

add_subdirectory(p21)
