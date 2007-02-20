# $Id$

bin_PROGRAMS = adrt_bench

adrt_bench_CFLAGS = \
	-D__STRICT_ANSI__ \
	-I.. \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/libutil \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS}

adrt_bench_LDFLAGS = -static

adrt_bench_SOURCES = \
	bench.c \
	main.c

adrt_bench_LDADD = \
	${top_builddir}/src/adrt/libcommon/libcommon.la \
	${top_builddir}/src/adrt/librender/librender.la \
	${top_builddir}/src/adrt/libutil/libutil.la \
	${top_builddir}/src/adrt/libtexture/libtexture.la \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${LIBTHREAD} \
	${LIBM} \
	${LIBZ}

#noinst_HEADERS = \
#	dispatcher.h

noinst_HEADERS = \
	bench.h

MAINTAINERCLEANFILES = Makefile.in

DEPS=src/libbu

include $(top_srcdir)/misc/Makefile.defs
