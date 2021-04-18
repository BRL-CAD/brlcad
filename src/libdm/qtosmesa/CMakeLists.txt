set(QTOSMESA_SRCS
  dm-qtosmesa.cpp
  fb-qtosmesa.cpp
  ../dm-gl.c
  )

if(BRLCAD_ENABLE_QT)

  find_package(Qt5 COMPONENTS Core Widgets)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${OSMESA_INCLUDE_DIR}
    )

  set_property(SOURCE dm-qtosmesa.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE if_qtosmesa.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  add_definitions(-DDM_PLUGIN)

  # Qt pulls in OpenGL even without QOpenGLWidget, so we need to mangle the names
  # using the Mesa mechanism
  add_definitions(-DUSE_MGL_NAMESPACE)

  dm_plugin_library(dm-qtosmesa SHARED ${QTOSMESA_SRCS})
  target_link_libraries(dm-qtosmesa libdmgl libdm libbu osmesa Qt5::Core Qt5::Widgets)
  set_property(TARGET dm-qtosmesa APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H OSMESA)
  VALIDATE_STYLE(dm-qtosmesa "${QTOSMESA_SRCS}")

  PLUGIN_SETUP(dm-qtosmesa dm)

endif(BRLCAD_ENABLE_QT)

CMAKEFILES(
  CMakeLists.txt
  ${QTOSMESA_SRCS}
  dm-qtosmesa.h
  fb-qtosmesa.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
