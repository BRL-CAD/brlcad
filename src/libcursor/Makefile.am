# $Id$

lib_LTLIBRARIES = libcursor.la

SRCS =	cursor.c

libcursor_la_SOURCES = \
	$(SRCS) \
	vers.c

libcursor_la_LDFLAGS = -version-info 19:1
libcursor_la_LIBADD = ${CURSES}

man_MANS = libcursor.3
EXTRA_DIST = $(man_MANS)

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh libcursor_version "The BRL-CAD Curses Library" > $@
