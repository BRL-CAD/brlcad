# $Id$

noinst_LTLIBRARIES = tkimgp.la

tkimgp_la_LDFLAGS = -module
tkimgp_la_SOURCES = \
	png.c

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/tkimg/base \
	-I${top_srcdir}/src/other/tkimg/png \
	-I${top_srcdir}/src/other/tkimg/pngtcl \
	-DPNG_PACKAGE_NAME=\"libpng\" \
	-DPNGTCL_PACKAGE_NAME=\"pngtcl\" \
	-DTKIMG_VERSION="\"${TKIMG_VERSION}\"" \
	-DTKIMG_PNG_PACKAGE_NAME=\"Tkimgpng\" \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0

EXTRA_DIST = \
	init.c \
	pkgIndex.tcl.in \
	tests

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

