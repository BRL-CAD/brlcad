# $Id$

lib_LTLIBRARIES = libtie.la

libtie_la_SOURCES = tie.c

include_HEADERS = tie.h
noinst_HEADERS = define.h struct.h


LINT=splint $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES)
lint:
	for file in $(HEADERS) $(SOURCES); do echo $$file; ${LINT} $$file > $${file}.lint; done

clean-local:
	-rm -f $(HEADERS:.h=.h.lint) $(SOURCES:.c=.c.lint)
