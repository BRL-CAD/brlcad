if(DYNAMIC_OPENSCENEGRAPH)
  add_definitions(-DOSGWIDGET_LIBRARY)
else()
  add_definitions(-DOSG_LIBRARY_STATIC)
endif()

# FIXME: For OS X, need flag for Framework or dylib
SET(OSGWIDGET_SRCS
  Box.cpp
  Browser.cpp
  PdfReader.cpp
  VncClient.cpp
  Canvas.cpp
  CMakeLists.txt
  Frame.cpp
  Input.cpp
  Label.cpp
  Lua.cpp
  Python.cpp
  StyleManager.cpp
  Table.cpp
  Util.cpp
  Version.cpp
  ViewerEventHandlers.cpp
  Widget.cpp
  Window.cpp
  WindowManager.cpp
  ${OPENSCENEGRAPH_VERSIONINFO_RC}
  )

add_library(osgWidget ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${OSGWIDGET_SRCS})
target_link_libraries(osgWidget osgText osgViewer osgDB osg OpenThreads osgGA
  ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY})
install(TARGETS osgWidget
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

