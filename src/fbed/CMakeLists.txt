IF(HAVE_TERMLIB)
	SET(FBED_INCLUDE_DIRS
		${FB_INCLUDE_DIRS}
		${VFONT_INCLUDE_DIRS}
		${TERMIO_INCLUDE_DIRS}
		${CURSOR_INCLUDE_DIRS}
		${CMAKE_CURRENT_SOURCE_DIR}
		)
	LIST(REMOVE_DUPLICATES FBED_INCLUDE_DIRS)
	include_directories(${FBED_INCLUDE_DIRS})

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
		squash.c
		try.c
		)

	BRLCAD_ADDEXEC(fbed "${fbed_SOURCES}" "libfb libvfont libtermio libcursor ${TERMLIB_LIBRARY}")

	ADD_MAN_PAGE(1 fbed.1)
	CMAKEFILES(ascii.h cursorbits.h extern.h fb_ik.h popup.h std.h try.h)
ENDIF(HAVE_TERMLIB)
CMAKEFILES(Makefile.am)
