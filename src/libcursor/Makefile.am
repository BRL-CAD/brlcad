# $Id$

lib_LTLIBRARIES = libcursor.la

man_MANS = libcursor.3

libcursor_la_SOURCES = cursor.c

libcursor_la_LDFLAGS = -version-info 19:1
libcursor_la_CFLAGS = ${TERMLIB_CPPFLAGS}

EXTRA_DIST = $(man_MANS)

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
