# $Id$

lib_LTLIBRARIES = libsysv.la

SRCS = \
	memset.c \
	redata.c \
	regex.c \
	strchr.c \
	strdup.c \
	strsep.c \
	strtok.c \
	tcl.c

libsysv_la_LDFLAGS = -version-info 19:1
libsysv_la_SOURCES = \
	$(SRCS)
	vers.c

EXTRA_DIST = \
	bsdselect.c \
	vers_win.c \
	libsysv.dsp

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh libsysv_version "BRL-CAD System V Compatibility Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
