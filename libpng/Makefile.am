# $Id$

lib_LTLIBRARIES = libpng.la

bin_PROGRAMS = ansi2knr pngtest
libpng_la_SOURCES = example.c png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngvcrd.c pngwio.c pngwrite.c pngwtran.c pngwutil.c
ansi2knr_SOURCES = ansi2knr.c
pngtest_SOURCES = pngtest.c
pngtest_LDADD = libpng.la -lz -lm

include_HEADERS = png.h pngconf.h
man_MANS = ansi2knr.1 libpng.3 libpngpf.3 png.5
EXTRA_DIST = $(man_MANS)

CFLAGS = -I$(top_srcdir)/h
libpng_la_LDFLAGS = -version-info 19:1

