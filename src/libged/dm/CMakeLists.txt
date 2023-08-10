include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(DM_SRCS
  dm.c
  ert.cpp
  screengrab.c
  )

set(DM_LIBS
  libged
  libdm
  libbu
  )

# TODO - check if we can use GL2PS without a system OpenGL install, using only
# the libosmesa backend.  In theory we should be able to capture from swrast
# with GL2PS without anything else, but I don't know if GL2PS is set up to make
# that practical...
if(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_GL2PS)
  find_package(GL2PS)
  if (GL2PS_FOUND)
    add_definitions(-DUSE_GL2PS)
    set(DM_LIBS ${DM_LIBS} GL2PS::GL2PS)
    include_directories(${GL2PS_INCLUDE_DIRS})
  endif (GL2PS_FOUND)
endif(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_GL2PS)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-dm SHARED ${DM_SRCS})
target_link_libraries(ged-dm ${DM_LIBS})
set_property(TARGET ged-dm APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-dm ${DM_SRCS})
PLUGIN_SETUP(ged-dm ged)

CMAKEFILES(
  CMakeLists.txt
  ${DM_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
