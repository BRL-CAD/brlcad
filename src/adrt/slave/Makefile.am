# $Id$

bin_PROGRAMS = adrt_slave

adrt_slave_SOURCES = \
	load.c \
	main.c \
	slave.c

adrt_slave_CFLAGS = \
	-I${top_srcdir}/src/adrt \
	-I${top_srcdir}/src/adrt/libtie \
	-I${top_srcdir}/src/adrt/libtienet \
	-I${top_srcdir}/src/adrt/libtexture \
	-I${top_srcdir}/src/adrt/librender \
	-I${top_srcdir}/src/adrt/libutil \
	${LIBZ_CPPFLAGS} \
	${TCL_CPPFLAGS}

#-DHAVE_MYSQL=0 -I/usr/local/include/mysql

adrt_slave_LDADD = \
	${LIBTHREAD} \
	${top_builddir}/src/adrt/librender/librender.la \
	${top_builddir}/src/adrt/libutil/libtieutil.la \
	${LIBM} \
	${top_builddir}/src/adrt/libtexture/libtexture.la \
	${top_builddir}/src/adrt/libtienet/libtienet.la \
	${top_builddir}/src/adrt/libtie/libtie.la \
	${LIBZ}

#-L/usr/local/lib/mysql -lmysqlclient

noinst_HEADERS = load.h slave.h

include $(top_srcdir)/misc/Makefile.defs
