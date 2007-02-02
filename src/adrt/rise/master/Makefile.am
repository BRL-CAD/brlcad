# $Id$

bin_PROGRAMS = rise_master

rise_master_CFLAGS = -D__STRICT_ANSI__ -I.. -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil -I${top_srcdir}/src/adrt/rise

rise_master_LDFLAGS = -static

rise_master_SOURCES = \
	dispatcher.c \
	main.c \
	master.c \
	post.c

rise_master_LDADD = \
	@BU@ \
	${top_builddir}/src/adrt/libcommon/libcommon.la \
	${top_builddir}/src/adrt/librender/librender.la \
	${top_builddir}/src/adrt/libutil/libutil.la \
	${top_builddir}/src/adrt/libtexture/libtexture.la \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${LIBTHREAD} \
	${LIBM} \
	${LIBZ}

noinst_HEADERS = \
	dispatcher.h \
	master.h \
	post.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
