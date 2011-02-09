libtkpng_la_CFLAGS = \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS} \
	-DDM_TK -DIF_TK \
	${NOWARN}

lib_LTLIBRARIES = libtkpng.la

brlcadscriptdir =  ${libdir}/tkpng${TKPNG_VERSION}
brlcadscript_SCRIPTS = pkgIndex.tcl

libtkpng_la_SOURCES = \
	generic/tkImgPNG.c \
	generic/tkImgPNGInit.c

libtkpng_la_LIBADD = \
	${LIBZ} \
	${TCL} \
	${TK} \
	${TCLSTUB}

DISTCLEANFILES = \
	pkgIndex.tcl

EXTRA_DIST = \
	README \
	ChangeLog \
	license.terms

include $(top_srcdir)/misc/Makefile.defs
