# $Id$

lib_LTLIBRARIES = liborle.la

SRCS = rle.c
liborle_la_SOURCES = $(SRCS) vers.c
CFLAGS = -I$(top_srcdir)/h

vers.c: $(SRCS)
	$(top_srcdir)/sh/newvers.sh  liborle_version "University of Utah RLE Library, Version 2"

man_MANS = liborle.3
EXTRA_DIST = $(man_MANS)
