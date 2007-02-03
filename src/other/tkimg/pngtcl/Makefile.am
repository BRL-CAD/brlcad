# $Id$

noinst_LTLIBRARIES = pngtcl.la

pngtcl_la_LDFLAGS = -module
pngtcl_la_SOURCES = \
	pngtcl.c \
	pngtclStubInit.c \
	pngtclStubLib.c

# per-program CPPFLAGS wasn't added until automake 1.7
AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/tkimg/pngtcl \
	-I${top_srcdir}/src/other/tkimg/zlibtcl \
	-DPNGTCL_PACKAGE_NAME=\"pngtcl\" \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0 \
	${PNG_CPPFLAGS} \
	${TCL_CPPFLAGS}

EXTRA_DIST = \
	pkgIndex.tcl.in \
	pngtcl.h \
	pngtclDecls.h \
	pngtclDeclsMask.h \
	pngtclDeclsUnmask.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
