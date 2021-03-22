#tests specific to exp2cxx

set(unitary_dir ${SC_SOURCE_DIR}/test/unitary_schemas)

add_test(NAME build_exp2cxx
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} --build .
  --target exp2cxx
  --config $<CONFIGURATION>
  )

add_test(NAME test_exp2cxx_unique_qualifiers
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:exp2cxx>
  -DINFILE=${unitary_dir}/unique_qualifiers.exp
  -P ${CMAKE_CURRENT_SOURCE_DIR}/unique_qualifiers.cmake
 )

add_test(NAME test_exp2cxx_inverse_qualifiers
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:exp2cxx>
  -DINFILE=${unitary_dir}/inverse_qualifiers.exp
  -P ${CMAKE_CURRENT_SOURCE_DIR}/inverse_qualifiers.cmake
 )
set_tests_properties(test_exp2cxx_unique_qualifiers test_exp2cxx_inverse_qualifiers PROPERTIES DEPENDS build_exp2cxx)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

