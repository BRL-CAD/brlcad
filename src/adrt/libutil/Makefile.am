# $Id$

lib_LTLIBRARIES = libutil.la

libutil_la_SOURCES = camera.c display.c image.c rand.c umath.c

AM_CFLAGS = @SDL_CFLAGS@ -I${top_srcdir}/libtie -I${top_srcdir}/libtienet -I${top_srcdir}/libcommon -I${top_srcdir}/librender -I${top_srcdir}/libtexture

noinst_HEADERS = camera.h display.h image.h rand.h umath.h
