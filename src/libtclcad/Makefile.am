# $Id$

lib_LTLIBRARIES = libtclcad.la

SRCS = \
	tclcadAutoPath.c \
	tclcadTkSetup.c \
	tkCanvBezier.c \
	tkImgFmtPIX.c

libtclcad_la_LDFLAGS = -version-info 19:1
libtclcad_la_CFLAGS = \
	${X_CFLAGS} \
	${ITK_CPPFLAGS} \
	${ITCL_CPPFLAGS} \
	${TK_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	-I$(top_srcdir)/src/other/tcl/generic \
	-I$(top_srcdir)/src/other/tcl/unix \
	-I$(top_srcdir)/src/other/tk/generic \
	-I$(top_srcdir)/src/other/tk/unix

libtclcad_la_LIBADD = \
	${FB} \
	${BN} \
	${BU} \
	${TK} \
	${TCLSTUB}

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

DEPS=src/libfb

include $(top_srcdir)/misc/Makefile.defs
