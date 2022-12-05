include (VCICommon)

include_directories (
  ../../..
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# source code directories
set (directories 
  .
)

set (sources
  MeshViewerWidget.cc
  QGLViewerWidget.cc
  meshviewer.cc
)

set (headers
  MeshViewerWidget.hh
  QGLViewerWidget.hh  
)

if (WIN32)
  vci_add_executable (QtViewer WIN32 ${sources} ${headers})
else ()
  vci_add_executable (QtViewer ${sources} ${headers})
endif ()

target_link_libraries (QtViewer
  OpenMeshCore
  OpenMeshTools
  ${QT_TARGET}::OpenGL
  ${QT_TARGET}::Widgets
  ${QT_TARGET}::Gui
  ${OPENGL_LIBRARIES}
)

if (QT_VERSION_MAJOR GREATER 5)
  target_link_libraries (QtViewer ${QT_TARGET}::OpenGLWidgets)
endif()

