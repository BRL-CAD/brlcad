# $Id$

bin_PROGRAMS = rise_slave

rise_slave_CFLAGS = \
	-D__STRICT_ANSI__ \
	-I.. \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/libcommon \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/libutil \
	-I${top_srcdir}/src/adrt/rise \
	${LIBZ_CPPFLAGS}

rise_slave_LDFLAGS = -static

rise_slave_SOURCES = \
	main.c \
	slave.c

rise_slave_LDADD = \
	@BU@ \
	${LIBTHREAD} \
	${top_builddir}/src/adrt/libcommon/libcommon.la \
	${top_builddir}/src/adrt/librender/librender.la \
	${top_builddir}/src/adrt/libutil/libutil.la \
	${top_builddir}/src/adrt/libtexture/libtexture.la \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${LIBM} \
	${LIBZ}

noinst_HEADERS = slave.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
