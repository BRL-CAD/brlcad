
lib_LTLIBRARIES = libtienet.la

AM_CFLAGS = ${TCL_CPPFLAGS} -D__STRICT_ANSI__ \
	    -I${top_srcdir}/src/adrt/libtie \
	    -I${top_srcdir}/src/adrt/librender \
	    -I${top_srcdir}/src/adrt/libtexture \
	    -I${top_srcdir}/src/adrt

libtienet_la_SOURCES = load.c load_MySQL.c tienet_master.c tienet_slave.c tienet_util.c
libtienet_la_LIBADD = ${BU} ${BU_LIBS} ${top_builddir}/src/adrt/libtie/libtie.la

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = load.h tienet.h tienet_define.h tienet_util.h tienet_master.h tienet_slave.h tienet_util.h

include $(top_srcdir)/misc/Makefile.defs
