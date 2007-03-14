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

termcapdir = $(BRLCAD_DATA)/etc
termcap_DATA = termcap

dist_man_MANS = termcap.3

termcap: termcap.src
	cp $(top_srcdir)/src/other/libtermlib/termcap.src termcap

EXTRA_DIST = \
	termcap \
	termcap.src

BUILT_SOURCES = termcap

DISTCLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

