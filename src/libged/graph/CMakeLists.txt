brlcad_find_package(Adaptagrams COMPONENTS avoid cola dialect topology)
if(Adaptagrams_FOUND)
  add_definitions(-DHAVE_ADAPTAGRAMS=1)
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${Adaptagrams_INCLUDE_DIRS}
)

set(GED_GRAPH_LIBS
  libged
  libbu
)
if(Adaptagrams_FOUND)
  set(GED_GRAPH_LIBS ${GED_GRAPH_LIBS}
    adaptagrams::dialect
    adaptagrams::topology
    adaptagrams::cola
    adaptagrams::avoid
    adaptagrams::vpsc
  )
endif()

ged_plugin_library(ged-graph graph.cpp)
target_link_libraries(ged-graph ${GED_GRAPH_LIBS})

cmakefiles(
  CMakeLists.txt
  graph.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
