# $Id$

bin_PROGRAMS = rise_master

rise_master_SOURCES = dispatcher.c main.c master.c post.c

AM_CFLAGS = -D__STRICT_ANSI__ -I.. -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil -I${top_srcdir}/src/adrt/rise

LDADD = -lpthread ${top_builddir}/src/adrt/libcommon/.libs/libcommon.a ${top_builddir}/src/adrt/librender/.libs/librender.a ${top_builddir}/src/adrt/libutil/.libs/libutil.a -lm ${top_builddir}/src/adrt/libtexture/.libs/libtexture.a ${top_builddir}/src/adrt/libtienet/.libs/libtienet.a ${top_builddir}/src/adrt/libtie/.libs/libtie.a ${LIBZ}

noinst_HEADERS = dispatcher.h master.h post.h
