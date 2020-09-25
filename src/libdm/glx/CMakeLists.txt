set(OGL_SRCS
  dm-ogl.c
  if_ogl.c
  )

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_OPENGL)

  find_package(TCL)
  find_package(X11)
  find_package(GL)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR_GL}
    ${OPENGL_INCLUDE_DIR_GLX}
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
    )

  set_property(SOURCE dm-ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE if_ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  add_definitions(-DDM_PLUGIN)


  dm_plugin_library(dm-ogl SHARED ${OGL_SRCS})
  target_link_libraries(dm-ogl libdm libbu ${X11_LIBRARIES} ${OPENGL_LIBRARIES} ${TCL_LIBRARY} ${TK_LIBRARY})
  set_property(TARGET dm-ogl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-ogl "${OGL_SRCS}")

  PLUGIN_SETUP(dm-ogl dm)

endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_OPENGL)

CMAKEFILES(
  CMakeLists.txt
  ${OGL_SRCS}
  dm-glx.h
  dm-ogl.h
  fb_ogl.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
