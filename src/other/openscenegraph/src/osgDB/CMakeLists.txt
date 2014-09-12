if(DYNAMIC_OPENSCENEGRAPH)
  option(OSG_PLUGIN_SEARCH_INSTALL_DIR_FOR_PLUGINS "Set to ON to have OpenSceneGraph search the configured install directory for plugins." ON)
  add_definitions(-DOSGDB_LIBRARY)
  if(OSG_PLUGIN_SEARCH_INSTALL_DIR_FOR_PLUGINS)
    # Add a default plugin search path component
    add_definitions(-DOSG_DEFAULT_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib${LIB_POSTFIX}/${OSG_PLUGINS})
    add_definitions(-DOSG_RELATIVE_LIBRARY_PATH=/lib${LIB_POSTFIX}/${OSG_PLUGINS})
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
  if(OSG_DEFAULT_IMAGE_PLUGIN_FOR_OSX STREQUAL "quicktime")
    add_definitions(-DDARWIN_QUICKTIME)
  else(OSG_DEFAULT_IMAGE_PLUGIN_FOR_OSX STREQUAL "quicktime")
    add_definitions(-DDARWIN_IMAGEIO)
  endif(OSG_DEFAULT_IMAGE_PLUGIN_FOR_OSX STREQUAL "quicktime")
endif(APPLE AND NOT ANDROID)
if(QTKIT_FOUND)
  add_definitions(-DUSE_QTKIT)
endif(QTKIT_FOUND)
if(QUICKTIME_FOUND)
  add_definitions(-DUSE_QUICKTIME)
endif(QUICKTIME_FOUND)
if(AV_FOUNDATION_FOUND)
  add_definitions(-DUSE_AV_FOUNDATION)
endif(AV_FOUNDATION_FOUND)
if(ZLIB_FOUND)
  add_definitions( -DUSE_ZLIB )
  include_directories(${ZLIB_INCLUDE_DIR})
  set(COMPRESSION_LIBRARIES ${ZLIB_LIBRARY})
endif(ZLIB_FOUND)

add_definitions(-DOSG_PLUGIN_EXTENSION=${CMAKE_SHARED_MODULE_SUFFIX})

add_library(osgDB ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${OSGDB_SRCS})
target_link_libraries(osgDB OpenThreads osg osgUtil ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY} ${COMPRESSION_LIBRARIES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
