# $Id$

bin_PROGRAMS = igvt_master

igvt_master_SOURCES = dispatcher.c master.c main.c
igvt_master_LDFLAGS = -static

AM_CFLAGS = @SDL_CFLAGS@ -D__STRICT_ANSI__ -I.. -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil -I${top_srcdir}/src/adrt/igvt

LDADD = @SDL_LIBS@ ${top_builddir}/src/librt/librt.la ${top_builddir}/src/adrt/libcommon/libcommon.la ${top_builddir}/src/adrt/librender/librender.la ${top_builddir}/src/adrt/libutil/libutil.la ${top_builddir}/src/adrt/libtienet/libtienet.la ${top_builddir}/src/adrt/libtie/libtie.la -lpthread -lm @LIBZ@

noinst_HEADERS = dispatcher.h master.h
