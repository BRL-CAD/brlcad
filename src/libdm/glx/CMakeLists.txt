set(OGL_SRCS dm-ogl.c if_ogl.c)
set(OGL_PROBE_SRCS dm_ogl_probe.c)

set(OGL_PROVIDER_TARGETS ${OPENGL_TARGETS})
set(OGL_X11_LIBRARIES ${BRLCAD_X11_LIBRARIES})
if(BRLCAD_X11_PROVIDER_RESOLVED STREQUAL "XMIN" AND TARGET Xmin::GL)
  set(OGL_PROVIDER_TARGETS Xmin::GL)
endif()

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_OPENGL AND OGL_PROVIDER_TARGETS)

  set(TCL_ENABLE_TK ON)
  find_package_tcl(TCL)
  if(BRLCAD_X11_PROVIDER_RESOLVED STREQUAL "SYSTEM")
    find_package(X11)
    if(X11_Xrender_LIB)
      list(APPEND OGL_X11_LIBRARIES ${X11_Xrender_LIB})
    endif()
  endif()

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
      ${BRLCAD_X11_INCLUDE_DIRS}
      ${TCL_INCLUDE_PATH}
      ${TK_INCLUDE_PATH}
    )

    set_property(SOURCE dm-ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
    set_property(SOURCE if_ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

    add_definitions(-DDM_PLUGIN)

    dm_plugin_library(dm-ogl SHARED ${OGL_SRCS})
    target_link_libraries(
      dm-ogl
      libdm
      libbu
      ${OGL_X11_LIBRARIES}
      ${TCL_LIBRARY}
      ${TK_LIBRARY}
      PNG::PNG
      ${OGL_PROVIDER_TARGETS}
    )
    set_property(TARGET dm-ogl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
    set_property(TARGET dm-ogl APPEND PROPERTY COMPILE_DEFINITIONS HAVE_GL_GLX_H)
    validate_style(dm-ogl "${OGL_SRCS}")

    brlcad_addexec(dm_ogl_probe "${OGL_PROBE_SRCS}" "libbu;${OGL_X11_LIBRARIES};${OGL_PROVIDER_TARGETS}")
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
