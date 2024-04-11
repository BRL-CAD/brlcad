include (VCICommon)

include_directories (
  ../../../..
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set (headers
  ../MeshViewerWidget.hh
  ../SubdivideWidget.hh
  ../../QtViewer/QGLViewerWidget.hh
  ../../QtViewer/MeshViewerWidgetT.hh
  ../../QtViewer/MeshViewerWidgetT_impl.hh
)

set (sources
  ../../QtViewer/QGLViewerWidget.cc
  ../SubdivideWidget.cc
  ../qtsubdivider.cc
)

if (WIN32)
  vci_add_executable (SubdividerGui WIN32 ${sources} ${headers})
else ()
  vci_add_executable (SubdividerGui ${sources} ${headers})
endif ()

target_link_libraries (SubdividerGui
  OpenMeshCore
  OpenMeshTools
  ${QT_TARGET}::OpenGL
  ${QT_TARGET}::Widgets
  ${QT_TARGET}::Gui
  ${OPENGL_LIBRARIES}
)

if (QT_VERSION_MAJOR GREATER 5)
  target_link_libraries (SubdividerGui ${QT_TARGET}::OpenGLWidgets)
endif()
