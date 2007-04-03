# $Id$

bin_PROGRAMS = comgeom-g

AM_CPPFLAGS = ${TCL_CPPFLAGS}

comgeom_g_SOURCES = \
	cvt.c \
	f2a.c \
	mat.c \
	read.c \
	region.c \
	solid.c \
	tools.c
comgeom_g_LDADD = \
	${WDB} ${WDB_LIBS} \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

noinst_HEADERS = 3d.h ged_types.h

man_MANS = comgeom-g.1

EXTRA_DIST = \
	$(man_MANS) \
	try.sh

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(comgeom_g_OBJECTS) \
	$(bin_PROGRAMS)
