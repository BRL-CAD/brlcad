# $Id$

noinst_LTLIBRARIES = zlibtcl.la

zlibtcl_la_LDFLAGS = -module
zlibtcl_la_SOURCES = \
	zlibtcl.c \
	zlibtclStubInit.c \
	zlibtclStubLib.c

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/tkimg/zlibtcl \
	-DZLIBTCL_PACKAGE_NAME=\"zlibtcl\" \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS}

EXTRA_DIST = \
	pkgIndex.tcl.in \
	zlibtcl.h \
	zlibtclDecls.h \
	zlibtclDeclsMask.h \
	zlibtclDeclsUnmask.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
