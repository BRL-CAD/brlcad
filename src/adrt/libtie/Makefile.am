# $Id$

lib_LTLIBRARIES = libtie.la

libtie_la_SOURCES = tie.c kdtree.c
libtie_la_LDFLAGS = -static

include_HEADERS = tie.h
noinst_HEADERS = define.h struct.h kdtree.h

AM_CFLAGS=-Wall -pedantic -std=c99 -pipe

LINT=splint $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES)
lint:
	for file in $(HEADERS) $(SOURCES); do echo $$file; ${LINT} $$file > $${file}.lint; done

clean-local:
	-rm -f $(HEADERS:.h=.h.lint) $(SOURCES:.c=.c.lint)

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
