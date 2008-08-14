# $Id$

lib_LTLIBRARIES = libtienet.la

AM_CFLAGS = ${TCL_CPPFLAGS} -D__STRICT_ANSI__ -I${top_srcdir}/src/adrt/libtie
libtienet_la_SOURCES = tienet_master.c tienet_slave.c tienet_util.c
libtienet_la_LIBADD = ${BU_LIBS}

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = tienet.h tienet_define.h tienet_util.h tienet_master.h tienet_slave.h tienet_util.h

include $(top_srcdir)/misc/Makefile.defs
