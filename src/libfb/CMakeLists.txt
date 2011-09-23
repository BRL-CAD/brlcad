SET(FB_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	${PKG_INCLUDE_DIRS}
	${X11_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR_GL}
	${OPENGL_INCLUDE_DIR_GLX}
	${TCL_INCLUDE_DIRS}
	${TK_INCLUDE_PATH}
	)
LIST(REMOVE_DUPLICATES FB_INCLUDE_DIRS)
SET(FB_INCLUDE_DIRS ${FB_INCLUDE_DIRS} CACHE STRING "Include directories for libfb" FORCE)
MARK_AS_ADVANCED(FB_INCLUDE_DIRS)
include_directories(${FB_INCLUDE_DIRS})

SET(LINKLIBS "")
if(BRLCAD-ENABLE_X11 AND X11_FOUND)
   SET(FBX11DEFS "-DIF_X")
	FOREACH(xlib ${X11_LIBRARIES})
	   SET(LINKLIBS "${LINKLIBS} ${xlib}")
	ENDFOREACH(xlib ${X11_LIBRARIES})
	SET(fbX11_srcs if_X.c if_X24.c)
endif()
if(BRLCAD-ENABLE_OPENGL AND OPENGL_FOUND AND NOT MSVC)
   SET(FBOGLDEFS "-DIF_OGL")
	FOREACH(ogllib ${OPENGL_LIBRARIES})
	   SET(LINKLIBS "${LINKLIBS} ${ogllib}")
	ENDFOREACH(ogllib ${OPENGL_LIBRARIES})
	SET(fbogl_srcs if_ogl.c)
endif()
IF(WIN32)
	add_definitions(
		-DIF_WGL
	)
   SET(LINKLIBS "${LINKLIBS} opengl32.lib ${TCL_TK_LIBRARY}")
	SET(fbwgl_srcs if_wgl.c)
ENDIF(WIN32)
IF(BRLCAD-ENABLE_TK)
	SET(fbtk_srcs if_tk.c)
ENDIF(BRLCAD-ENABLE_TK)

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

add_definitions(
	-DIF_REMOTE
	-DIF_AB
	${FBX11DEFS}
	${FBOGLDEFS}
	${C99_FLAG}
	${XOPEN_DEFINE}
)

BRLCAD_ADDLIB(libfb "${LIBFB_SOURCES}" "libbu libpkg ${LINKLIBS}")
SET_TARGET_PROPERTIES(libfb PROPERTIES VERSION 19.0.1 SOVERSION 19)
IF(BRLCAD_BUILD_LOCAL_TK AND BRLCAD-ENABLE_TK)
	ADD_DEPENDENCIES(libfb tk)
ENDIF(BRLCAD_BUILD_LOCAL_TK AND BRLCAD-ENABLE_TK)

ADD_MAN_PAGE(3 libfb.3)

SET(libfb_ignore_files
	dmdfb.h
	if_TEMPLATE.c
	nilcursor.h
	oglcursor.h
	if_X.c
	if_X24.c
	if_ogl.c
	if_wgl.c
	if_tk.c
)
CMAKEFILES(${libfb_ignore_files})
CMAKEFILES(Makefile.am)
