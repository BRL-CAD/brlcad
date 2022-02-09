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

# We build the resources in at this level, but list them out
# in the subdirectories
add_subdirectory(images)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qtcad_srcs
  bindings.cpp
  QAccordion.cpp
  QColorRGB.cpp
  QFlowLayout.cpp
  QKeyVal.cpp
  QToolPalette.cpp
  QgModel.cpp
  QgSelectionProxyModel.cpp
  QgTreeView.cpp
  QtAppExecDialog.cpp
  QtSW.cpp
  QtCADQuad.cpp
  QtCADTree.cpp
  QtCADView.cpp
  QtConsole.cpp
  QtConsoleListener.cpp
  )

set(QTCAD_HDR_DIR ${BRLCAD_SOURCE_DIR}/include/qtcad)
set(qtcad_moc_headers bindings.h)
set(qth_names
  QAccordion
  QColorRGB
  QFlowLayout
  QKeyVal
  QToolPalette
  QgModel
  QgTreeView
  QtAppExecDialog
  QtCADTree
  QtCADView
  QtCADQuad
  QtConsole
  QtConsoleListener
  QtSW
  )
if (OPENGL_LIBS)
  set(qtcad_srcs ${qtcad_srcs} QtGL.cpp)
  set(qth_names ${qth_names} QtGL)
endif (OPENGL_LIBS)

foreach(qh ${qth_names})
  set(qtcad_moc_headers ${qtcad_moc_headers} ${QTCAD_HDR_DIR}/${qh}.h)
endforeach(qh ${qth_names})

if(BRLCAD_ENABLE_QT)

  set(USE_OBJECT_LIBS OFF)

  QT5_WRAP_CPP(qtcad_moc_srcs ${qtcad_moc_headers})
  QT5_ADD_RESOURCES(qtcad_qrc qtcad_resources.qrc)
  BRLCAD_ADDLIB(libqtcad "${qtcad_srcs};${qtcad_moc_srcs};${qtcad_qrc}" "libged;libdm;librt;libbu;Qt5::Core;Qt5::Widgets;${OPENGL_LIBS}")
  if (OPENGL_LIBS)
    set_property(TARGET libqtcad APPEND PROPERTY COMPILE_DEFINITIONS "QT_SYSTEM_OPENGL")
  endif (OPENGL_LIBS)

  DISTCLEAN(
    moc_bindings.cpp_parameters
    qtcad_resources.qrc.depends
    )
  foreach(qh ${qth_names})
    DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/__/__/include/qtcad/moc_${qh}.cpp_parameters)
  endforeach(qh ${qth_names})

endif(BRLCAD_ENABLE_QT)

add_subdirectory(tests)

set(qtcad_ignore_srcs
  CMakeLists.txt
  ${qtcad_srcs}
  ${qtcad_moc_headers}
  qtcad_resources.qrc
  QtGL.cpp
  qt6.patch
  )

CMAKEFILES(${qtcad_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

