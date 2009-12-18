
SUBDIRS = \
	base \
	png \
	zlibtcl \
	pngtcl

# per-program CPPFLAGS wasn't added until automake 1.7
AM_CPPFLAGS = \
	${PNG_CPPFLAGS} \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS}

AM_CFLAGS = ${X_CFLAGS}

tkimg_la_LDFLAGS = -module
lib_LTLIBRARIES = tkimg.la

tkimg_la_SOURCES =
tkimg_la_LIBADD = \
	base/tkimg.lo \
	base/tkimgIO.lo \
	base/tkimgPPB.lo \
	base/tkimgStubInit.lo \
	base/tkimgStubLib.lo \
	base/tkimgUtils.lo \
	png/png.lo \
	pngtcl/pngtcl.lo \
	pngtcl/pngtclStubInit.lo \
	pngtcl/pngtclStubLib.lo \
	zlibtcl/zlibtcl.lo \
	zlibtcl/zlibtclStubInit.lo \
	zlibtcl/zlibtclStubLib.lo \
	${LIBZ} \
	${PNG} \
	${TCL} \
	${TK} \
	${TCLSTUB}

# TCLSTUB is necessary so it will load as a dylib on Mac OS X without
# unresolved stub symbols.

EXTRA_DIST = \
	init.c \
	license.terms

include $(top_srcdir)/misc/Makefile.defs
