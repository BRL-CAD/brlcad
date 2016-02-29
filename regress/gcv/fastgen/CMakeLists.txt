configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-fastgen.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-fastgen.cmake" @ONLY)
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fastgen.done"
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-fastgen.cmake"
  DEPENDS fast4-g gdiff "${CMAKE_CURRENT_SOURCE_DIR}/fastgen_dos.fast4" "${CMAKE_CURRENT_SOURCE_DIR}/fastgen_box.fast4"
  )

add_test(NAME regress-fastgen COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-fastgen.cmake")
add_custom_target(regress-fastgen DEPENDS fast4-g gdiff "${CMAKE_CURRENT_BINARY_DIR}/fastgen.done")
set_target_properties(regress-fastgen PROPERTIES FOLDER "BRL-CAD Regression Tests")
Add_Regression_Test(fastgen "fast4-g;gdiff")

DISTCLEAN(
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen.done"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen.log"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen_unix.g"
  "${CMAKE_CURRENT_BINARY_DIR}/fastgen_dos.g"
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
