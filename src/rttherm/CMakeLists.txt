include_directories(
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
)

IF(BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_TK)
	FOREACH(xlib ${X11_LIBRARIES})
		SET(RTTHERM_XLIBS "${RTTHERM_XLIBS} ${xlib}")
	ENDFOREACH(xlib ${X11_LIBRARIES})
	BRLCAD_ADDEXEC(ssampview "spectrum.c ssampview.c" "libfb liboptical ${TCL_TK_LIBRARY} ${RTTHERM_XLIBS}")
ENDIF(BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_TK)

BRLCAD_ADDEXEC(rttherm "spectrum.c viewtherm.c" "libged libfb librttherm libmultispectral liboptical")
SET_TARGET_PROPERTIES(rttherm PROPERTIES COMPILE_DEFINITIONS "RT_MULTISPECTRAL")
INSTALL(PROGRAMS ssampview.tcl DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

BRLCAD_ADDEXEC(ssamp-bw ssamp-bw.c "liboptical")

set(rttherm_MANS
  rttherm.1
  ssamp-bw.1
)
install(FILES ${rttherm_MANS} DESTINATION ${MAN_DIR}/man1)
