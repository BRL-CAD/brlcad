set(QTGL_SRCS
  dm-qtgl.cpp
  fb-qtgl.cpp
  qtglwin.cpp
)

if(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL AND OPENGL_TARGETS)

  find_package_qt(REQUIRED)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
  )

  set_property(SOURCE dm-qtgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE fb-qtgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  set(qtgl_linklibs
    libqtcad
    libdmgl
    libdm
    libbu
  )

  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(qtgl_moc_srcs qtglwin.h)
    list(APPEND qtgl_linklibs
      Qt6::Core
      Qt6::Widgets
      Qt6::OpenGL
      Qt6::OpenGLWidgets
      ${OPENGL_TARGETS}
    )
  else()
    qt5_wrap_cpp(qtgl_moc_srcs qtglwin.h)
    list(APPEND qtgl_linklibs
      Qt5::Core
      Qt5::Widgets
      Qt5::OpenGL
      ${OPENGL_TARGETS}
    )
  endif()

  set(QTGL_SRCS ${QTGL_SRCS} ${qtgl_moc_srcs})

  dm_plugin_library(dm-qtgl SHARED ${QTGL_SRCS})
  target_link_libraries(dm-qtgl ${qtgl_linklibs})
  set_property(TARGET dm-qtgl APPEND PROPERTY COMPILE_DEFINITIONS DM_PLUGIN BRLCADBUILD HAVE_CONFIG_H)
  validate_style(dm-qtgl "${QTGL_SRCS}")
  plugin_setup(dm-qtgl dm)

  distclean(moc_qtglwin.cpp_parameters)

endif()

cmakefiles(
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
