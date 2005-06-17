# $Id$

bin_PROGRAMS = igvt_master

igvt_master_SOURCES = dispatcher.c master.c main.c

AM_CFLAGS = @SDL_CFLAGS@ -D__STRICT_ANSI__ -I.. -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil -I${top_srcdir}/src/adrt/igvt

LDADD = @SDL_LIBS@ ${top_builddir}/src/adrt/libcommon/.libs/libcommon.a ${top_builddir}/src/adrt/librender/.libs/librender.a ${top_builddir}/src/adrt/libutil/.libs/libutil.a ${top_builddir}/src/adrt/libtienet/.libs/libtienet.a ${top_builddir}/src/adrt/libtie/.libs/libtie.a -lpthread -lm @LIBZ@

noinst_HEADERS = dispatcher.h master.h
