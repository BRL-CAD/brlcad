set(QG_DIRS)

if(BRLCAD_ENABLE_QT)
  find_package_qt(REQUIRED)

  if(BRLCAD_ENABLE_OPENGL)
    if(OPENGL_opengl_LIBRARY)
      set(OPENGL_LIBS ${OPENGL_opengl_LIBRARY})
    endif(OPENGL_opengl_LIBRARY)
    if(OPENGL_gl_LIBRARY)
      set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
    endif(OPENGL_gl_LIBRARY)
  endif(BRLCAD_ENABLE_OPENGL)
  if(OPENGL_LIBS)
    if(Qt6Widgets_FOUND)
      set(OPENGL_LIBS "Qt6::OpenGL;Qt6::OpenGLWidgets;${OPENGL_LIBS}")
    else()
      set(OPENGL_LIBS "Qt5::OpenGL;${OPENGL_LIBS}")
    endif(Qt6Widgets_FOUND)
  endif(OPENGL_LIBS)

  # Modern CMake FindOpenGL sets OPENGL_INCLUDE_DIR; older CMake set OPENGL_INCLUDE_DIR_GL.
  if(OPENGL_INCLUDE_DIR_GL)
    set(QG_GL_INC ${OPENGL_INCLUDE_DIR_GL})
  else()
    set(QG_GL_INC ${OPENGL_INCLUDE_DIR})
  endif()
  set(QG_DIRS ${QG_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QG_GL_INC})
endif(BRLCAD_ENABLE_QT)

# We build the resources in at this level, but list them out
# in the subdirectories
add_subdirectory(images)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(
  qtcad_srcs
  QgCanvasInput.cpp
  QgAccordion.cpp
  QgAppExecDialog.cpp
  QgAttributesModel.cpp
  QgColorRGB.cpp
  QgConsole.cpp
  QgConsoleListener.cpp
  QgDockWidget.cpp
  QgFlowLayout.cpp
  QgGeomImport.cpp
  QgKeyVal.cpp
  QgMeasureFilter.cpp
  QgModel.cpp
  QgPaletteController.cpp
  QgPluginCommands.cpp
  QgPluginContext.cpp
  QgPluginManager.cpp
  QgPolyFilter.cpp
  QgQuadView.cpp
  QgSW.cpp
  QgSelectFilter.cpp
  QgSession.cpp
  QgSketchFilter.cpp
  QgToolBase.cpp
  QgToolPalette.cpp
  QgTreeSelectionModel.cpp
  QgTreeView.cpp
  QgUtil.cpp
  QgView.cpp
  QgViewFilter.cpp
  QgViewCtrl.cpp
)
if(OPENGL_LIBS)
  set(qtcad_srcs ${qtcad_srcs} QgGL.cpp)
endif(OPENGL_LIBS)

set(QTCAD_HDR_DIR ${BRLCAD_SOURCE_DIR}/include/qtcad)
foreach(qs ${qtcad_srcs})
  get_filename_component(qbase ${qs} NAME_WE)
  if(EXISTS "${QTCAD_HDR_DIR}/${qbase}.h")
    set(qtcad_hdrs ${qtcad_hdrs} "${QTCAD_HDR_DIR}/${qbase}.h")
  endif(EXISTS "${QTCAD_HDR_DIR}/${qbase}.h")
endforeach(qs ${qtcad_srcs})

# Note - libqtcad_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
if(BRLCAD_ENABLE_QT)
  if(NOT BRLCAD_ENABLE_BRLCAD_LIBRARY)
    set(USE_OBJECT_LIBS OFF)
  endif()
  set(QTCAD_SRCS ${qtcad_srcs})
  set(QTCAD_PUBLIC_LIBS ${libqtcad_deps})

  if(Qt6Widgets_FOUND)
    qt6_add_resources(qtcad_qrc qtcad_resources.qrc)
    set(QTCAD_PUBLIC_LIBS ${QTCAD_PUBLIC_LIBS} Qt6::Core Qt6::Widgets ${OPENGL_LIBS})
  else()
    qt5_add_resources(qtcad_qrc qtcad_resources.qrc)
    set(QTCAD_PUBLIC_LIBS ${QTCAD_PUBLIC_LIBS} Qt5::Core Qt5::Widgets ${OPENGL_LIBS})
  endif(Qt6Widgets_FOUND)
  set(QTCAD_SRCS ${QTCAD_SRCS} ${qtcad_qrc} ${qtcad_hdrs})

  brlcad_addlib(libqtcad "${QTCAD_SRCS}" "" "${QG_DIRS}" SHARED
    PUBLIC_LIBS ${QTCAD_PUBLIC_LIBS}
  )
  set_target_properties(libqtcad PROPERTIES VERSION 20.0.1 SOVERSION 20 AUTOMOC ON)
  if(OPENGL_LIBS)
    set_property(TARGET libqtcad APPEND PROPERTY COMPILE_DEFINITIONS "QT_SYSTEM_OPENGL")
    if(TARGET libqtcad-obj)
      set_property(TARGET libqtcad-obj APPEND PROPERTY COMPILE_DEFINITIONS "QT_SYSTEM_OPENGL")
    endif(TARGET libqtcad-obj)
    if(TARGET libqtcad-brlcad-obj)
      set_property(TARGET libqtcad-brlcad-obj APPEND PROPERTY COMPILE_DEFINITIONS "QT_SYSTEM_OPENGL")
    endif(TARGET libqtcad-brlcad-obj)
  endif(OPENGL_LIBS)

  distclean(qtcad_resources.qrc.depends)
  foreach(qh ${qtcad_hdrs})
    get_filename_component(qhbase ${qh} NAME_WE)
    distclean(${CMAKE_CURRENT_BINARY_DIR}/__/__/include/qtcad/moc_${qhbase}.cpp_parameters)
  endforeach(qh ${qtcad_hdrs})
endif(BRLCAD_ENABLE_QT)

add_subdirectory(tests)

set(
  qtcad_ignore_srcs
  CMakeLists.txt
  README
  ${qtcad_srcs}
  ${qtcad_hdrs}
  bindings.h
  bindings.cpp
  QgCanvasInput.h
  QgCanvasState.h
  qtcad_resources.qrc
  QgGL.cpp
)

cmakefiles(${qtcad_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
