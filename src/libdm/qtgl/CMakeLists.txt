set(QTGL_SRCS
  dm-qtgl.cpp
  fb-qtgl.cpp
  qtglwin.cpp
  )

if(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    )

  set_property(SOURCE dm-qtgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE fb-qtgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  add_definitions(-DDM_PLUGIN)

  if (OPENGL_opengl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_opengl_LIBRARY})
  endif (OPENGL_opengl_LIBRARY)
  if (OPENGL_gl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
  endif (OPENGL_gl_LIBRARY)

  if (Qt6Widgets_FOUND)
    QT6_WRAP_CPP(qtgl_moc_srcs qtglwin.h)
  else ()
    QT5_WRAP_CPP(qtgl_moc_srcs qtglwin.h)
  endif(Qt6Widgets_FOUND)
  set(QTGL_SRCS ${QTGL_SRCS} ${qtgl_moc_srcs})

  dm_plugin_library(dm-qtgl SHARED ${QTGL_SRCS})
  if (Qt6Widgets_FOUND)
    target_link_libraries(dm-qtgl
      libqtcad libdmgl libdm libbu Qt6::Core Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets ${OPENGL_LIBS})
  else()
    target_link_libraries(dm-qtgl
      libqtcad libdmgl libdm libbu Qt5::Core Qt5::Widgets Qt5::OpenGL ${OPENGL_LIBS})
  endif (Qt6Widgets_FOUND)
  set_property(TARGET dm-qtgl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-qtgl "${QTGL_SRCS}")

  PLUGIN_SETUP(dm-qtgl dm)

  DISTCLEAN(moc_qtglwin.cpp_parameters)

endif(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

CMAKEFILES(
  CMakeLists.txt
  ${QTGL_SRCS}
  dm-qtgl.h
  qtglwin.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
