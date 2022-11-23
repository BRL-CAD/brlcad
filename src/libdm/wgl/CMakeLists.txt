set(WGL_SRCS
  dm-wgl.c
  if_wgl.c
  )

if(WIN32 AND BRLCAD_ENABLE_OPENGL)

  find_package(OpenGL)
  find_package(TCL)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIR_GL}
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
    # Needed for tkWinInt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../other/ext/tk/generic
    ${CMAKE_CURRENT_SOURCE_DIR}/../../other/ext/tk/win
    )

  set_property(SOURCE dm-wgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE if_wgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  add_definitions(-DDM_PLUGIN)

  if (OPENGL_opengl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_opengl_LIBRARY})
  endif (OPENGL_opengl_LIBRARY)
  if (OPENGL_gl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
  endif (OPENGL_gl_LIBRARY)

  dm_plugin_library(dm-wgl SHARED ${WGL_SRCS})
  target_link_libraries(dm-wgl libdmgl libdm libbu ${TCL_LIBRARY} ${TK_LIBRARY} ${OPENGL32_LIBRARY} ${OPENGL_LIBS})
  set_property(TARGET dm-wgl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-wgl "${WGL_SRCS}")

  PLUGIN_SETUP(dm-wgl dm)

endif(WIN32 AND BRLCAD_ENABLE_OPENGL)

CMAKEFILES(
  CMakeLists.txt
  ${WGL_SRCS}
  dm-wgl.h
  fb_wgl.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
