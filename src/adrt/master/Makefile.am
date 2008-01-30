# $Id$

bin_PROGRAMS = adrt_master

adrt_master_SOURCES = \
	compnet.c \
	dispatcher.c \
	master.c \
	main.c

adrt_master_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/librender \
	${LIBZ_CPPFLAGS}

adrt_master_LDADD = \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${LIBTHREAD} \
	${LIBM} \
	${LIBZ}

noinst_HEADERS = \
	compnet.h \
	dispatcher.h \
	master.h

include $(top_srcdir)/misc/Makefile.defs
