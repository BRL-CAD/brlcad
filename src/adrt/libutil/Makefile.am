# $Id$

lib_LIBRARIES = libtieutil.a
libtieutil_a_SOURCES = camera.c display.c image.c rand.c umath.c
AM_CFLAGS = @SDL_CFLAGS@ -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libtexture

noinst_HEADERS = camera.h display.h font.h

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = image.h umath.h rand.h 

include $(top_srcdir)/misc/Makefile.defs
