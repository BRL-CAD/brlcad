#
# Adding unittest for unitary schemas
#
file(GLOB UNITARY_SCHEMAS "${CMAKE_CURRENT_SOURCE_DIR}/unitary_schemas/*.exp")
foreach(UNITARY_SCHEMA ${UNITARY_SCHEMAS})
  get_filename_component(SCHEMA_NAME ${UNITARY_SCHEMA} NAME_WE)
  set(TEST_NAME test_unit_${SCHEMA_NAME})
  add_test(${TEST_NAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/check-express ${UNITARY_SCHEMA})
  set_tests_properties( ${TEST_NAME} PROPERTIES LABELS unitary_schemas DEPENDS build_check_express )
  #if the file name begins with fail_, then testing it should result in an error.
  if( UNITARY_SCHEMA MATCHES "fail_.*" )
    set_tests_properties( ${TEST_NAME} PROPERTIES WILL_FAIL true )
  endif( UNITARY_SCHEMA MATCHES "fail_.*" )
endforeach(UNITARY_SCHEMA ${UNITARY_SCHEMAS})

add_subdirectory(p21)
add_subdirectory(cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

