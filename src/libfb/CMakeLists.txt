SET(LINKLIBS "")
SET(INCLUDEDIRS "")
if(BRLCAD-ENABLE_X11 AND X11_FOUND)
   SET(FBX11DEFS "-DIF_X")
	FOREACH(xlib ${X11_LIBRARIES})
	   SET(LINKLIBS "${LINKLIBS} ${xlib}")
	ENDFOREACH(xlib ${X11_LIBRARIES})
endif()
if(BRLCAD-ENABLE_OPENGL AND OPENGL_FOUND AND NOT MSVC)
   SET(FBOGLDEFS "-DIF_OGL")
	FOREACH(ogllib ${OPENGL_LIBRARIES})
	   SET(LINKLIBS "${LINKLIBS} ${ogllib}")
	ENDFOREACH(ogllib ${OPENGL_LIBRARIES})
endif()
IF(MSVC)
	add_definitions(
		-DIF_WGL
	)
   SET(LINKLIBS "${LINKLIBS} opengl32.lib ${TCL_TK_LIBRARY}")
ENDIF(MSVC)

set(LIBFB_SOURCES
        asize.c 
        fb_generic.c 
        fb_log.c 
        fb_obj.c 
        fb_paged_io.c 
        fb_rect.c 
        fb_util.c 
        fbserv_obj.c 
        if_X.c 
        if_X24.c 
        if_debug.c 
        if_disk.c 
        if_mem.c 
        if_null.c 
        if_ogl.c 
        if_remote.c 
        if_stack.c 
        if_tk.c 
        if_wgl.c 
        tcl.c 
        vers.c
)

include_directories(
	${X11_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR_GL}
	${OPENGL_INCLUDE_DIR_GLX}
	${TCL_INCLUDE_DIRS}
	${TK_INCLUDE_PATH}
	)

add_definitions(
	-DIF_REMOTE
	-DIF_AB
	${FBX11DEFS}
	${FBOGLDEFS}
)

BRLCAD_ADDLIB(libfb "${LIBFB_SOURCES}" "libbu libpkg ${LINKLIBS}" STRICT)
SET_TARGET_PROPERTIES(libfb PROPERTIES VERSION 19.0.1 SOVERSION 19)
IF(BRLCAD_BUILD_LOCAL_TK AND BRLCAD-ENABLE_TK)
	ADD_DEPENDENCIES(libfb tk)
ENDIF(BRLCAD_BUILD_LOCAL_TK AND BRLCAD-ENABLE_TK)

install(FILES libfb.3 DESTINATION ${MAN_DIR}/man3)
