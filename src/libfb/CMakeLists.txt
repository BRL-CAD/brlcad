set(FB_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  ${OPENNURBS_INCLUDE_DIR}
  )
BRLCAD_LIB_INCLUDE_DIRS(fb FB_INCLUDE_DIRS "")

set(LINKLIBS "")
if(BRLCAD_ENABLE_X11 AND X11_FOUND)
  set(FBX11DEFS "-DIF_X")
  set(LINKLIBS ${LINKLIBS} ${X11_LIBRARIES})
  set(fbX11_srcs if_X.c if_X24.c)
endif()
if(BRLCAD_ENABLE_OPENGL AND OPENGL_FOUND AND NOT MSVC)
  set(FBOGLDEFS "-DIF_OGL")
  set(LINKLIBS ${LINKLIBS} ${OPENGL_LIBRARIES})
  set(fbogl_srcs if_ogl.c)
endif()
if(WIN32)
  add_definitions(
    -DIF_WGL
    )
  set(LINKLIBS ${LINKLIBS} opengl32.lib ${TCL_TK_LIBRARY})
  set(fbwgl_srcs if_wgl.c)
endif(WIN32)
if(BRLCAD_ENABLE_TK)
  set(FBTKDEFS "-DIF_TK")
  set(LINKLIBS ${LINKLIBS} ${TK_LIBRARY})
  set(fbtk_srcs if_tk.c)
endif(BRLCAD_ENABLE_TK)

set(LIBFB_SOURCES
  ${fbX11_srcs}
  ${fbogl_srcs}
  ${fbwgl_srcs}
  ${fbtk_srcs}
  asize.c
  fb_generic.c
  fb_log.c
  fb_obj.c
  fb_paged_io.c
  fb_rect.c
  fb_util.c
  fbserv_obj.c
  if_debug.c
  if_disk.c
  if_mem.c
  if_null.c
  if_remote.c
  if_stack.c
  tcl.c
  vers.c
  )

# Unfortunately, on Mac OS X the OpenGL headers are not C90
# compliant - anything pulling them in needs the C99 flag
get_property(libfb_C_FLAGS GLOBAL PROPERTY libfb_C_FLAGS)
LIST(APPEND libfb_C_FLAGS "${C99_FLAG}")
set_property(GLOBAL PROPERTY libfb_C_FLAGS "${libfb_C_FLAGS}")

add_definitions(
  -DIF_REMOTE
  -DIF_AB
  ${FBX11DEFS}
  ${FBOGLDEFS}
  ${FBTKDEFS}
  ${XOPEN_DEFINE}
  )

BRLCAD_ADDLIB(libfb "${LIBFB_SOURCES}" "libbu;libpkg;${LINKLIBS}")
SET_TARGET_PROPERTIES(libfb PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
  ADD_DEPENDENCIES(libfb tk)
endif(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)

set(libfb_ignore_files
  dmdfb.h
  if_TEMPLATE.c
  if_X.c
  if_X24.c
  if_ogl.c
  if_wgl.c
  if_tk.c
  )
CMAKEFILES(${libfb_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
