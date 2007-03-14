# $Id$

lib_LTLIBRARIES = libtermlib.la

AM_CPPFLAGS = -DCM_N -DCM_GT -DCM_B -DCM_D ${TERMLIB_CPPFLAGS}

include_HEADERS = libterm.h

libtermlib_la_SOURCES = \
	termcap.c \
	tgoto.c \
	tputs.c

EXTRA_DIST = termcap.src

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

