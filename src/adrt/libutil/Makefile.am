# $Id$

lib_LIBRARIES = libtieutil.a
libtieutil_a_SOURCES = camera.c display.c image.c rand.c umath.c \
	util_image.c util_math.c util_rand.c
AM_CFLAGS = @SDL_CFLAGS@ -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libtexture

noinst_HEADERS = camera.h display.h font.h image.h rand.h umath.h

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = util_image.h util_math.h util_rand.h

include $(top_srcdir)/misc/Makefile.defs
