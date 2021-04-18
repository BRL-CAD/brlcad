set(SWRAST_SRCS
  dm-swrast.cpp
  fb-swrast.cpp
  ../dm-gl.c
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${OSMESA_INCLUDE_DIR}
  )

set_property(SOURCE dm-swrast.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
set_property(SOURCE if_swrast.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

add_definitions(-DDM_PLUGIN)

# Use the Mesa mechanism for mangling names so we don't collide with any
# system OpenGL libs
add_definitions(-DUSE_MGL_NAMESPACE)

dm_plugin_library(dm-swrast SHARED ${SWRAST_SRCS})
target_link_libraries(dm-swrast libdmgl libdm libbu osmesa)
set_property(TARGET dm-swrast APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H OSMESA)
VALIDATE_STYLE(dm-swrast "${SWRAST_SRCS}")

PLUGIN_SETUP(dm-swrast dm)

CMAKEFILES(
  CMakeLists.txt
  ${SWRAST_SRCS}
  dm-swrast.h
  fb-swrast.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
