# $Id$

bin_PROGRAMS = fbserv
fbserv_SOURCES = fbserv.c
fbserv_LDADD = ${FB}

man_MANS = fbserv.1
EXTRA_DIST = \
	fbserv_win.c \
	$(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(fbserv_OBJECTS) \
	$(bin_PROGRAMS)
