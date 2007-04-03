# $Id$

lib_LIBRARIES = libutil.a
libutil_a_SOURCES = camera.c display.c image.c rand.c umath.c

AM_CFLAGS = @SDL_CFLAGS@ -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libtexture

noinst_HEADERS = camera.h display.h font.h image.h rand.h umath.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
