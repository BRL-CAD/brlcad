# $Id$

bin_PROGRAMS = isst_slave

isst_slave_SOURCES = \
	main.c \
	slave.c

isst_slave_LDFLAGS = -static

isst_slave_CFLAGS = \
	-D__STRICT_ANSI__ \
	-I.. \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/libutil \
	-I${top_srcdir}/src/adrt/isst \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS}

isst_slave_LDADD = \
	@BU@ \
	${LIBTHREAD} \
	${top_builddir}/src/adrt/libcommon/libcommon.la \
	${top_builddir}/src/adrt/librender/librender.la \
	${top_builddir}/src/adrt/libutil/libutil.la \
	${LIBM} \
	${top_builddir}/src/adrt/libtexture/libtexture.la \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${LIBZ}

noinst_HEADERS = slave.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
