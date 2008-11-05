
noinst_LTLIBRARIES = tkimgb.la

AM_CPPFLAGS = \
	-DTKIMG_VERSION="\"${TKIMG_VERSION}\"" \
	-DTKIMG_PACKAGE_NAME=\"Tkimg\" \
	-I${top_srcdir}/src/other/tkimg/base \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS}

AM_CFLAGS = ${X_CFLAGS}

tkimgb_la_LDFLAGS = -module
tkimgb_la_SOURCES = \
	tkimg.c \
	tkimgIO.c \
	tkimgPPB.c \
	tkimgStubInit.c \
	tkimgStubLib.c \
	tkimgUtils.c

EXTRA_DIST = \
	aclocal.m4 \
	configure.in \
	pkgIndex.tcl.in \
	tclconfig \
	tkimg.decls \
	tkimg.h \
	tkimgConfig.sh.in \
	tkimgDecls.h \
	tools

include $(top_srcdir)/misc/Makefile.defs
