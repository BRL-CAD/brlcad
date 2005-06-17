# $Id$

bin_PROGRAMS = igvt_observer

igvt_observer_SOURCES = main.c observer.c

AM_CFLAGS = -D__STRICT_ANSI__ @SDL_CFLAGS@ -I.. -I${top_srcdir}/libtie -I${top_srcdir}/libtienet -I${top_srcdir}/libtexture -I${top_srcdir}/libcommon -I${top_srcdir}/librender -I${top_srcdir}/libutil -I${top_srcdir}/igvt

# LDADD = @BRLCAD_LDFLAGS@ @SDL_LIBS@ ${top_builddir}/libtie/.libs/libtie.a ${top_builddir}/libtienet/.libs/libtienet.a ${top_builddir}/libtexture/.libs/libtexture.a ${top_builddir}/libcommon/.libs/libcommon.a ${top_builddir}/librender/.libs/librender.a -lpthread
LDADD = @SDL_LIBS@ ${top_builddir}/libtienet/.libs/libtienet.a ${top_builddir}/libtie/.libs/libtie.a ${top_builddir}/libtexture/.libs/libtexture.a ${top_builddir}/libcommon/.libs/libcommon.a ${top_builddir}/librender/.libs/librender.a ${top_builddir}/libutil/.libs/libutil.a -lpthread -lm

noinst_HEADERS = observer.h
