# $Id$

SRCS = rle.c

if ONLY_BENCHMARK

lib_LTLIBRARIES =

else !ONLY_BENCHMARK

lib_LTLIBRARIES = liborle.la

liborle_la_SOURCES = $(SRCS) vers.c

liborle_la_LDFLAGS = -version-info 19:1

endif
# end ONLY_BENCHMARK

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh  liborle_version "University of Utah RLE Library, Version 2" > $@

man_MANS = liborle.3

EXTRA_DIST = $(man_MANS)
