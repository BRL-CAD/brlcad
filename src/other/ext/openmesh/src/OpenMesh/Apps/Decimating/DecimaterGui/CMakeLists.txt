include (VCICommon)

include_directories (
  ../../../..
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set (headers
  ../DecimaterViewerWidget.hh
  ../../QtViewer/QGLViewerWidget.hh
  ../../QtViewer/MeshViewerWidgetT.hh
  ../../QtViewer/MeshViewerWidget.hh
  ../../QtViewer/MeshViewerWidgetT_impl.hh
)

set (sources
  ../../QtViewer/QGLViewerWidget.cc
  ../../QtViewer/MeshViewerWidget.cc
  ../DecimaterViewerWidget.cc
  ../decimaterviewer.cc
)

if (WIN32)
  vci_add_executable (DecimaterGui WIN32 ${sources} ${headers})
else ()
  vci_add_executable (DecimaterGui ${sources} ${headers} )
endif ()

target_link_libraries (DecimaterGui
  OpenMeshCore
  OpenMeshTools
  ${QT_TARGET}::OpenGL
  ${QT_TARGET}::Widgets
  ${QT_TARGET}::Gui
  ${OPENGL_LIBRARIES}
)

if (QT_VERSION_MAJOR GREATER 5)
  target_link_libraries (DecimaterGui ${QT_TARGET}::OpenGLWidgets)
endif()

