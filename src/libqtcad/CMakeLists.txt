if(BRLCAD_ENABLE_QT)

  if (BRLCAD_ENABLE_OPENGL)
    if (OPENGL_opengl_LIBRARY)
      set(OPENGL_LIBS ${OPENGL_opengl_LIBRARY})
    endif (OPENGL_opengl_LIBRARY)
    if (OPENGL_gl_LIBRARY)
      set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
    endif (OPENGL_gl_LIBRARY)
  endif (BRLCAD_ENABLE_OPENGL)
  if (OPENGL_LIBS)
    find_package(Qt5 COMPONENTS Core Widgets Gui OpenGL)
    set(OPENGL_LIBS "Qt5::OpenGL;${OPENGL_LIBS}")
  else (OPENGL_LIBS)
    find_package(Qt5 COMPONENTS Core Widgets Gui)
  endif (OPENGL_LIBS)

  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${GED_INCLUDE_DIRS}
    ${DM_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR_GL}
    )

endif(BRLCAD_ENABLE_QT)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qtcad_srcs
  bindings.cpp
  QAccordionWidget.cpp
  QtSW.cpp
  QtCADQuad.cpp
  QtCADView.cpp
  QtConsole.cpp
  QtConsoleListener.cpp
  )

set(QTCAD_HDR_DIR ${BRLCAD_SOURCE_DIR}/include/qtcad)
set(qtcad_moc_headers
  bindings.h
  ${QTCAD_HDR_DIR}/QAccordionWidget.h
  ${QTCAD_HDR_DIR}/QtCADView.h
  ${QTCAD_HDR_DIR}/QtCADQuad.h
  ${QTCAD_HDR_DIR}/QtConsole.h
  ${QTCAD_HDR_DIR}/QtConsoleListener.h
  ${QTCAD_HDR_DIR}/QtSW.h
  )

if (OPENGL_LIBS)
  set(qtcad_srcs
    ${qtcad_srcs}
    QtGL.cpp
    )
  set(qtcad_moc_headers
    ${qtcad_moc_headers}
    ${QTCAD_HDR_DIR}/QtGL.h
    )
endif (OPENGL_LIBS)


if(BRLCAD_ENABLE_QT)

  set(USE_OBJECT_LIBS OFF)

  QT5_WRAP_CPP(qtcad_moc_srcs ${qtcad_moc_headers})
  BRLCAD_ADDLIB(libqtcad "${qtcad_srcs};${qtcad_moc_srcs}" "libged;libdm;librt;libbu;Qt5::Core;Qt5::Widgets;${OPENGL_LIBS}")
  if (OPENGL_LIBS)
    set_property(TARGET libqtcad APPEND PROPERTY COMPILE_DEFINITIONS "QT_SYSTEM_OPENGL")
  endif (OPENGL_LIBS)
endif(BRLCAD_ENABLE_QT)

set(qtcad_ignore_srcs
  CMakeLists.txt
  ${qtcad_srcs}
  ${qtcad_moc_headers}
  QtGL.cpp
  )

CMAKEFILES(${qtcad_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

