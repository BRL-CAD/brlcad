# $Id$
lib_LTLIBRARIES = libtermio.la

libtermio_la_SOURCES = termio.c vers.c
libtermio_la_LDFLAGS = -version-info 19:1

AM_CFLAGS = -I$(top_srcdir)/h

BUILT_SOURCES = vers.c
vers.c:
	$(top_srcdir)/sh/newvers.sh
