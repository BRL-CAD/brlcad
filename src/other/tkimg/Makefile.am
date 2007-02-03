# $Id$

SUBDIRS = \
	base \
	png \
	zlibtcl \
	pngtcl

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

# per-program CPPFLAGS wasn't added until automake 1.7
AM_CPPFLAGS = \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0 \
	${PNG_CPPFLAGS} \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS}

EXTRA_DIST = \
	init.c

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
