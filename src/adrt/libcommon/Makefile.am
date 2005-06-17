# $Id$

lib_LTLIBRARIES = libcommon.la

if HAVE_BRLCAD
brlcad = g.c
endif

libcommon_la_SOURCES = canim.c cdb.c env.c pack.c unpack.c $(brlcad)

AM_CFLAGS = -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil

noinst_HEADERS = adrt_common.h canim.h cdb.h env.h pack.h unpack.h 
