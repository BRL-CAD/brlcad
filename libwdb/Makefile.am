# $Id$
lib_LTLIBRARIES = libwdb.la

SRCS = wdb.c reg.c ars.c arbn.c units.c pipe.c export.c nmg.c id.c strsol.c cline.c bot.c skt.c extr.c mater.c nurb.c
libwdb_la_SOURCES = $(SRCS) vers.c
libwdb_la_LDFLAGS = -version-info 19:1

AM_CFLAGS = -I$(top_srcdir)/h

man_MANS = libwdb.3
EXTRA_DIST = $(man_MANS)

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	$(top_srcdir)/sh/newvers.sh
