# $Id$

lib_LTLIBRARIES = liborle.la

SRCS = rle.c
liborle_la_SOURCES = $(SRCS) vers.c
liborle_la_LDFLAGS = -version-info 19:1
AM_CFLAGS = -I$(top_srcdir)/h

vers.c: $(SRCS)
	sh $(top_srcdir)/sh/vers.sh  liborle_version "University of Utah RLE Library, Version 2" > $@

man_MANS = liborle.3
EXTRA_DIST = $(man_MANS)
