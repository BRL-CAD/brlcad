# $Id$

noinst_LTLIBRARIES = tkimgb.la

tkimgb_la_LDFLAGS = -module
tkimgb_la_SOURCES = \
	tkimg.c \
	tkimgIO.c \
	tkimgPPB.c \
	tkimgStubInit.c \
	tkimgStubLib.c \
	tkimgUtils.c

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/tkimg/base \
	-DTKIMG_VERSION="\"${TKIMG_VERSION}\"" \
	-DTKIMG_PACKAGE_NAME=\"Tkimg\" \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0

EXTRA_DIST = \
	tkimg.h \
	tkimgDecls.h
	tclconfig \
	tools

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
