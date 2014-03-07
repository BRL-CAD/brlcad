include_directories(${FREETYPE_INCLUDE_DIRS} )

if(APPLE)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-dylib_file,/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib")
endif(APPLE)

set(OSG_FREETYPE_PLUGIN_SRCS
  FreeTypeFont.cpp
  FreeTypeLibrary.cpp
  ReaderWriterFreeType.cpp
  )

if(DYNAMIC_OPENSCENEGRAPH)
  add_library(osgdb_freetype MODULE ${OSG_FREETYPE_PLUGIN_SRCS})
else(DYNAMIC_OPENSCENEGRAPH)
  add_library(osgdb_freetype STATIC ${OSG_FREETYPE_PLUGIN_SRCS})
endif(DYNAMIC_OPENSCENEGRAPH)

target_link_libraries(osgdb_freetype osgText osgDB osgUtil osg OpenThreads ${FREETYPE_LIBRARY})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
