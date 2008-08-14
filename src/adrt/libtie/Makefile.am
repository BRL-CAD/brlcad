# $Id$

lib_LTLIBRARIES = libtie.la

libtie_la_LIBADD = libtie0.la libtie1.la ${BU_LIBS}
libtie_la_SOURCES =

SOURCES = tie.c tie_kdtree.c
libtie0_la_SOURCES = $(SOURCES)
libtie1_la_SOURCES = $(SOURCES)

libtie0_la_CFLAGS = -DTIE_PRECISION=0
libtie1_la_CFLAGS = -DTIE_PRECISION=1

noinst_LTLIBRARIES = libtie0.la libtie1.la

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
