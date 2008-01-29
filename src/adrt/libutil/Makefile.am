# $Id$

lib_LTLIBRARIES = libtieutil.la
libtieutil_la_SOURCES = camera.c image.c rand.c umath.c

AM_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/libtexture \
	${TCL_CPPFLAGS}

libtieutil_la_LIBADD= \
	$(top_builddir)/src/libbu/libbu.la

noinst_HEADERS = camera.h font.h

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = image.h umath.h rand.h 

include $(top_srcdir)/misc/Makefile.defs
