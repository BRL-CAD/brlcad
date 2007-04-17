# $Id$

lib_LTLIBRARIES = libtclcad.la

libtclcad_la_SOURCES = \
	tclcadAutoPath.c \
	tclcadTkSetup.c \
	tkCanvBezier.c \
	tkImgFmtPIX.c

libtclcad_la_LDFLAGS = -version-info 19:1
libtclcad_la_LIBADD = ${TCLCAD_LIBS}
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

EXTRA_DIST = \
	libtclcad.dsp

DEPS=src/libfb

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
