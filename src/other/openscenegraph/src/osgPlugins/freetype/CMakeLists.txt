IF(WIN32)
    SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:MSVCRT")
ENDIF()

INCLUDE_DIRECTORIES(${FREETYPE_INCLUDE_DIRS} )

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
install(TARGETS osgdb_freetype
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
