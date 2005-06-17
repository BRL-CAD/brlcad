# $Id$

lib_LTLIBRARIES = libcommon.la

if HAVE_BRLCAD
brlcad = g.c
endif

libcommon_la_SOURCES = canim.c cdb.c env.c pack.c unpack.c $(brlcad)

AM_CFLAGS = @BRLCAD_CFLAGS@ -I${top_srcdir}/libtie -I${top_srcdir}/libtienet -I${top_srcdir}/libtexture -I${top_srcdir}/librender -I${top_srcdir}/libutil

noinst_HEADERS = canim.h cdb.h env.h pack.h unpack.h
