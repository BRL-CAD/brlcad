set(OSG_OSG_PLUGIN_SRCS
  ReaderWriterOSG.cpp
  ReaderWriterOSG2.cpp
  )

if(DYNAMIC_OPENSCENEGRAPH)
  add_library(osgdb_osg MODULE ${OSG_OSG_PLUGIN_SRCS})
else(DYNAMIC_OPENSCENEGRAPH)
  add_library(osgdb_osg STATIC ${OSG_OSG_PLUGIN_SRCS})
endif(DYNAMIC_OPENSCENEGRAPH)

target_link_libraries(osgdb_osg osgDB osgUtil osg OpenThreads
  ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY} ${COMPRESSION_LIBRARIES}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
