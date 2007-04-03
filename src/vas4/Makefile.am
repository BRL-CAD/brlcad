# $Id$

bin_PROGRAMS = vas4

AM_CPPFLAGS = ${TCL_CPPFLAGS}

vas4_LDADD = \
	${FB} ${FB_LIBS} \
	${BU} ${BU_LIBS} \
	${PKG} ${PKG_LIBS}
vas4_SOURCES = \
	record_seq.c \
	vas4.c \
	vas_io.c

noinst_HEADERS = vas4.h

man_MANS = vas4.1

EXTRA_DIST = \
	$(man_MANS) \
	init_ik.c \
	status.c \
	vas4.doc

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
