set(QTGL_SRCS
  dm-qtgl.cpp
  fb-qtgl.cpp
  )

if(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

  find_package(OpenGL)
  find_package(Qt5 COMPONENTS Core Widgets OpenGL)

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

  dm_plugin_library(dm-qtgl SHARED ${QTGL_SRCS})
  target_link_libraries(dm-qtgl libdmgl libdm libbu Qt5::Core Qt5::Widgets
    Qt5::OpenGL ${OPENGL_LIBS})
  set_property(TARGET dm-qtgl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-qtgl "${QTGL_SRCS}")

  PLUGIN_SETUP(dm-qtgl dm)

endif(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

CMAKEFILES(
  CMakeLists.txt
  ${QTGL_SRCS}
  dm-qtgl.h
  fb-qtgl.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
