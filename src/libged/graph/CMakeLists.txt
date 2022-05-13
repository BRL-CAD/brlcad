find_package(ADAPTAGRAMS)
if(ADAPTAGRAMS_FOUND)
  add_definitions(-DHAVE_ADAPTAGRAMS=1)
endif(ADAPTAGRAMS_FOUND)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${AVOID_INCLUDE_DIR}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-graph SHARED graph.cpp)
target_link_libraries(ged-graph libged libbu ${ADAPTAGRAMS_LIBRARIES})
set_property(TARGET ged-graph APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-graph graph.cpp)
PLUGIN_SETUP(ged-graph ged)

CMAKEFILES(
  CMakeLists.txt
  graph.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
