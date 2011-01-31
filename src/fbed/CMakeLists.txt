IF(HAVE_TERMLIB)
	include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}
		${TERMLIB_INCLUDE_DIR}
		${TCL_INCLUDE_DIRS}
		)

	SET(fbed_SOURCES
		char.c 
		empty.c 
		execshell.c 
		fbed.c 
		fill_buf.c 
		getinput.c 
		glob.c 
		pos_pad.c 
		prnt.c 
		sgi_dep.c 
		squash.c 
		try.c
		)

	BRLCAD_ADDEXEC(fbed "${fbed_SOURCES}" "libfb libvfont libtermio libcursor ${TERMLIB_LIBRARY}")

	install(FILES fbed.1 DESTINATION ${MAN_DIR}/man1)
ENDIF(HAVE_TERMLIB)
