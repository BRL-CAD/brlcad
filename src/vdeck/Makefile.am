# $Id$

bin_PROGRAMS = vdeck

vdeck_LDADD = ${LIBRT}
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

