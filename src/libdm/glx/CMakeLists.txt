# Include directories needed by libdm users
set(DM_OGL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )
include_directories(${DM_OGL_INCLUDE_DIRS})

find_package(X11)
find_package(GL)

set(OGL_SRCS
  dm-ogl.c
  if_ogl.c
  )

set_property(SOURCE dm-ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
set_property(SOURCE if_ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

add_definitions(-DDM_PLUGIN)
add_definitions(-DDM_OGL -DIF_OGL)

add_library(dm-ogl SHARED ${OGL_SRCS})
target_link_libraries(dm-ogl libdm libbu ${X11_LIBRARIES} ${OPENGL_LIBRARIES})
if (CPP_DLL_DEFINES)
  set_property(TARGET dm-ogl APPEND PROPERTY COMPILE_DEFINITIONS "DM_DLL_EXPORTS")
endif (CPP_DLL_DEFINES)
VALIDATE_STYLE(dm-ogl "${OGL_SRCS}")

PLUGIN_SETUP(dm-ogl dm)

CMAKEFILES(dm-ogl.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
