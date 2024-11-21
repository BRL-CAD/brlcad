include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(SELECT_SRCS select.c select2.cpp)

ged_plugin_library(ged-select ${SELECT_SRCS})
target_link_libraries(ged-select libged libbu)

cmakefiles(
  CMakeLists.txt
  ${SELECT_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
