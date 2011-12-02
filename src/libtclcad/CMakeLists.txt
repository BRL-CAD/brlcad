# Include directories needed by libtclcad users
SET(TCLCAD_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${DM_INCLUDE_DIRS}
	${GED_INCLUDE_DIRS}
	)

# We need to guarantee internal Tcl headers
SET(TCLCAD_LOCAL_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/src/other/tcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/tcl/unix
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itk/generic
  ${PNG_INCLUDE_DIR}
)

BRLCAD_INCLUDE_DIRS(TCLCAD_INCLUDE_DIRS)

add_definitions(
	${C99_FLAG}
	${XOPEN_DEFINE}
)

FOREACH(xlib ${X11_LIBRARIES})
	SET(TCLCAD_XLIBS "${TCLCAD_XLIBS} ${xlib}")
ENDFOREACH(xlib ${X11_LIBRARIES})
SET(LIBTCLCAD_SRCS
	tclcad.c
	tclcadAutoPath.c
	tclcad_obj.c
	tkImgFmtPIX.c
        cmdhist_obj.c
	)
BRLCAD_ADDLIB(libtclcad "${LIBTCLCAD_SRCS}" "libged libdm ${TCLCAD_XLIBS} ${X11_Xi_LIB}")
SET_TARGET_PROPERTIES(libtclcad PROPERTIES VERSION 19.0.1 SOVERSION 19)
CMAKEFILES(Makefile.am)
