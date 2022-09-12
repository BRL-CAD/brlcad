include (VCICommon)

include_directories (
  ../../..
  ${CMAKE_CURRENT_SOURCE_DIR}
)


set( headers
  ProgViewerWidget.hh
  ../QtViewer/QGLViewerWidget.hh
  ../QtViewer/MeshViewerWidgetT.hh
  ../QtViewer/MeshViewerWidgetT_impl.hh
)

set( sources
  ProgViewerWidget.cc
  progviewer.cc  
  ../QtViewer/QGLViewerWidget.cc
)

if (WIN32)
  vci_add_executable( ProgViewer WIN32 ${sources} ${headers})
else ()
  vci_add_executable( ProgViewer ${sources} ${headers})
endif ()

target_link_libraries ( ProgViewer
  OpenMeshCore
  OpenMeshTools
  ${QT_TARGET}::OpenGL
  ${QT_TARGET}::Widgets
  ${QT_TARGET}::Gui
  ${OPENGL_LIBRARIES}
)

if (QT_VERSION_MAJOR GREATER 5)
  target_link_libraries (ProgViewer ${QT_TARGET}::OpenGLWidgets)
endif()

