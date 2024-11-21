include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

ged_plugin_library(ged-edit edit.c edit2.cpp)
target_link_libraries(ged-edit libged libbu)

cmakefiles(
  CMakeLists.txt
  edit.c
  edit2.cpp
  ged_edit2.h
  uri.hh
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
