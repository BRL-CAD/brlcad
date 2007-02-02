# $Id$

lib_LTLIBRARIES = libpng.la

libpng_la_LDFLAGS = -version-info 3:5:2
libpng_la_LIBADD = ${LIBZ}
libpng_la_SOURCES = \
	png.c \
	pngerror.c \
	pnggccrd.c \
	pngget.c \
	pngmem.c \
	pngpread.c \
	pngread.c \
	pngrio.c \
	pngrtran.c \
	pngrutil.c \
	pngset.c \
	pngtrans.c \
	pngvcrd.c \
	pngwio.c \
	pngwrite.c \
	pngwtran.c \
	pngwutil.c

noinst_PROGRAMS = pngtest

pngtest_SOURCES = pngtest.c

pngtest_LDADD = \
	${lib_LTLIBRARIES} \
	${LIBZ} \
	${LIBM}

include_HEADERS = \
	png.h \
	pngconf.h

dist_man_MANS = \
	libpng.3 \
	libpngpf.3 \
	png.5

EXTRA_DIST = \
	ANNOUNCE \
	CHANGES \
	INSTALL \
	KNOWNBUG \
	LICENSE \
	Makefile \
	README \
	TODO \
	Y2KINFO \
	configure \
	contrib \
	example.c \
	libpng.txt \
	pngbar.jpg \
	pngbar.png \
	pngnow.png \
	pngtest.png \
	projects \
	scripts

.PHONY : test
test:	pngtest
	./pngtest ${top_srcdir}/src/other/libpng/pngtest.png

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

