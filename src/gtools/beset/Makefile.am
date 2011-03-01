
AM_CPPFLAGS = \
	${STRICT_FLASGS} \
	${TCL_CPPFLAGS}

noinst_PROGRAMS = beset

beset_SOURCES = \
	beset.c \
	fitness.c \
	population.c

noinst_HEADERS = \
	beset.h \
	fitness.h \
	population.h

beset_LDADD = \
	${WDB}

# man_MANS = beset.1

# EXTRA_DIST = $(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(beset_OBJECTS) \
	$(noinst_PROGRAMS)
