# $Id$

lib_LTLIBRARIES = libtienet.la

AM_CFLAGS = -D__STRICT_ANSI__ -I${top_srcdir}/src/adrt/libtie
libtienet_la_SOURCES = tienet_master.c tienet_slave.c tienet_util.c
libtienet_la_LDFLAGS = -static

include_HEADERS = tienet.h
noinst_HEADERS = tienet_define.h tienet_util.h tienet_master.h tienet_slave.h tienet_util.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
