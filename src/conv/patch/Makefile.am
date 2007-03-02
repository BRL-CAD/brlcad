# $Id$

bin_PROGRAMS = \
	patch-g \
	rpatch

AM_CPPFLAGS = ${TCL_CPPFLAGS}

patch_g_SOURCES = patch-g.c
patch_g_LDADD = ${RT} ${WDB} ${BU} ${SYSV}

rpatch_SOURCES = rpatch.c
rpatch_LDADD = ${BU} ${SYSV}

noinst_HEADERS = patch-g.h

man_MANS = patch-g.1 rpatch.1

EXTRA_DIST = \
	$(man_MANS) \
	pull_comp.sh \
	pull_solidsub.sh \
	rpatch.f

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(patch_g_OBJECTS) \
	$(rpatch_OBJECTS) \
	$(bin_PROGRAMS)
