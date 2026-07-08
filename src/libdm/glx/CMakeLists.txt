set(OGL_SRCS dm-ogl.c if_ogl.c)
set(OGL_PROBE_SRCS dm_ogl_probe.c)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_OPENGL AND OPENGL_TARGETS)

  set(TCL_ENABLE_TK ON)
  find_package_tcl(TCL)
  find_package(X11)

  if(TCL_FOUND AND TK_FOUND)

    # Since ogl uses an X11 OpenGL and that may differ from the overall standard
    # system OpenGL in use, build in the libdmgl logic rather than risking
    # conflicting OpenGL linkages.
    set(OGL_SRCS ${OGL_SRCS} ../dm-gl.c)

    include_directories(
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/..
      ${BRLCAD_BINARY_DIR}/include
      ${BRLCAD_SOURCE_DIR}/include
      ${BU_INCLUDE_DIRS}
      SYSTEM
      ${PNG_INCLUDE_DIRS}
      ${X11_INCLUDE_DIR}
      ${TCL_INCLUDE_PATH}
      ${TK_INCLUDE_PATH}
    )

    set_property(SOURCE dm-ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
    set_property(SOURCE if_ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

    add_definitions(-DDM_PLUGIN)

    if(X11_Xrender_LIB)
      list(APPEND X11_LIBRARIES ${X11_Xrender_LIB})
    endif(X11_Xrender_LIB)
    if(X11_Xi_LIB)
      list(APPEND X11_LIBRARIES ${X11_Xi_LIB})
    endif(X11_Xi_LIB)

    dm_plugin_library(dm-ogl SHARED ${OGL_SRCS})
    target_link_libraries(
      dm-ogl
      libdm
      libbu
      ${X11_LIBRARIES}
      ${TCL_LIBRARY}
      ${TK_LIBRARY}
      PNG::PNG
      ${OPENGL_TARGETS}
    )
    set_property(TARGET dm-ogl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
    set_property(TARGET dm-ogl APPEND PROPERTY COMPILE_DEFINITIONS HAVE_GL_GLX_H)
    validate_style(dm-ogl "${OGL_SRCS}")

    brlcad_addexec(dm_ogl_probe "${OGL_PROBE_SRCS}" "libbu;${X11_LIBRARIES};${OPENGL_TARGETS}")
    set_property(TARGET dm_ogl_probe APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H HAVE_GL_GLX_H)
    validate_style(dm_ogl_probe "${OGL_PROBE_SRCS}")

    plugin_setup(dm-ogl dm)
  endif(TCL_FOUND AND TK_FOUND)
endif()

cmakefiles(
  CMakeLists.txt
  ${OGL_SRCS}
  ${OGL_PROBE_SRCS}
  dm-ogl.h
  fb_ogl.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
