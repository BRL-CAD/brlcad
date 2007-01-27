# $Id$

lib_LTLIBRARIES = libtermio.la

SRCS = termio.c

libtermio_la_LDFLAGS = -version-info 19:1
libtermio_la_SOURCES = \
	$(SRCS) \
	vers.c

EXTRA_DIST = \
	libtermio.dsp \
	termio_win32.c

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh termio_version "The BRL-CAD Terminal Mode Compatibility Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
