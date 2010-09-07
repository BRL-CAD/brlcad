SET(LINKLIBS "")
SET(INCLUDEDIRS "")
if(BRLCAD-ENABLE_X11 AND X11_FOUND)
   SET(FBX11DEFS "-DIF_X")
	FOREACH(xlib ${X11_LIBRARIES})
	   SET(LINKLIBS "${LINKLIBS} ${xlib}")
	ENDFOREACH(xlib ${X11_LIBRARIES})
   SET(INCLUDEDIRS "${INCLUDEDIRS} ${X11_INCLUDE_DIR}")
endif()
if(BRLCAD-ENABLE_OPENGL AND OPENGL_FOUND)
   SET(FBOGLDEFS "-DIF_OGL")
	FOREACH(ogllib ${OPENGL_LIBRARIES})
	   SET(LINKLIBS "${LINKLIBS} ${ogllib}")
	ENDFOREACH(ogllib ${OPENGL_LIBRARIES})
   SET(INCLUDEDIRS "${INCLUDEDIRS} ${OPENGL_INCLUDE_DIR}")
endif()

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
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${INCLUDEDIRS}
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
)

add_definitions(
 	-DHAVE_CONFIG_H
	-DBRLCADBUILD=1
	-DIF_REMOTE
        -DIF_AB
	${FBX11DEFS}
	${FBOGLDEFS}
)

BRLCAD_ADDLIB(fb "${LIBFB_SOURCES}" "bu pkg ${LINKLIBS}")

install(FILES libfb.3 DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man3)
