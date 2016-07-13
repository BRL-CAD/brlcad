
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/regress-dem.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/regress-dem.cmake" @ONLY)
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dem.done"
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-dem.cmake"
  DEPENDS dem-g "${CMAKE_CURRENT_SOURCE_DIR}/hawaii-c.tar.bz2"
  )

add_test(NAME regress-gcv-dem COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regress-dem.cmake")
add_custom_target(regress-gcv-dem DEPENDS dem-g "${CMAKE_CURRENT_BINARY_DIR}/dem.done")
set_target_properties(regress-gcv-dem PROPERTIES FOLDER "BRL-CAD Regression Tests")
set_target_properties(regress-gcv-dem PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
Add_Regression_Test(gcv-dem "dem-g")
add_dependencies(regress-gcv regress-gcv-dem)

DISTCLEAN(
  "${CMAKE_CURRENT_BINARY_DIR}/dem.done"
  "${CMAKE_CURRENT_BINARY_DIR}/regress-dem.log"
  "${CMAKE_CURRENT_BINARY_DIR}/hawaii-c"
  "${CMAKE_CURRENT_BINARY_DIR}/hawaii-c.dsp"
  "${CMAKE_CURRENT_BINARY_DIR}/hawaii-c.g"
  "${CMAKE_CURRENT_BINARY_DIR}/hawaii-c.tmp"
  )

CMAKEFILES(
  hawaii-c.tar.bz2
  README
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
