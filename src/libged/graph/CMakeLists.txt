set(GRAPH_LIBS libged libbu)
brlcad_find_package(Adaptagrams COMPONENTS avoid cola dialect topology)
if(Adaptagrams_FOUND)
  set(ADAPT_DEF "HAVE_ADAPTAGRAMS")
  set(GRAPH_LIBS ${GED_GRAPH_LIBS}
    adaptagrams::dialect
    adaptagrams::topology
    adaptagrams::cola
    adaptagrams::avoid
    adaptagrams::vpsc
  )
endif()

# The graph plugin uses Adaptagrams, which is an LGPL library, so we
# always build it as a plugin by specifying the PLUGIN_ONLY option.
#
# This also makes sure the plugin version of the libged command loading
# mechanism is always tested.
ged_plugin_library(ged-graph
  graph.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  SYSTEM_INCLUDES ${Adaptagrams_INCLUDE_DIRS}
  LIBS ${GRAPH_LIBS}
  DEFINITIONS "${ADAPT_DEF}"
  PLUGIN_ONLY
)

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
