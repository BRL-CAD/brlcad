tkpng_la_CFLAGS = \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS} \
	-DDM_TK -DIF_TK \
	${NOWARN}

tkpng_la_LDFLAGS = -module
lib_LTLIBRARIES = tkpng.la

tkpng_la_SOURCES = \
	tkImgPNG.c \
	tkImgPNGInit.c

tkpng_la_LIBADD = \
	${LIBZ} \
	${TCL} \
	${TK} \
	${TCLSTUB}

EXTRA_DIST = \
	README \
	license.terms

include $(top_srcdir)/misc/Makefile.defs
