# $Id$

lib_LTLIBRARIES = libsysv.la

# XXX this library needs to move into src/other at some point
libsysv_la_LDFLAGS = -version-info 19:1
libsysv_la_SOURCES = \
	getopt.c \
	memset.c \
	redata.c \
	regex.c \
	strchr.c \
	strdup.c \
	strsep.c \
	strtok.c \
	tcl.c \
	vers.c

EXTRA_DIST = \
	bsdselect.c \
	libsysv.dsp

BUILT_SOURCES = vers.c
vers.c:
	@SH@ $(top_srcdir)/sh/vers.sh libsysv_version "BRL-CAD System V Compatibility Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

