include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Qt5Widgets_INCLUDE_DIRS}
  )

add_definitions(${Qt5Widgets_DEFINITIONS})

# We can't use the CMake global values for automoc et. al. because most of
# BRL-CAD's targets are not Qt targets and we don't want to run the
# automatic logic for all of them.


add_executable(qbrlcad main.cxx)
target_link_libraries(qbrlcad Qt5::Widgets)
install(TARGETS qbrlcad
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

