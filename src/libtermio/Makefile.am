# $Id$

lib_LTLIBRARIES = libtermio.la

SRCS = termio.c

libtermio_la_SOURCES = \
	$(SRCS) \
	vers.c

libtermio_la_LDFLAGS = -version-info 19:1

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	sh $(top_srcdir)/sh/vers.sh termio_version "The BRL-CAD Terminal Mode Compatibility Library" > $@
