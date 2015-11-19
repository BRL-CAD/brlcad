# This should get rolled into a macro as more formats are added

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dem/regress-dem.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/dem/regress-dem.cmake @ONLY)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dem/hawaii-c.done
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/dem/regress-dem.cmake
  DEPENDS dem-g ${CMAKE_CURRENT_SOURCE_DIR}/dem/hawaii-c.tar.bz2 ${CMAKE_CURRENT_SOURCE_DIR}/dem/hawaii-c-control.tar.bz2
  )

add_custom_target(regress-dem DEPENDS dem-g ${CMAKE_CURRENT_BINARY_DIR}/dem/hawaii-c.done)
set_target_properties(regress-dem PROPERTIES FOLDER "BRL-CAD Regression Tests")
set_target_properties(regress-dem PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

CMAKEFILES(dem)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
