set(QTGL_SRCS
  dm-qtgl.cpp
  if_qtgl.cpp
  )

if(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

  find_package(GL)
  find_package(Qt5 COMPONENTS Core Widgets OpenGL)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR_GL}
    ${OPENGL_INCLUDE_DIR_GLX}
    )

  set_property(SOURCE dm-qtgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE if_qtgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  add_definitions(-DDM_PLUGIN)

  dm_plugin_library(dm-qtgl SHARED ${QTGL_SRCS})
  qt5_use_modules(dm-qtgl Core Widgets OpenGL)
  target_link_libraries(dm-qtgl libdm_gl libdm libbu ${OPENGL_LIBRARIES})
  set_property(TARGET dm-qtgl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-qtgl "${QTGL_SRCS}")

  PLUGIN_SETUP(dm-qtgl dm)

endif(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

CMAKEFILES(
  CMakeLists.txt
  ${QTGL_SRCS}
  dm-qtgl.h
  fb_qtgl.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
