# $Id$
lib_LTLIBRARIES = libtermio.la

libtermio_la_SOURCES = termio.c vers.c

CFLAGS = -I$(top_srcdir)/h

BUILT_SOURCES = vers.c
vers.c:
	$(top_srcdir)/sh/newvers.sh
