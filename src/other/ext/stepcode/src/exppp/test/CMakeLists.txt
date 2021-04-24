cmake_minimum_required(VERSION 3.6.3)
project(test_exppp)

set(breakLongStr_SRCS
     test_breakLongStr.c
     ../exppp.c
   )

add_test(NAME build_exppp
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} --build .
  --target exppp
  --config $<CONFIGURATION>
  )

# this executable doesn't really check the results, just ensures no segfaults. ought to improve it...
SC_ADDEXEC(tst_breakLongStr SOURCES ${breakLongStr_SRCS} LINK_LIBRARIES express base TESTABLE)
add_test(NAME build_tst_breakLongStr
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} --build .
  --target tst_breakLongStr
  --config $<CONFIGURATION>
  )
add_test(test_breakLongStr ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tst_breakLongStr)
set_tests_properties(test_breakLongStr PROPERTIES DEPENDS build_tst_breakLongStr)

#could run test schemas through exppp, then check line length...


# ---- syntax tests ----
set(unitary_dir ${SC_SOURCE_DIR}/test/unitary_schemas)

add_test(NAME test_exppp_unique_qualifiers
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -DEXPPP=$<TARGET_FILE:exppp>
  -DINFILE=${unitary_dir}/unique_qualifiers.exp
  -P ${CMAKE_CURRENT_SOURCE_DIR}/unique_qualifiers.cmake
  )

add_test(NAME test_exppp_lost_var
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -DEXPPP=$<TARGET_FILE:exppp>
  -DINFILE=${CMAKE_CURRENT_SOURCE_DIR}/exppp_lost_var.exp
  -P ${CMAKE_CURRENT_SOURCE_DIR}/exppp_lost_var.cmake
  )

add_test(NAME test_exppp_inverse_qualifiers
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -DEXPPP=$<TARGET_FILE:exppp>
  -DINFILE=${unitary_dir}/inverse_qualifiers.exp
  -P ${CMAKE_CURRENT_SOURCE_DIR}/inverse_qualifiers.cmake
  )

add_test(NAME test_exppp_div_slash
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -DEXPPP=$<TARGET_FILE:exppp>
  -DINFILE=${CMAKE_CURRENT_SOURCE_DIR}/exppp_div_slash.exp
  -P ${CMAKE_CURRENT_SOURCE_DIR}/exppp_div_slash.cmake
  )

add_test(NAME test_exppp_supertype_andor
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -DEXPPP=$<TARGET_FILE:exppp>
  -DINFILE=${CMAKE_CURRENT_SOURCE_DIR}/exppp_supertype_andor.exp
  -P ${CMAKE_CURRENT_SOURCE_DIR}/exppp_supertype_andor.cmake
  )

  set_tests_properties(test_exppp_unique_qualifiers test_exppp_inverse_qualifiers test_exppp_lost_var test_exppp_div_slash test_exppp_supertype_andor PROPERTIES DEPENDS build_exppp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
