find_package(ADAPTAGRAMS)
if(ADAPTAGRAMS_FOUND)
  add_definitions(-DHAVE_ADAPTAGRAMS=1)
else(ADAPTAGRAMS_FOUND)
  set(AVOID_INCLUDE_DIR "")
  set(ADAPTAGRAMS_LIBRARIES "")
endif(ADAPTAGRAMS_FOUND)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${AVOID_INCLUDE_DIR}
)

ged_plugin_library(ged-graph graph.cpp)
target_link_libraries(ged-graph libged libbu ${ADAPTAGRAMS_LIBRARIES})

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
