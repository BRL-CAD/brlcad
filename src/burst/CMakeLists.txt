if(HAVE_TERMLIB)
	include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}
		${TERMLIB_INCLUDE_DIR}
		${TCL_INCLUDE_DIRS}
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

	BRLCAD_ADDEXEC(burst "${burst_SOURCES}" "librt libfb libtermio ${TERMLIB_LIBRARY}")

	SET(burst_noinst_HEADERS
		ascii.h 
		burst.h 
		extern.h 
		Hm.h 
		Mm.h 
		Sc.h 
		trie.h
		)
	CMAKEFILES(${burst_noinst_HEADERS} burst.format)
endif(HAVE_TERMLIB)
