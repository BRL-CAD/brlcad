# $Id$

bin_PROGRAMS = \
	g-off \
	off-g

AM_CPPFLAGS = ${TCL_CPPFLAGS}

g_off_SOURCES = g-off.c
g_off_LDADD = ${RT} ${SYSV}

off_g_SOURCES = off-g.c
off_g_LDADD = ${RT} ${WDB} ${SYSV}

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(g_off_OBJECTS) \
	$(off_g_OBJECTS) \
	$(bin_PROGRAMS)
