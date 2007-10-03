# $Id$

lib_LTLIBRARIES = libtie.la

libtie_la_SOURCES = \
	tie.c \
	tie_kdtree.c

tieincludedir = $(includedir)/tie
tieinclude_HEADERS = \
	tie.h \
	tie_define.h \
	tie_kdtree.h \
	tie_struct.h

AM_CPPFLAGS = ${TCL_CPPFLAGS}

LINT=splint $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES)
lint:
	for file in $(HEADERS) $(SOURCES); do echo $$file; ${LINT} $$file > $${file}.lint; done

clean-local:
	-rm -f $(HEADERS:.h=.h.lint) $(SOURCES:.c=.c.lint)

include $(top_srcdir)/misc/Makefile.defs
