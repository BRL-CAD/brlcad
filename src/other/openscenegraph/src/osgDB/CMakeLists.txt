if(DYNAMIC_OPENSCENEGRAPH)
  option(OSG_PLUGIN_SEARCH_INSTALL_DIR_FOR_PLUGINS "Set to ON to have OpenSceneGraph search the configured install directory for plugins." ON)
  add_definitions(-DOSGDB_LIBRARY)
  if(OSG_PLUGIN_SEARCH_INSTALL_DIR_FOR_PLUGINS)
        # Add a default plugin search path component
    add_definitions(-DOSG_DEFAULT_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib${LIB_POSTFIX}/${OSG_PLUGINS})
  endif()
else(DYNAMIC_OPENSCENEGRAPH)
  add_definitions(-DOSG_LIBRARY_STATIC)
endif(DYNAMIC_OPENSCENEGRAPH)

set(OSGDB_SRCS
    ObjectWrapper.cpp
    InputStream.cpp
    OutputStream.cpp
    Compressors.cpp
    Archive.cpp
    AuthenticationMap.cpp
    Callbacks.cpp
    ConvertUTF.cpp
    DatabasePager.cpp
    DatabaseRevisions.cpp
    DotOsgWrapper.cpp
    DynamicLibrary.cpp
    ExternalFileWriter.cpp
    Field.cpp
    FieldReader.cpp
    FieldReaderIterator.cpp
    FileCache.cpp
    FileNameUtils.cpp
    FileUtils.cpp
    fstream.cpp
    ImageOptions.cpp
    ImagePager.cpp
    Input.cpp
    MimeTypes.cpp
    Output.cpp
    Options.cpp
    PluginQuery.cpp
    ReaderWriter.cpp
    ReadFile.cpp
    Registry.cpp
    SharedStateManager.cpp
    StreamOperator.cpp
    Version.cpp
    WriteFile.cpp
    XmlParser.cpp
)

if(APPLE AND NOT ANDROID)
  if(NOT OSG_BUILD_PLATFORM_IPHONE AND NOT OSG_BUILD_PLATFORM_IPHONE_SIMULATOR)
    # Needs CoreFoundation calls and a Carbon function
    SET(OSGDB_PLATFORM_SPECIFIC_LIBRARIES ${CARBON_LIBRARY} ${COCOA_LIBRARY})
  endif()

  if(OSG_DEFAULT_IMAGE_PLUGIN_FOR_OSX STREQUAL "quicktime")
    ADD_DEFINITIONS(-DDARWIN_QUICKTIME)
  else()
    ADD_DEFINITIONS(-DDARWIN_IMAGEIO)
  endif()
endif()
if(QTKIT_FOUND)
  ADD_DEFINITIONS(-DUSE_QTKIT)
endif()
if(QUICKTIME_FOUND)
  ADD_DEFINITIONS(-DUSE_QUICKTIME)
endif()
if(AV_FOUNDATION_FOUND)
  ADD_DEFINITIONS(-DUSE_AV_FOUNDATION)
endif()
# We guarantee zlib
add_definitions(-DUSE_ZLIB)
include_directories(${ZLIB_INCLUDE_DIR})
set(COMPRESSION_LIBRARIES ${ZLIB_LIBRARY})

ADD_DEFINITIONS(-DOSG_PLUGIN_EXTENSION=${CMAKE_SHARED_MODULE_SUFFIX})

add_definitions(-DOSG_PLUGIN_EXTENSION=${CMAKE_SHARED_MODULE_SUFFIX})

add_library(osgDB ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${OSGDB_SRCS})
target_link_libraries(osgDB OpenThreads osg osgUtil
  ${OSGDB_PLATFORM_SPECIFIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY} ${COMPRESSION_LIBRARIES})
install(TARGETS osgDB
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
