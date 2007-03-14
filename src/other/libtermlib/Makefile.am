# $Id$

lib_LTLIBRARIES = libtermlib.la

AM_CPPFLAGS = -DCM_N -DCM_GT -DCM_B -DCM_D ${TERMLIB_CPPFLAGS}

include_HEADERS = termcap.h
noinst_HEADERS = \
	pathnames.h \
	termcap_private.h

libtermlib_la_LDFLAGS = -version-info 0:6:0
libtermlib_la_SOURCES = \
	termcap.c \
	tgoto.c \
	tputs.c \
	tputws.c

dist_man_MANS = termcap.3
EXTRA_DIST = termcap.src

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

