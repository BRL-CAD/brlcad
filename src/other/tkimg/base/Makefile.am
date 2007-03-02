# $Id$

noinst_LTLIBRARIES = tkimgb.la

AM_CPPFLAGS = \
	-DTKIMG_VERSION="\"${TKIMG_VERSION}\"" \
	-DTKIMG_PACKAGE_NAME=\"Tkimg\" \
	-I${top_srcdir}/src/other/tkimg/base \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS}

tkimgb_la_LDFLAGS = -module
tkimgb_la_SOURCES = \
	tkimg.c \
	tkimgIO.c \
	tkimgPPB.c \
	tkimgStubInit.c \
	tkimgStubLib.c \
	tkimgUtils.c

EXTRA_DIST = \
	tclconfig \
	tkimg.h \
	tkimgDecls.h \
	tools

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
