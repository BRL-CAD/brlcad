set(SWRAST_SRCS
  dm-swrast.cpp
  fb-swrast.cpp
  swrastwin.cpp
  ../dm-gl.c
  )

if(BRLCAD_ENABLE_QT)

  # Qt is used for the stand-alone FB window, but other than that should not be
  # used by swrast code.
  find_package(Qt5 COMPONENTS Core Widgets)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${OSMESA_INCLUDE_DIR}
    )

  add_definitions(-DDM_PLUGIN)

  # Use the Mesa mechanism for mangling names so we don't collide with any
  # system OpenGL libs
  add_definitions(-DUSE_MGL_NAMESPACE)

  QT5_WRAP_CPP(swrast_moc_srcs swrastwin.h)
  set(SWRAST_SRCS ${SWRAST_SRCS} ${swrast_moc_srcs})

  dm_plugin_library(dm-swrast SHARED ${SWRAST_SRCS})
  target_link_libraries(dm-swrast libqtcad libdm libbu osmesa Qt5::Core Qt5::Widgets)
  set_property(TARGET dm-swrast APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H OSMESA)
  VALIDATE_STYLE(dm-swrast "${SWRAST_SRCS}")

  PLUGIN_SETUP(dm-swrast dm)
endif(BRLCAD_ENABLE_QT)

CMAKEFILES(
  CMakeLists.txt
  ${SWRAST_SRCS}
  dm-swrast.h
  swrastwin.h
  tinygl_testing.patch
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
