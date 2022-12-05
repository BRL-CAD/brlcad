include (VCICommon)

include_directories (
  ../../../..
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set (headers
  VDPMSynthesizerViewerWidget.hh
)

set (sources
  ../../QtViewer/QGLViewerWidget.cc
  vdpmsynthesizer.cc
  VDPMSynthesizerViewerWidget.cc
)


if (WIN32)
  vci_add_executable (Synthesizer WIN32 ${sources} ${headers})
else ()
  vci_add_executable (Synthesizer ${sources} ${headers})
endif ()

target_link_libraries (Synthesizer
  OpenMeshCore
  OpenMeshTools
  ${QT_TARGET}::OpenGL
  ${QT_TARGET}::Widgets
  ${QT_TARGET}::Gui
  ${OPENGL_LIBRARIES}
)

if (QT_VERSION_MAJOR GREATER 5)
  target_link_libraries (Synthesizer ${QT_TARGET}::OpenGLWidgets)
endif()
