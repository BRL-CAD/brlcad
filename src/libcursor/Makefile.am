# $Id$

lib_LTLIBRARIES = libcursor.la

man_MANS = libcursor.3

SRCS =	cursor.c

libcursor_la_SOURCES = \
	$(SRCS) \
	vers.c

libcursor_la_LDFLAGS = -version-info 19:1
libcursor_la_LIBADD = ${TERMLIB}

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh libcursor_version "The BRL-CAD Curses Library" > $@

EXTRA_DIST = $(man_MANS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
