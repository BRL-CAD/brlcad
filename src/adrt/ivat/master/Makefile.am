# $Id$

bin_PROGRAMS = ivat_master

ivat_master_SOURCES = \
	compnet.c \
	dispatcher.c \
	ivat_python.c \
	master.c \
	main.c

ivat_master_LDFLAGS = -static

ivat_master_CFLAGS = @SDL_CFLAGS@ -D__STRICT_ANSI__ -I.. -I${top_srcdir}/src/adrt/libtie -I${top_srcdir}/src/adrt/libtienet -I${top_srcdir}/src/adrt/libtexture -I${top_srcdir}/src/adrt/libcommon -I${top_srcdir}/src/adrt/librender -I${top_srcdir}/src/adrt/libutil -I${top_srcdir}/src/adrt/ivat -I/usr/local/include/python2.4

ivat_master_LDADD = \
	-lpython2.4 \
	@SDL_LIBS@ \
	${LIBRT} \
	${top_builddir}/src/adrt/libcommon/libcommon.la \
	${top_builddir}/src/adrt/librender/librender.la \
	${top_builddir}/src/adrt/libutil/libutil.la \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${LIBTHREAD} \
	${LIBM} \
	${LIBZ}

noinst_HEADERS = \
	dispatcher.h \
	ivat_python.h \
	master.h

MAINTAINERCLEANFILES = Makefile.am

include $(top_srcdir)/misc/Makefile.defs
