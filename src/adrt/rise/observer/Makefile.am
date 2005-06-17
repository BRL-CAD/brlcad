# $Id$

bin_PROGRAMS = rise_observer

rise_observer_SOURCES = main.c observer.c

AM_CFLAGS = -D__STRICT_ANSI__ @SDL_CFLAGS@ -I.. -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil -I${top_srcdir}/src/adrt/rise

LDADD = @SDL_LIBS@ ${top_builddir}/src/adrt/libtienet/.libs/libtienet.a ${top_builddir}/src/adrt/libtie/.libs/libtie.a ${top_builddir}/src/adrt/libtexture/.libs/libtexture.a ${top_builddir}/src/adrt/libcommon/.libs/libcommon.a ${top_builddir}/src/adrt/librender/.libs/librender.a ${top_builddir}/src/adrt/libutil/.libs/libutil.a -lpthread -lm @LIBZ@

noinst_HEADERS = observer.h
