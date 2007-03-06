# $Id$

bin_PROGRAMS = vdeck

AM_CPPFLAGS = ${TCL_CPPFLAGS}

vdeck_LDADD = ${RT} ${SYSV}
vdeck_SOURCES = \
	cgarbs.c \
	match.c \
	parsarg.c \
	vdeck.c

noinst_HEADERS = std.h vextern.h

man_MANS = vdeck.1

EXTRA_DIST = \
	$(man_MANS) \
	vproc.c

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
