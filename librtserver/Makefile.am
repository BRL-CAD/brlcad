# $Id$

if JAVA
lib_LTLIBRARIES = librtserver.la
else
lib_LTLIBRARIES =
endif

MAJOR=0
MINOR=0
PATCH=0
VERSION = $(MAJOR):$(PATCH):$(MINOR)
 
librtserver_la_SOURCES = rtserver.c

AM_CFLAGS = -I$(top_srcdir)/h @JAVA_CFLAGS@
librtserver_la_LDFLAGS = -version-info $(VERSION)
librtserver_la_LIBADD = @RT@
