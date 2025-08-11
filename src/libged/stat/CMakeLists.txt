include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${BRLCAD_SOURCE_DIR}/src/libbu # for fort.h
)

ged_plugin_library(ged-stat stat.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../../libbu/fort.c)
target_link_libraries(ged-stat libged libbu)

cmakefiles(
  CMakeLists.txt
  stat.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
