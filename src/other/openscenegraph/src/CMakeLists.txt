
add_subdirectory(OpenThreads)
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/OpenThreads/include
  ${CMAKE_CURRENT_SOURCE_DIR}/OpenThreads/include
  )

add_subdirectory(osg)
add_subdirectory(osgUtil)
add_subdirectory(osgDB)
add_subdirectory(osgGA)
add_subdirectory(osgText)
add_subdirectory(osgViewer)
add_subdirectory(osgPlugins)
add_subdirectory(osgWidget)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
