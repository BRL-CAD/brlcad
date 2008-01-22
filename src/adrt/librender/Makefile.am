# $Id$

lib_LTLIBRARIES = librender.la

librender_la_SOURCES = \
	camera.c \
	component.c \
	cut.c \
	depth.c \
	flat.c \
	flos.c \
	grid.c \
	hit.c \
	normal.c \
	path.c \
	phong.c \
	render_util.c \
	spall.c \
	surfel.c

librender_la_LIBADD = \
	${LIBM} \
	$(top_builddir)/src/libbu/libbu.la

AM_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/libutil \
	${TCL_CPPFLAGS}

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = \
	camera.h \
	component.h \
	cut.h \
	depth.h \
	flat.h \
	flos.h \
	grid.h \
	hit.h \
	normal.h \
	path.h \
	phong.h \
	render.h \
	render_internal.h \
	render_util.h \
	spall.h \
	surfel.h

include $(top_srcdir)/misc/Makefile.defs
