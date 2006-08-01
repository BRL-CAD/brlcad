# $Id$

bin_PROGRAMS = g-jack jack-g

g_jack_SOURCES = g-jack.c
jack_g_SOURCES = jack-g.c
jack_g_LDADD = ${RT} ${WDB}


include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(g_jack_OBJECTS) \
	$(jack_g_OBJECTS) \
	$(bin_PROGRAMS)
