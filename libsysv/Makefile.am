# $Id$
lib_LTLIBRARIES = libsysv.la

libsysv_la_SOURCES = regex.c redata.c vers.c
libsysv_la_LDFLAGS = -version-info 19:1

noinst_HEADERS = cclass.h cname.h regex2.h utils.h

CFLAGS = -I$(top_srcdir)/h

BUILT_SOURCES = vers.c
vers.c:
	$(top_srcdir)/sh/newvers.sh
