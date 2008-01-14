# $Id$

bin_PROGRAMS = isst_master

isst_master_SOURCES = \
	compnet.c \
	dispatcher.c \
	master.c \
	main.c

isst_master_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/librender \
	${LIBZ_CPPFLAGS}

isst_master_LDADD = \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${LIBTHREAD} \
	${LIBM} \
	${LIBZ}

noinst_HEADERS = \
	compnet.h \
	dispatcher.h \
	master.h

include $(top_srcdir)/misc/Makefile.defs
