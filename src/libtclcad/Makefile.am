# $Id$

lib_LTLIBRARIES = libtclcad.la

AM_CFLAGS = ${X_CFLAGS}

SRCS = \
	tclcadAutoPath.c \
	tclcadTkSetup.c \
	tkCanvBezier.c \
	tkImgFmtPIX.c

libtclcad_la_CFLAGS = -I${top_srcdir}/src/other/libtk/generic
libtclcad_la_LDFLAGS = -version-info 19:1

libtclcad_la_LIBADD = \
	${TK} \
	${BN} \
	${BU} \
	${FB} \
	${TCL}

libtclcad_la_SOURCES = \
	$(SRCS) \
	vers.c

EXTRA_DIST = \
	libtclcad.dsp \
	vers_win.c

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh tclcad_version "BRL-CAD Routines for Tcl/Tk" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

