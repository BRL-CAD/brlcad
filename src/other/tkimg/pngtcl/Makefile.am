# $Id$

noinst_LTLIBRARIES = pngtcl.la

# per-program CPPFLAGS wasn't added until automake 1.7
AM_CPPFLAGS = \
	-DPNGTCL_PACKAGE_NAME=\"pngtcl\" \
	-I${top_srcdir}/src/other/tkimg/pngtcl \
	-I${top_srcdir}/src/other/tkimg/zlibtcl \
	${PNG_CPPFLAGS} \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS}

pngtcl_la_LDFLAGS = -module
pngtcl_la_SOURCES = \
	pngtcl.c \
	pngtclStubInit.c \
	pngtclStubLib.c

EXTRA_DIST = \
	pkgIndex.tcl.in \
	pngtcl.h \
	pngtclDecls.h \
	pngtclDeclsMask.h \
	pngtclDeclsUnmask.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
