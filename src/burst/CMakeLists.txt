include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${TERMLIB_INCLUDE_DIR}
	${TCL_INCLUDE_PATH}
)

SET(burst_SOURCES
	burst.c 
	error.c 
	fb.c 
	glob.c 
	grid.c 
	gridrotate.c 
	Hm.c 
	HmGetc.c 
	HmGlob.c 
	idents.c 
	paint.c 
	plot.c 
	prnt.c 
	Sc.c 
	trie.c 
	ui.c
)

if(CURSES_HAVE_CURSES_H)
   BRLCAD_ADDEXEC(burst "${burst_SOURCES}" "librt libfb libtermio ${TERMLIB_LIBRARY}")
endif(CURSES_HAVE_CURSES_H)


SET(burst_noinst_HEADERS
	ascii.h 
	burst.h 
	extern.h 
	Hm.h 
	Mm.h 
	Sc.h 
	trie.h
)
