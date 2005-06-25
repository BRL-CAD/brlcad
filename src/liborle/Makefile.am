# $Id$

if ONLY_BENCHMARK

lib_LTLIBRARIES =
man_MANS =
BUILT_SOURCES =

else !ONLY_BENCHMARK

lib_LTLIBRARIES = liborle.la
man_MANS = liborle.3
BUILT_SOURCES = vers.c

liborle_la_SOURCES = $(SRCS) vers.c
liborle_la_LDFLAGS = -version-info 19:1

endif
# end ONLY_BENCHMARK

SRCS = rle.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh  liborle_version "University of Utah RLE Library, Version 2" > $@

EXTRA_DIST = $(man_MANS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

