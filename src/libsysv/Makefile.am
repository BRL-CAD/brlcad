# $Id$

lib_LTLIBRARIES = libsysv.la

libsysv_la_SOURCES = \
	memset.c \
	redata.c \
	regex.c \
	strchr.c \
	strdup.c \
	strsep.c \
	strtok.c \
	tcl.c

libsysv_la_CFLAGS = ${TCL_CPPFLAGS}
libsysv_la_LDFLAGS = -version-info 19:1

EXTRA_DIST = \
	bsdselect.c \
	libsysv.dsp

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
